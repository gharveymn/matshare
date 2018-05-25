#include "headers/mshinit.h"
#include "headers/matlabutils.h"
#include "headers/mshutils.h"
#include "headers/mshtypes.h"
#include "headers/mshsegments.h"

static bool_t s_is_glob_init;

static void msh_ProcStartup(void);
static void msh_InitProcLock(void);
static void msh_InitInfoSegment(void);
static void msh_MapInfoSegment(void);
static void msh_GlobalStartup(void);

void msh_InitializeMatshare(void)
{
	
	/* set this every time because matlabutils uses a static variable */
	SetMexErrorCallback(msh_OnError);
	
	/* this memory should persist if this is not the first time running */
	if(g_local_info != NULL)
	{
		return;
	}
	
	/* lock the file */
	mexLock();
	
	/* assume not */
	s_is_glob_init = FALSE;
	
	msh_ProcStartup();
	
	if(!g_local_info->flags.is_proc_lock_init)
	{
		msh_InitProcLock();
	}

	/* manually acquire the process lock to because the usual function requires input from the info segment */
#ifdef MSH_WIN
	DWORD ret = WaitForSingleObject(g_local_info->proc_lock, INFINITE);
			if(ret == WAIT_ABANDONED)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "WaitProcLockAbandonedError", "One of the processes failed while using the lock. Cannot safely continue.");
			}
			else if(ret == WAIT_FAILED)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "WaitProcLockFailedError", "The wait for process lock failed.");
			}
#else
	
	if(lockf(g_local_info->proc_lock, F_LOCK, 0) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "WaitProcLockFailedError", "The wait for process lock failed.");
	}
#endif
	g_local_info->flags.proc_lock_level = 1;
	
	if(!g_local_info->shm_info_seg.is_init)
	{
		msh_InitInfoSegment();
	}
	
	if(!g_local_info->shm_info_seg.is_mapped)
	{
		msh_MapInfoSegment();
	}
	
	if(!g_local_info->flags.is_proc_lock_init)
	{
		msh_InitProcLock();
	}
	
	/* includes writing to shared memory, but the memory has been aligned so all writes are atomic */
	msh_GlobalStartup();

	/* manually release the process lock */
#ifdef MSH_WIN
	if(ReleaseMutex(g_local_info->proc_lock) == 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "ReleaseMutexError", "The process lock release failed.");
			}
#else
	if(lockf(g_local_info->proc_lock, F_ULOCK, 0) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "ULockfBadFileError", "The fildes argument is not a valid open file descriptor.");
	}
#endif
	g_local_info->flags.proc_lock_level = 0;

}


static void msh_ProcStartup(void)
{
	g_local_info = mxCalloc(1, sizeof(GlobalInfo_t));
	mexMakeMemoryPersistent(g_local_info);
	
	msh_InitializeTable(&g_local_info->seg_list.seg_table);
	
#ifdef MSH_WIN
	g_local_info->this_pid = GetCurrentProcessId();
#else
	g_local_info->this_pid = getpid();
#endif
	
	mexAtExit(msh_OnExit);
	
}


static void msh_InitProcLock(void)
{
#ifdef MSH_THREAD_SAFE

#ifdef MSH_WIN
	
	g_local_info->lock_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	g_local_info->lock_sec.lpSecurityDescriptor = NULL;
	g_local_info->lock_sec.bInheritHandle = TRUE;
	
	g_local_info->proc_lock = CreateMutex(&g_local_info->lock_sec, FALSE, MSH_LOCK_NAME);
	if(g_local_info->proc_lock == NULL)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "Internal:InitMutexError", "Failed to create the mutex.");
	}

#else
	
	/* set */
	g_local_info->proc_lock = shm_open(MSH_LOCK_NAME, O_WRONLY | O_CREAT, MSH_DEFAULT_PERMISSIONS);
	if(g_local_info->proc_lock == -1)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CreateError", "There was an error opening the process lock.");
	}

#endif
	
	g_local_info->flags.is_proc_lock_init = TRUE;

#endif

}


static void msh_InitInfoSegment(void)
{
	g_local_info->shm_info_seg.seg_sz = sizeof(SharedInfo_t);

#ifdef MSH_WIN
	
	SetLastError(0);
	g_local_info->shm_info_seg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)g_local_info->shm_info_seg.seg_sz, MSH_SHARED_INFO_SEGMENT_NAME);
	if(g_local_info->shm_info_seg.handle == NULL)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CreateUpdateSegError", "Could not create or open the update memory segment.");
	}
	g_local_info->shm_info_seg.is_init = TRUE;
	
	s_is_glob_init = (GetLastError() != ERROR_ALREADY_EXISTS); /* initialize if the segment does not already exist */

#else
	
	/* Try to open an already created segment so we can get the global msh_InitializeMatshare signal */
	g_local_info->shm_info_seg.handle = shm_open(MSH_SHARED_INFO_SEGMENT_NAME, O_RDWR | O_CREAT | O_EXCL, MSH_DEFAULT_PERMISSIONS);
	if(g_local_info->shm_info_seg.handle == -1)
	{
		/* then the segment has already been initialized */
		s_is_glob_init = FALSE;
		
		if(errno == EEXIST)
		{
			g_local_info->shm_info_seg.handle = shm_open(MSH_SHARED_INFO_SEGMENT_NAME, O_RDWR, 0);
			if(g_local_info->shm_info_seg.handle == -1)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, errno, "OpenError", "There was an error opening the shared info segment.");
			}
		}
		else
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CreateError", "There was an error creating the shared info segment.");
		}
	}
	else
	{
		s_is_glob_init = TRUE;
	}
	g_local_info->shm_info_seg.is_init = TRUE;
	
	if(ftruncate(g_local_info->shm_info_seg.handle, g_local_info->shm_info_seg.seg_sz) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "TruncateError", "There was an error truncating the shared info segment.");
	}


#endif

}


static void msh_MapInfoSegment(void)
{
#ifdef MSH_WIN
	
	g_local_info->shm_info_seg.shared_memory_ptr = MapViewOfFile(g_local_info->shm_info_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_local_info->shm_info_seg.seg_sz);
	if(g_shared_info == NULL)
	{
		ReadMexError(__FILE__, __LINE__, "MapUpdateSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}
	
	/* lock this virtual mapping to physical memory to make sure it doesn't get written to the pagefile */
	VirtualLock(g_local_info->shm_info_seg.shared_memory_ptr, g_local_info->shm_info_seg.seg_sz);

#else
	
	/* `g_shared_info` is this but casted to type `SharedInfo_t` */
	g_local_info->shm_info_seg.shared_memory_ptr = mmap(NULL, g_local_info->shm_info_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_local_info->shm_info_seg.handle, 0);
	if(g_shared_info == MAP_FAILED)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "MmapError", "There was an error mapping the shared info segment.");
	}
	
	/* lock the segment to physical memory so it doesn't get paged out */
	mlock(g_local_info->shm_info_seg.shared_memory_ptr, g_local_info->shm_info_seg.seg_sz);

#endif
	
	g_local_info->shm_info_seg.is_mapped = TRUE;
	
}


static void msh_GlobalStartup(void)
{
	if(s_is_glob_init)
	{
		g_shared_info->update_pid = g_local_info->this_pid;
		g_shared_info->num_procs = 1;
		g_shared_info->last_seg_num = -1;
		g_shared_info->first_seg_num = -1;
		g_shared_info->rev_num = 0;
#ifdef MSH_UNIX
		g_shared_info->user_def.security = MSH_DEFAULT_PERMISSIONS; /** default value **/
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
		
		g_shared_info->user_def.will_gc = TRUE;		/** default value **/
	
	}
	else
	{
		msh_AtomicIncrement(&g_shared_info->num_procs);
	}
}
