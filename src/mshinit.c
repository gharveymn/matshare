#include "headers/mshinit.h"

static bool_t s_is_glob_init;

void InitializeMatshare(void)
{
	
	/* set this every time because matlabutils uses a static variable */
	SetMexErrorCallback(MshOnError);
	
	/* this memory should persist if this is not the first time running */
	if(g_local_info != NULL)
	{
		return;
	}
	
	/* lock the file */
	mexLock();
	
	/* assume not */
	s_is_glob_init = FALSE;
	
	ProcStartup();
	
	if(!g_local_info->shm_info_seg.is_init)
	{
		InitInfoSegment();
	}
	
	if(!g_local_info->shm_info_seg.is_mapped)
	{
		MapInfoSegment();
	}
	
	/* includes writing to shared memory, but the memory has been aligned so all writes are atomic */
	GlobalStartup();

#ifdef MSH_THREAD_SAFE
	if(!g_local_info->flags.is_proc_lock_init)
	{
		InitProcLock();
	}
#endif

}


void ProcStartup(void)
{
	g_local_info = mxCalloc(1, sizeof(GlobalInfo_t));
	mexMakeMemoryPersistent(g_local_info);

#ifdef MSH_WIN
	g_local_info->this_pid = GetCurrentProcessId();
#else
	g_local_info->this_pid = getpid();
#endif
	
	mexAtExit(MshOnExit);
	
}


void InitProcLock(void)
{
#ifdef MSH_THREAD_SAFE

#ifdef MSH_WIN
	
	g_local_info->lock_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	g_local_info->lock_sec.lpSecurityDescriptor = NULL;
	g_local_info->lock_sec.bInheritHandle = TRUE;
	
	g_local_info->proc_lock = CreateMutex(&g_local_info->lock_sec, FALSE, MSH_LOCK_NAME);
	if(g_local_info->proc_lock == NULL)
	{
		ReadMexError("Internal:InitMutexError", "Failed to create the mutex (Error number: %u).", GetLastError());
	}

#else
	
	g_local_info->proc_lock = shm_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, g_shared_info->security);
	if(g_local_info->proc_lock == -1)
	{
		ReadShmOpenError(errno);
	}
	
//	g_local_info->proc_lock = sem_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, g_shared_info->security, 1);
//	if(g_local_info->proc_lock == SEM_FAILED)
//	{
//		readSemError(errno);
//	}

#endif
	
	g_local_info->flags.is_proc_lock_init = TRUE;

#endif

}


void InitInfoSegment(void)
{
	
	g_local_info->shm_info_seg.seg_sz = sizeof(SharedInfo_t);

#ifdef MSH_WIN
	
	SetLastError(0);
	g_local_info->shm_info_seg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)g_local_info->shm_info_seg.seg_sz, MSH_UPDATE_SEGMENT_NAME);
	if(g_local_info->shm_info_seg.handle == NULL)
	{
		ReadMexError("CreateUpdateSegError", "Could not create or open the update memory segment (Error number: %u).", GetLastError());
	}
	g_local_info->shm_info_seg.is_init = TRUE;
	
	s_is_glob_init = (GetLastError() != ERROR_ALREADY_EXISTS); /* initialize if the segment does not already exist */

#else
	
	/* Try to open an already created segment so we can get the global InitializeMatshare signal */
	strncpy(g_local_info->shm_info_seg.name, MSH_UPDATE_SEGMENT_NAME, MSH_MAX_NAME_LEN*sizeof(char));
	g_local_info->shm_info_seg.handle = shm_open(g_local_info->shm_info_seg.name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if(g_local_info->shm_info_seg.handle == -1)
	{
		/* then the segment has already been initialized */
		s_is_glob_init = FALSE;
		
		if(errno == EEXIST)
		{
			g_local_info->shm_info_seg.handle = shm_open(g_local_info->shm_info_seg.name, O_RDWR, S_IRUSR | S_IWUSR);
			if(g_local_info->shm_info_seg.handle == -1)
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
		s_is_glob_init = TRUE;
	}
	g_local_info->shm_info_seg.is_init = TRUE;
	
	if(ftruncate(g_local_info->shm_info_seg.handle, g_local_info->shm_info_seg.seg_sz) != 0)
	{
		ReadFtruncateError(errno);
	}


#endif

}


void MapInfoSegment(void)
{
#ifdef MSH_WIN
	
	g_local_info->shm_info_seg.shared_memory_ptr = MapViewOfFile(g_local_info->shm_info_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_local_info->shm_info_seg.seg_sz);
	if(g_shared_info == NULL)
	{
		ReadMexError("MapUpdateSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}
	
	/* lock this virtual mapping to physical memory to make sure it doesn't get written to the pagefile */
	VirtualLock((void*)g_local_info->shm_info_seg.shared_memory_ptr, g_local_info->shm_info_seg.seg_sz);

#else
	
	/* `g_shared_info` is this but casted to type `SharedInfo_t` */
	g_local_info->shm_info_seg.shared_memory_ptr = mmap(NULL, g_local_info->shm_info_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_local_info->shm_info_seg.handle, 0);
	if(g_shared_info == MAP_FAILED)
	{
		ReadMmapError(errno);
	}

#endif
	
	g_local_info->shm_info_seg.is_mapped = TRUE;
	
}


void GlobalStartup(void)
{
	if(s_is_glob_init)
	{
		g_shared_info->num_procs = 1;
		g_shared_info->last_seg_num = -1;
		g_shared_info->first_seg_num = -1;
		g_shared_info->rev_num = 0;
		/* g_shared_info->update_pid = g_local_info->this_pid; */
#ifdef MSH_UNIX
		g_shared_info->security = S_IRUSR | S_IWUSR; /** default value **/
#endif

#ifdef MSH_SHARETYPE_COPY
		g_shared_info->user_def.sharetype = msh_SHARETYPE_COPY;
#else
		g_shared_info->user_def.sharetype = msh_SHARETYPE_OVERWRITE;
#endif

#ifdef MSH_THREAD_SAFE
		g_shared_info->user_def.is_thread_safe = TRUE;          /** default value **/
#else
		g_shared_info->user_def.is_thread_safe = FALSE;
#endif
		
		g_shared_info->user_def.will_remove_unused = TRUE;		/** default value **/
	
	}
	else
	{
		g_shared_info->num_procs += 1;
	}
}