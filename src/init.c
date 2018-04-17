#include "headers/init.h"
#include "headers/mshtypes.h"


static bool_t is_glob_init;

void InitializeMatshare()
{
	/* lock the file */
	mexLock();
	
	//assume not
	is_glob_init = FALSE;
	
	/* find out if this has been initialized yet in this process */
	ProcStartup();
	
	if(!g_info->shm_info_seg.is_init)
	{
		InitUpdateSegment();
	}
	
	if(!g_info->shm_info_seg.is_mapped)
	{
		MapUpdateSegment();
	}
	
	/* includes writing to shared memory, but the memory has been aligned so all writes are atomic */
	GlobalStartup();

#ifdef MSH_THREAD_SAFE
	if(!g_info->flags.is_proc_lock_init)
	{
		InitProcLock();
	}
#endif

}


void ProcStartup(void)
{
	g_info = mxCalloc(1, sizeof(LocalInfo_t));
	mexMakeMemoryPersistent(g_info);

#ifdef MSH_WIN
	g_info->this_pid = GetProcessId(GetCurrentProcess());
#else
	g_info->this_pid = getpid();
#endif
	
	mexAtExit(OnExit);
	
}


void InitProcLock(void)
{
#ifdef MSH_THREAD_SAFE

#ifdef MSH_WIN
	
	g_info->lock_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	g_info->lock_sec.lpSecurityDescriptor = NULL;
	g_info->lock_sec.bInheritHandle = TRUE;
	
	g_info->proc_lock = CreateMutex(&g_info->lock_sec, FALSE, MSH_LOCK_NAME);
	if(g_info->proc_lock == NULL)
	{
		ReadErrorMex("Internal:InitMutexError", "Failed to create the mutex (Error number: %u).", GetLastError());
	}

#else
	
	g_info->proc_lock = shm_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, shm_info->security);
	if(g_info->proc_lock == -1)
	{
		ReadShmOpenError(errno);
	}
	
//	g_info->proc_lock = sem_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, shm_info->security, 1);
//	if(g_info->proc_lock == SEM_FAILED)
//	{
//		readSemError(errno);
//	}

#endif
	
	g_info->flags.is_proc_lock_init = TRUE;

#endif

}


void InitUpdateSegment(void)
{

	g_info->shm_info_seg.seg_sz = sizeof(ShmInfo_t);
	
#ifdef MSH_WIN
	
	g_info->shm_info_seg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)g_info->shm_info_seg.seg_sz, MSH_UPDATE_SEGMENT_NAME);
	DWORD err = GetLastError();
	if(g_info->shm_info_seg.handle == NULL)
	{
		ReleaseProcessLock();
		ReadErrorMex("CreateUpdateSegError", "Could not create or open the update memory segment (Error number: %u).");
	}
	g_info->shm_info_seg.is_init = TRUE;
	
	is_glob_init = (bool_t)(err != ERROR_ALREADY_EXISTS); /* initialize if the segment does not already exist */
	
#else
	
	/* Try to open an already created segment so we can get the global InitializeMatshare signal */
	strncpy(g_info->shm_info_seg.name, MSH_UPDATE_SEGMENT_NAME, MSH_MAX_NAME_LEN*sizeof(char));
	g_info->shm_info_seg.handle = shm_open(g_info->shm_info_seg.name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if(g_info->shm_info_seg.handle == -1)
	{
		/* then the segment has already been initialized */
		is_glob_init = FALSE;
		
		if(errno == EEXIST)
		{
			g_info->shm_info_seg.handle = shm_open(g_info->shm_info_seg.name, O_RDWR, S_IRUSR | S_IWUSR);
			if(g_info->shm_info_seg.handle == -1)
			{
				ReadShmOpenError(errno);
			}
		}
		else
		{
			ReadShmOpenError(errno);
		}
	}
	else
	{
		is_glob_init = TRUE;
	}
	g_info->shm_info_seg.is_init = TRUE;
	
	if(ftruncate(g_info->shm_info_seg.handle, g_info->shm_info_seg.seg_sz) != 0)
	{
		ReadFtruncateError(errno);
	}
	

#endif

}

void MapUpdateSegment(void)
{
#ifdef MSH_WIN
	
	g_info->shm_info_seg.ptr = MapViewOfFile(g_info->shm_info_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_info->shm_info_seg.seg_sz);
	DWORD err = GetLastError();
	if(shm_info == NULL)
	{
		ReleaseProcessLock();
		ReadErrorMex("MapUpdateSegError", "Could not map the update memory segment (Error number %u)", err);
	}

#else
	
	/* `shm_info` is this but casted to type `ShmInfo_t` */
	g_info->shm_info_seg.ptr = mmap(NULL, g_info->shm_info_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_info->shm_info_seg.handle, 0);
	if(shm_info == MAP_FAILED)
	{
		ReadMmapError(errno);
	}

#endif
	
	g_info->shm_info_seg.is_mapped = TRUE;
	
}


void GlobalStartup(void)
{
	if(is_glob_init)
	{
		shm_info->num_procs = 1;
		shm_info->lead_seg_num = -1;
		shm_info->first_seg_num = -1;
		shm_info->rev_num = 0;
		shm_info->update_pid = g_info->this_pid;
#ifdef MSH_UNIX
		shm_info->security = S_IRUSR | S_IWUSR; /** default value **/
#endif

#ifdef MSH_SHARETYPE_COPY
		shm_info->sharetype = msh_SHARETYPE_COPY;
#else
		shm_info->sharetype = msh_SHARETYPE_OVERWRITE;
#endif

#ifdef MSH_THREAD_SAFE
		shm_info->is_thread_safe = TRUE; 		/** default value **/
#endif
	
	}
	else
	{
		shm_info->num_procs += 1;
	}
}


void AutoInit(mshdirective_t directive)
{
	char init_check_name[MSH_MAX_NAME_LEN] = {0};
#ifdef MSH_WIN
	snprintf(init_check_name, MSH_MAX_NAME_LEN, MSH_INIT_CHECK_NAME, GetProcessId(GetCurrentProcess()));
	HANDLE temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1, init_check_name);
	DWORD err = GetLastError();
	if(temp_handle == NULL)
	{
		ReadErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
	}
	else if(err != ERROR_ALREADY_EXISTS)
	{
		if(directive == msh_DETACH || directive == msh_OBJ_DEREGISTER)
		{
			if(CloseHandle(temp_handle) == 0)
			{
				ReadErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
			}
			return;
		}
		
		/*then this is the process initializer (and maybe the global initializer; we'll find out later) */
		InitializeMatshare();
		g_info->lcl_init_seg.handle = temp_handle;
		memcpy(g_info->lcl_init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->lcl_init_seg.is_init = TRUE;
	}
	else
	{
		if(CloseHandle(temp_handle) == 0)
		{
			ReadErrorMex("CloseHandleError", "Error closing the InitializeMatshare file handle (Error Number %u)", GetLastError());
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
			ReadShmOpenError(errno);
		}
	}
	else
	{
		if(directive == msh_DETACH || directive == msh_OBJ_DEREGISTER)
		{
			if(shm_unlink(init_check_name) != 0)
			{
				ReadShmUnlinkError(errno);
			}
			return;
		}
		
		/*then this is the process initializer (and maybe the global initializer; we'll find out later) */
		InitializeMatshare();
		g_info->lcl_init_seg.handle = temp_handle;
		memcpy(g_info->lcl_init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->lcl_init_seg.is_init = TRUE;
	}
#endif
}