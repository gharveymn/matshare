#include "headers/init.h"
#include "headers/msh_types.h"


static bool_t is_glob_init;
static bool_t is_proc_init;

void init(bool_t is_mem_safe)
{
	
	header_t hdr;
	
	//assume not
	is_glob_init = FALSE;
	is_proc_init = FALSE;
	
	/* find out if this has been initialized yet in this process */
	procStartup();
	glob_info->flags.is_mem_safe = is_mem_safe;

	if(!glob_info->flags.is_proc_lock_init)
	{
		initProcLock();
	}
	
	if(!glob_info->shm_update_reg.is_init)
	{
		initUpdateSegment();
	}
	
	if(!glob_info->shm_update_reg.is_mapped)
	{
		mapUpdateSegment();
	}
	
	/* only actually locks if this is memory safe */
	acquireProcLock();
	globStartup(&hdr);
	
	if(!glob_info->shm_data_reg.is_init)
	{
		initDataSegment();
	}
	
	if(!glob_info->shm_data_reg.is_mapped)
	{
		mapDataSegment();
	}
	
	if(is_glob_init)
	{
		memcpy(shm_data_ptr, &hdr, hdr.shmsiz);
	}
	
	glob_info->cur_seg_info.rev_num = shm_update_info->rev_num;
	glob_info->cur_seg_info.seg_num = shm_update_info->seg_num;
	glob_info->cur_seg_info.seg_sz = shm_update_info->seg_sz;
	
	glob_info->flags.is_freed = FALSE;
	
	if(is_proc_init)
	{
		if(!glob_info->flags.is_glob_shm_var_init)
		{
			
			if(is_glob_init)
			{
				glob_shm_var = mxCreateDoubleMatrix(0, 0, mxREAL);
			}
			else
			{
				/* fetch the data rather than create a dummy variable */
				shallowfetch(shm_data_ptr, &glob_shm_var);
			}
			mexMakeArrayPersistent(glob_shm_var);
			glob_info->flags.is_glob_shm_var_init = TRUE;
		}
		else
		{
			readMXError("ShmVarInitError", "The globally shared variable was unexpectedly already initialized before startup.");
		}
	}
	else
	{
		if(!glob_info->flags.is_glob_shm_var_init)
		{
			readMXError("ShmVarInitError", "The globally shared variable was unexpectedly not initialized after startup.");
		}
	}
	
	releaseProcLock();
	
}


void procStartup(void)
{
	mex_info* temp_info = mxCalloc(1, sizeof(mex_info));

	/* if MatShare fails here then we have a critical error */
	
	temp_info->startup_flag.reg_sz = MSH_MAX_NAME_LEN;
	
#ifdef MSH_WIN
	
	temp_info->this_pid = GetProcessId(GetCurrentProcess());
	
	sprintf(temp_info->startup_flag.name, MATSHARE_STARTUP_FLAG_PRENAME, temp_info->this_pid);
	temp_info->startup_flag.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MSH_STARTUP_SIG), temp_info->startup_flag.name);
	DWORD err = GetLastError();
	if(temp_info->startup_flag.handle == NULL)
	{
		mxFree(temp_info);
		readMXError("MshStartupFlagError", "Could not create or open the update memory segment (Error number: %u).", err);
	}
	temp_info->startup_flag.is_init = TRUE;
	
	if(err == ERROR_ALREADY_EXISTS)
	{
		/* then this has already been started */
		is_proc_init = FALSE;
	}
	else
	{
		is_proc_init = TRUE;
	}

#else
	
	/* Try to open an already created segment so we can get the proc init signal */
	temp_info->this_pid = getpid();
	
	sprintf(temp_info->startup_flag.name, MATSHARE_STARTUP_FLAG_PRENAME, (unsigned long)temp_info->this_pid);
	temp_info->startup_flag.handle = shm_open(temp_info->startup_flag.name, O_RDWR|O_CREAT|O_EXCL, S_IRWXU);
	if(temp_info->startup_flag.handle == -1)
	{
		/* already exists */
		if(errno != EEXIST)
		{
			readShmError(errno);
		}
		is_proc_init = FALSE;
	}
	else
	{
		is_proc_init = TRUE;
	}
	temp_info->startup_flag.is_init = TRUE;

#endif
	
	if(is_proc_init)
	{
		mexLock();
		
		glob_info = temp_info;
		mexMakeMemoryPersistent(glob_info);
		
		/* mxCalloc guarantees that these are zeroed
		glob_info->flags.is_proc_lock_init = FALSE;
		
		glob_info->shm_data_reg = {0};
		glob_info->shm_update_reg.is_init = FALSE;
		glob_info->shm_update_reg.is_mapped = FALSE;
		
		glob_info->shm_data_reg.is_init = FALSE;
		glob_info->shm_data_reg.is_mapped = FALSE;
		
		temp_info->startup_flag.is_mapped = FALSE;
		
		glob_info->flags.is_glob_shm_var_init = FALSE;
		
		glob_info->flags.is_freed = FALSE;
		glob_info->flags.is_proc_locked = FALSE;
		*/
		
		glob_info->num_lcl_objs_using = 1;
		
		mexAtExit(onExit);
		
	}
	else
	{
#ifdef MSH_WIN
		CloseHandle(temp_info->startup_flag.handle);
#endif
		mxFree(temp_info);
		glob_info->num_lcl_objs_using += 1;
	}
	
}


void initProcLock(void)
{

#ifdef MSH_WIN
	
	glob_info->lock_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	glob_info->lock_sec.lpSecurityDescriptor = NULL;
	glob_info->lock_sec.bInheritHandle = TRUE;
	
	glob_info->proc_lock = CreateMutex(&glob_info->lock_sec, FALSE, MSH_LOCK_NAME);
	if(glob_info->proc_lock == NULL)
	{
		readMXError("Internal:InitMutexError", "Failed to create the mutex (Error number: %u).", GetLastError());
	}

#else
	
	glob_info->proc_lock = sem_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, S_IRWXU, 1);
	if(glob_info->proc_lock == SEM_FAILED)
	{
		readSemError(errno);
	}

#endif
	
	glob_info->flags.is_proc_lock_init = TRUE;

}


void initUpdateSegment(void)
{

	glob_info->shm_update_reg.reg_sz = sizeof(shm_segment_info);
	
#ifdef MSH_WIN
	
	glob_info->shm_update_reg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)glob_info->shm_update_reg.reg_sz, MSH_UPDATE_SEGMENT_NAME);
	DWORD err = GetLastError();
	if(glob_info->shm_update_reg.handle == NULL)
	{
		releaseProcLock();
		readMXError("CreateUpdateSegError", "Could not create or open the update memory segment (Error number: %u).");
	}
	glob_info->shm_update_reg.is_init = TRUE;
	
	is_glob_init = (bool_t)(err != ERROR_ALREADY_EXISTS); /* initialize if the segment does not already exist */
	
#else
	
	/* Try to open an already created segment so we can get the global init signal */
	strcpy(glob_info->shm_update_reg.name, MSH_UPDATE_SEGMENT_NAME);
	glob_info->shm_update_reg.handle = shm_open(glob_info->shm_update_reg.name, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
	if(glob_info->shm_update_reg.handle == -1)
	{
		/* then the segment has already been initialized */
		is_glob_init = FALSE;
		
		if(errno == EEXIST)
		{
			glob_info->shm_update_reg.handle = shm_open(glob_info->shm_update_reg.name, O_RDWR, S_IRWXU);
			if(glob_info->shm_update_reg.handle == -1)
			{
				readShmError(errno);
			}
		}
		else
		{
			readShmError(errno);
		}
	}
	else
	{
		is_glob_init = TRUE;
	}
	glob_info->shm_update_reg.is_init = TRUE;
	
	if(ftruncate(glob_info->shm_update_reg.handle, glob_info->shm_update_reg.reg_sz) != 0)
	{
		readFtruncateError(errno);
	}
	

#endif

}

void mapUpdateSegment(void)
{
#ifdef MSH_WIN
	
	glob_info->shm_update_reg.ptr = MapViewOfFile(glob_info->shm_update_reg.handle, FILE_MAP_ALL_ACCESS, 0, 0, glob_info->shm_update_reg.reg_sz);
	if(shm_update_info == NULL)
	{
		releaseProcLock();
		readMXError("MapUpdateSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}

#else
	
	/* `shm_update_info` is this but casted to type `shm_segment_info` */
	glob_info->shm_update_reg.ptr = mmap(NULL, glob_info->shm_update_reg.reg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, glob_info->shm_update_reg.handle, 0);
	if(shm_update_info == MAP_FAILED)
	{
		readMmapError(errno);
	}

#endif
	
	glob_info->shm_update_reg.is_mapped = TRUE;
	
}


void globStartup(header_t* hdr)
{
	if(is_glob_init)
	{
		
		/* this is the first region created */
		/* this info shouldn't ever actually be used */
		/* but make sure the memory segment is consistent */
		hdr->isNumeric = 1;
		hdr->isSparse = 0;
		hdr->isEmpty = 1;
		hdr->complexity = mxREAL;
		hdr->classid = mxDOUBLE_CLASS;
		hdr->nDims = 2;
		hdr->elemsiz = sizeof(mxDouble);
		hdr->nzmax = 0;      /* update this later on sparse*/
		hdr->nFields = 0;                                 /* update this later */
		hdr->shmsiz = pad_to_align(sizeof(header_t));     /* update this later */
		hdr->strBytes = 0;
		hdr->par_hdr_off = 0;
		
//		shm_update_info->error_flag = FALSE;
		shm_update_info->num_procs = 1;
		shm_update_info->seg_num = 0;
		shm_update_info->rev_num = 0;
		shm_update_info->lead_seg_num = 0;
		shm_update_info->upd_pid = glob_info->this_pid;
		shm_update_info->seg_sz = hdr->shmsiz;
	}
	else
	{
		if(is_proc_init)
		{
			shm_update_info->num_procs += 1;
		}
	}
}


void initDataSegment(void)
{
	
	glob_info->shm_data_reg.reg_sz = shm_update_info->seg_sz;
	sprintf(glob_info->shm_data_reg.name, MSH_SEGMENT_NAME, (unsigned long long)shm_update_info->seg_num);

#ifdef MSH_WIN
	
	uint32_t lo_sz = (uint32_t)(glob_info->shm_data_reg.reg_sz & 0xFFFFFFFFL);
	uint32_t hi_sz = (uint32_t)((glob_info->shm_data_reg.reg_sz >> 32) & 0xFFFFFFFFL);
	glob_info->shm_data_reg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, glob_info->shm_data_reg.name);
	DWORD err = GetLastError();
	if(glob_info->shm_data_reg.handle == NULL)
	{
		releaseProcLock();
		readMXError("CreateFileError", "Error creating the file mapping (Error Number %u)", err);
	}
	glob_info->shm_data_reg.is_init = TRUE;
	
#else
	
	glob_info->shm_data_reg.handle = shm_open(glob_info->shm_data_reg.name, O_RDWR | O_CREAT, S_IRWXU);
	if(glob_info->shm_data_reg.handle == -1)
	{
		readShmError(errno);
	}
	glob_info->shm_data_reg.is_init = TRUE;
	
	if(ftruncate(glob_info->shm_data_reg.handle, shm_update_info->seg_sz) != 0)
	{
		readFtruncateError(errno);
	}
	
#endif

}


void mapDataSegment(void)
{
#ifdef MSH_WIN
	
	glob_info->shm_data_reg.ptr = MapViewOfFile(glob_info->shm_data_reg.handle, FILE_MAP_ALL_ACCESS, 0, 0, glob_info->shm_data_reg.reg_sz);
	if(shm_data_ptr == NULL)
	{
		releaseProcLock();
		readMXError("MapDataSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}
	
#else
	
	glob_info->shm_data_reg.ptr = mmap(NULL, glob_info->shm_data_reg.reg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, glob_info->shm_data_reg.handle, 0);
	if(shm_data_ptr == MAP_FAILED)
	{
		readMmapError(errno);
	}
	
#endif
	
	glob_info->shm_data_reg.is_mapped = TRUE;
	
}