#include "headers/init.h"


static bool_t is_glob_init;

void init()
{
	
	Header_t hdr;
	
	/* lock the file */
	mexLock();
	
	//assume not
	is_glob_init = FALSE;
	
	/* find out if this has been initialized yet in this process */
	procStartup();
	
	if(!g_info->shm_update_seg.is_init)
	{
		initUpdateSegment();
	}
	
	if(!g_info->shm_update_seg.is_mapped)
	{
		mapUpdateSegment();
	}
	
	/* includes writing to shared memory, but the memory has been aligned so all writes are atomic */
	globStartup(&hdr);

#ifdef MSH_THREAD_SAFE
	if(!g_info->flags.is_proc_lock_init)
	{
		initProcLock();
	}
#endif
	
	/* only actually locks if this is memory safe */
	acquireProcLock();
	
	if(!g_info->flags.is_var_q_init)
	{
		g_info->var_q_front = mxCalloc(1, sizeof(VariableNode_t));
		mexMakeMemoryPersistent(g_info->var_q_front);
		g_info->flags.is_var_q_init = TRUE;
	}
	
	g_info->var_q_front->rev_num = shm_update_info->rev_num;
	g_info->var_q_front->seg_num = shm_update_info->seg_num;
	g_info->var_q_front->data_seg.seg_sz = shm_update_info->seg_sz;
	
	if(!g_info->var_q_front->data_seg.is_init)
	{
		initDataSegment();
	}
	
	if(!g_info->var_q_front->data_seg.is_mapped)
	{
		mapDataSegment();
	}
	
	if(is_glob_init)
	{
		/* initialize shared memory */
		memset(g_info->var_q_front->data_seg.ptr, 0, g_info->var_q_front->data_seg.seg_sz);
		
		/* set the data to mirror the dummy variable at the end */
		memcpy(g_info->var_q_front->data_seg.ptr, &hdr, hdr.obj_sz);
	}
	
	
	if(!g_info->flags.is_glob_shm_var_init)
	{
		
		if(is_glob_init)
		{
			g_info->var_q_front->var = mxCreateDoubleMatrix(0, 0, mxREAL);
		}
		else
		{
			/* fetch the data rather than create a dummy variable */
			shmFetch(g_info->var_q_front->data_seg.ptr, &g_info->var_q_front->var);
		}
		mexMakeArrayPersistent(g_info->var_q_front->var);
		g_info->flags.is_glob_shm_var_init = TRUE;
	}
	else
	{
		readErrorMex("ShmVarInitError", "The globally shared variable was unexpectedly already initialized before startup.");
	}
	
	updateAll();
	releaseProcLock();
	
}


void procStartup(void)
{
	g_info = mxCalloc(1, sizeof(MexInfo_t));
	mexMakeMemoryPersistent(g_info);

#ifdef MSH_WIN
	g_info->this_pid = GetProcessId(GetCurrentProcess());
#else
	g_info->this_pid = getpid();
#endif
	
	
	/* mxCalloc guarantees that these are zeroed
	g_info->flags.is_proc_lock_init = FALSE;
	
	g_info->var_q_front->data_seg = {0};
	g_info->shm_update_seg.is_init = FALSE;
	g_info->shm_update_seg.is_mapped = FALSE;
	
	g_info->var_q_front->data_seg.is_init = FALSE;
	g_info->var_q_front->data_seg.is_mapped = FALSE;
	
	temp_info->startup_flag.is_mapped = FALSE;
	
	g_info->flags.is_glob_shm_var_init = FALSE;
	
	g_info->flags.is_freed = FALSE;
	g_info->flags.is_proc_locked = FALSE;
	*/
	
	g_info->num_lcl_objs = 0; /* make sure this is zero */
	
	mexAtExit(onExit);
	
	
}


void initProcLock(void)
{
#ifdef MSH_THREAD_SAFE

#ifdef MSH_WIN
	
	g_info->lock_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	g_info->lock_sec.lpSecurityDescriptor = NULL;
	g_info->lock_sec.bInheritHandle = TRUE;
	
	g_info->proc_lock = CreateMutex(&g_info->lock_sec, FALSE, MSH_LOCK_NAME);
	if(g_info->proc_lock == NULL)
	{
		readErrorMex("Internal:InitMutexError", "Failed to create the mutex (Error number: %u).", GetLastError());
	}

#else
	
	g_info->proc_lock = shm_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, shm_update_info->security);
	if(g_info->proc_lock == -1)
	{
		readShmOpenError(errno);
	}
	
//	g_info->proc_lock = sem_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, shm_update_info->security, 1);
//	if(g_info->proc_lock == SEM_FAILED)
//	{
//		readSemError(errno);
//	}

#endif
	
	g_info->flags.is_proc_lock_init = TRUE;

#endif

}


void initUpdateSegment(void)
{

	g_info->shm_update_seg.seg_sz = sizeof(ShmSegmentInfo_t);
	
#ifdef MSH_WIN
	
	g_info->shm_update_seg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)g_info->shm_update_seg.seg_sz, MSH_UPDATE_SEGMENT_NAME);
	DWORD err = GetLastError();
	if(g_info->shm_update_seg.handle == NULL)
	{
		releaseProcLock();
		readErrorMex("CreateUpdateSegError", "Could not create or open the update memory segment (Error number: %u).");
	}
	g_info->shm_update_seg.is_init = TRUE;
	
	is_glob_init = (bool_t)(err != ERROR_ALREADY_EXISTS); /* initialize if the segment does not already exist */
	
#else
	
	/* Try to open an already created segment so we can get the global init signal */
	strncpy(g_info->shm_update_seg.name, MSH_UPDATE_SEGMENT_NAME, MSH_MAX_NAME_LEN*sizeof(char));
	g_info->shm_update_seg.handle = shm_open(g_info->shm_update_seg.name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if(g_info->shm_update_seg.handle == -1)
	{
		/* then the segment has already been initialized */
		is_glob_init = FALSE;
		
		if(errno == EEXIST)
		{
			g_info->shm_update_seg.handle = shm_open(g_info->shm_update_seg.name, O_RDWR, S_IRUSR | S_IWUSR);
			if(g_info->shm_update_seg.handle == -1)
			{
				readShmOpenError(errno);
			}
		}
		else
		{
			readShmOpenError(errno);
		}
	}
	else
	{
		is_glob_init = TRUE;
	}
	g_info->shm_update_seg.is_init = TRUE;
	
	if(ftruncate(g_info->shm_update_seg.handle, g_info->shm_update_seg.seg_sz) != 0)
	{
		readFtruncateError(errno);
	}
	

#endif

}

void mapUpdateSegment(void)
{
#ifdef MSH_WIN
	
	g_info->shm_update_seg.ptr = MapViewOfFile(g_info->shm_update_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_info->shm_update_seg.seg_sz);
	DWORD err = GetLastError();
	if(shm_update_info == NULL)
	{
		releaseProcLock();
		readErrorMex("MapUpdateSegError", "Could not map the update memory segment (Error number %u)", err);
	}

#else
	
	/* `shm_update_info` is this but casted to type `ShmSegmentInfo_t` */
	g_info->shm_update_seg.ptr = mmap(NULL, g_info->shm_update_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_info->shm_update_seg.handle, 0);
	if(shm_update_info == MAP_FAILED)
	{
		readMmapError(errno);
	}

#endif
	
	g_info->shm_update_seg.is_mapped = TRUE;
	
}


void globStartup(Header_t* hdr)
{
	if(is_glob_init)
	{
		/* this is the first region created */
		/* this info shouldn't ever actually be used */
		/* but make sure the memory segment is consistent */
		hdr->data_offsets.pr = SIZE_MAX;
		hdr->data_offsets.pi = SIZE_MAX;
		hdr->data_offsets.ir = SIZE_MAX;
		hdr->data_offsets.jc = SIZE_MAX;
		hdr->data_offsets.dims = SIZE_MAX;
		hdr->data_offsets.field_str = SIZE_MAX;
		hdr->data_offsets.child_hdrs = SIZE_MAX;
		
		hdr->is_numeric = TRUE;
		hdr->is_sparse = FALSE;
		hdr->is_empty = TRUE;
		hdr->complexity = mxREAL;
		hdr->classid = mxDOUBLE_CLASS;
		hdr->num_dims = 0;
		hdr->elem_size = sizeof(mxDouble);
		hdr->num_elems = 0;      					/* update this later on sparse*/
		hdr->num_fields = 0;						/* update this later */
		hdr->obj_sz = padToAlign(sizeof(Header_t));     	/* update this later */
		
		shm_update_info->num_procs = 1;
		shm_update_info->lead_seg_num = 0;
		shm_update_info->seg_num = 0;
		shm_update_info->rev_num = 0;
		shm_update_info->seg_sz = hdr->obj_sz;
		shm_update_info->upd_pid = g_info->this_pid;
#ifdef MSH_UNIX
		shm_update_info->security = S_IRUSR | S_IWUSR; /** default value **/
#endif
		shm_update_info->sharetype = msh_SHARETYPE_OVERWRITE;

#ifdef MSH_THREAD_SAFE
		shm_update_info->is_thread_safe = TRUE; 		/** default value **/
#endif
	
	}
	else
	{
		shm_update_info->num_procs += 1;
	}
}


void initDataSegment(void)
{

#ifdef MSH_WIN
	
	snprintf(g_info->var_q_front->data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, (unsigned long long)g_info->var_q_front->seg_num);
	uint32_t lo_sz = (uint32_t)(g_info->var_q_front->data_seg.seg_sz & 0xFFFFFFFFL);
	uint32_t hi_sz = (uint32_t)((g_info->var_q_front->data_seg.seg_sz >> 32) & 0xFFFFFFFFL);
	g_info->var_q_front->data_seg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, g_info->var_q_front->data_seg.name);
	DWORD err = GetLastError();
	if(g_info->var_q_front->data_seg.handle == NULL)
	{
		releaseProcLock();
		readErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u)", err);
	}
	g_info->var_q_front->data_seg.is_init = TRUE;
	
#else
	
	snprintf(g_info->var_q_front->data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, (unsigned long long)0);
	
	g_info->var_q_front->data_seg.handle = shm_open(g_info->var_q_front->data_seg.name, O_RDWR | O_CREAT, shm_update_info->security);
	if(g_info->var_q_front->data_seg.handle == -1)
	{
		readShmOpenError(errno);
	}
	g_info->var_q_front->data_seg.is_init = TRUE;
	
	if(ftruncate(g_info->var_q_front->data_seg.handle, g_info->var_q_front->data_seg.seg_sz) != 0)
	{
		readFtruncateError(errno);
	}
	
#endif

}


void mapDataSegment(void)
{
#ifdef MSH_WIN
	
	g_info->var_q_front->data_seg.ptr = MapViewOfFile(g_info->var_q_front->data_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_info->var_q_front->data_seg.seg_sz);
	if(g_info->var_q_front->data_seg.ptr == NULL)
	{
		releaseProcLock();
		readErrorMex("MapDataSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}
	
#else
	
	g_info->var_q_front->data_seg.ptr = mmap(NULL, g_info->var_q_front->data_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_info->var_q_front->data_seg.handle, 0);
	if(g_info->var_q_front->data_seg.ptr == MAP_FAILED)
	{
		readMmapError(errno);
	}
	
#endif
	
	g_info->var_q_front->data_seg.is_mapped = TRUE;
	
}


void autoInit(mshdirective_t directive)
{
	char init_check_name[MSH_MAX_NAME_LEN] = {0};
#ifdef MSH_WIN
	snprintf(init_check_name, MSH_MAX_NAME_LEN, MSH_INIT_CHECK_NAME, GetProcessId(GetCurrentProcess()));
	HANDLE temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1, init_check_name);
	DWORD err = GetLastError();
	if(temp_handle == NULL)
	{
		readErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
	}
	else if(err != ERROR_ALREADY_EXISTS)
	{
		if(directive == msh_DETACH || directive == msh_OBJ_DEREGISTER)
		{
			if(CloseHandle(temp_handle) == 0)
			{
				readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
			}
			return;
		}
		
		/*then this is the process initializer (and maybe the global initializer; we'll find out later) */
		init();
		g_info->lcl_init_seg.handle = temp_handle;
		memcpy(g_info->lcl_init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->lcl_init_seg.is_init = TRUE;
	}
	else
	{
		if(CloseHandle(temp_handle) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
		}
	}
#else
	snprintf(init_check_name, MSH_MAX_NAME_LEN, MSH_INIT_CHECK_NAME, (unsigned long)(getpid()));
	int temp_handle;
	if((temp_handle = shm_open(init_check_name, O_RDONLY|O_CREAT|O_EXCL, 0))  == -1)
	{
		/* we want this to error if it does exist */
		if(errno != EEXIST)
		{
			readShmOpenError(errno);
		}
	}
	else
	{
		if(directive == msh_DETACH || directive == msh_OBJ_DEREGISTER)
		{
			if(shm_unlink(init_check_name) != 0)
			{
				readShmUnlinkError(errno);
			}
			return;
		}
		
		/*then this is the process initializer (and maybe the global initializer; we'll find out later) */
		init();
		g_info->lcl_init_seg.handle = temp_handle;
		memcpy(g_info->lcl_init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->lcl_init_seg.is_init = TRUE;
	}
#endif
}