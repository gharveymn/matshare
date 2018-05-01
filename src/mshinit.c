#include "headers/mshinit.h"

static bool_t is_glob_init;

void InitializeMatshare()
{
	
	/* this memory should persist if this is not the first time running */
	if(g_info != NULL)
	{
		return;
	}
	
	/* lock the file */
	mexLock();
	
	//assume not
	is_glob_init = FALSE;
	
	ProcStartup();
	
	if(!g_info->shm_info_seg.is_init)
	{
		InitInfoSegment();
	}
	
	if(!g_info->shm_info_seg.is_mapped)
	{
		MapInfoSegment();
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
	g_info = mxCalloc(1, sizeof(GlobalInfo_t));
	mexMakeMemoryPersistent(g_info);

#ifdef MSH_WIN
	g_info->this_pid = GetProcessId(GetCurrentProcess());
#else
	g_info->this_pid = getpid();
#endif
	
	mexAtExit(MshOnExit);
	SetMexErrorCallback(MshOnError);
	
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
		ReadMexError("Internal:InitMutexError", "Failed to create the mutex (Error number: %u).", GetLastError());
	}

#else
	
	g_info->proc_lock = shm_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, s_info->security);
	if(g_info->proc_lock == -1)
	{
		ReadShmOpenError(errno);
	}
	
//	g_info->proc_lock = sem_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, s_info->security, 1);
//	if(g_info->proc_lock == SEM_FAILED)
//	{
//		readSemError(errno);
//	}

#endif
	
	g_info->flags.is_proc_lock_init = TRUE;

#endif

}


void InitInfoSegment(void)
{
	
	g_info->shm_info_seg.seg_sz = sizeof(s_SharedInfo_t);

#ifdef MSH_WIN
	
	SetLastError(0);
	g_info->shm_info_seg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)g_info->shm_info_seg.seg_sz, MSH_UPDATE_SEGMENT_NAME);
	if(g_info->shm_info_seg.handle == NULL)
	{
		ReadMexError("CreateUpdateSegError", "Could not create or open the update memory segment (Error number: %u).", GetLastError());
	}
	g_info->shm_info_seg.is_init = TRUE;
	
	is_glob_init = (bool_t)(GetLastError() != ERROR_ALREADY_EXISTS); /* initialize if the segment does not already exist */

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


void MapInfoSegment(void)
{
#ifdef MSH_WIN
	
	g_info->shm_info_seg.s_ptr = MapViewOfFile(g_info->shm_info_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_info->shm_info_seg.seg_sz);
	if(s_info == NULL)
	{
		ReadMexError("MapUpdateSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}
	
	/* lock this virtual mapping to physical memory to make sure it doesn't get written to the pagefile */
	VirtualLock(g_info->shm_info_seg.s_ptr, g_info->shm_info_seg.seg_sz);

#else
	
	/* `s_info` is this but casted to type `s_SharedInfo_t` */
	g_info->shm_info_seg.s_ptr = mmap(NULL, g_info->shm_info_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_info->shm_info_seg.handle, 0);
	if(s_info == MAP_FAILED)
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
		s_info->num_procs = 1;
		s_info->last_seg_num = -1;
		s_info->first_seg_num = -1;
		s_info->rev_num = 0;
		/* s_info->update_pid = g_info->this_pid; */
#ifdef MSH_UNIX
		s_info->security = S_IRUSR | S_IWUSR; /** default value **/
#endif

#ifdef MSH_SHARETYPE_COPY
		s_info->user_def.sharetype = msh_SHARETYPE_COPY;
#else
		s_info->user_def.sharetype = msh_SHARETYPE_OVERWRITE;
#endif

#ifdef MSH_THREAD_SAFE
		s_info->user_def.is_thread_safe = TRUE;          /** default value **/
#else
		s_info->user_def.is_thread_safe = FALSE;
#endif
		
		s_info->user_def.will_remove_unused = TRUE;		/** default value **/
	
	}
	else
	{
		s_info->num_procs += 1;
	}
}