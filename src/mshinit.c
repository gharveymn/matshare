#include "headers/mshinit.h"
#include "headers/matlabutils.h"
#include "headers/mshutils.h"
#include "headers/mshtypes.h"
#include "headers/mshsegments.h"

static void msh_InitializeSharedInfo(void);

void msh_InitializeMatshare(void)
{
	
	/* set this every time because matlabutils uses a static variable */
	SetMexErrorCallback(msh_OnError);
	
	if(g_local_info.is_initialized)
	{
		return;
	}
	
	/* lock the file */
	if(!g_local_info.is_mex_locked)
	{
		if(mexIsLocked())
		{
			ReadMexError(__FILE__, __LINE__, "MexLockedError", "Matshare tried to lock its file when it was already locked.");
		}
		mexAtExit(msh_OnExit);
		mexLock();
		g_local_info.is_mex_locked = TRUE;
	}
	
	if(g_local_info.this_pid == 0)
	{
		g_local_info.this_pid = msh_GetPid();
	}

#ifdef MSH_WIN
	if(g_local_info.process_lock == MSH_INVALID_HANDLE)
	{
		if((g_local_info.process_lock = CreateMutex(NULL, FALSE, MSH_LOCK_NAME)) == NULL)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CreateMutexError", "Failed to create the mutex.");
		}
	}
#endif
	
	msh_InitializeSharedInfo();
	
	g_local_info.is_initialized = TRUE;

}


static void msh_InitializeSharedInfo(void)
{

#ifdef MSH_UNIX
	SharedInfoCheck_t new_check = {0}, old_check = {0};
#endif
	
	if(g_local_info.shared_info_wrapper.handle == MSH_INVALID_HANDLE)
	{
		/* note: on both windows and linux, this is guaranteed to be zeroed after creating */
#ifdef MSH_WIN
		g_local_info.shared_info_wrapper.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)sizeof(SharedInfo_t), MSH_SHARED_INFO_SEGMENT_NAME);
		if(g_local_info.shared_info_wrapper.handle == NULL)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CreateSharedInfoError", "Could not create or open the shared info segment.");
		}
#else
		g_local_info.shared_info_wrapper.handle = shm_open(MSH_SHARED_INFO_SEGMENT_NAME, O_RDWR | O_CREAT, MSH_DEFAULT_PERMISSIONS);
		if(g_local_info.shared_info_wrapper.handle == MSH_INVALID_HANDLE)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CreateError", "There was an error creating the shared info segment.");
		}
		
		if(ftruncate(g_local_info.shared_info_wrapper.handle, sizeof(SharedInfo_t)) != 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "TruncateError", "There was an error truncating the shared info segment.");
		}
#endif
	}
	
	if(g_local_info.shared_info_wrapper.ptr == NULL)
	{
		
		g_local_info.has_detached = FALSE;
		
		g_local_info.shared_info_wrapper.ptr = msh_MapMemory(g_local_info.shared_info_wrapper.handle, sizeof(SharedInfo_t));

#ifdef MSH_WIN
		if(msh_AtomicIncrement(&g_shared_info->num_procs) == 1)
		{
			g_shared_info->last_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->first_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->user_defined.sharetype = MSH_SHARETYPE;                                   /** default value **/
			g_shared_info->user_defined.thread_safety.values.is_thread_safe = MSH_THREAD_SAFETY;     /** default value **/
			g_shared_info->user_defined.will_gc = TRUE;		                  				    /** default value **/
			g_shared_info->is_initialized = TRUE;
		}
#else
		do
		{
			old_check.span = g_shared_info->shared_info_check.span;
			new_check.span = old_check.span;
			new_check.values.num_procs += 1;
		} while(msh_AtomicCompareSwap(&g_shared_info->shared_info_check.span, old_check.span, new_check.span) != old_check.span);
		
		if(new_check.values.will_unlink)
		{
			/* close whatever we just opened and get the new shared memory */
			msh_UnmapMemory(g_local_info.shared_info_wrapper.ptr, sizeof(SharedInfo_t));
			g_local_info.shared_info_wrapper.ptr = NULL;
			
			msh_CloseSharedMemory(g_local_info.shared_info_wrapper.handle);
			g_local_info.shared_info_wrapper.handle = MSH_INVALID_HANDLE;
			
			msh_InitializeSharedInfo();
			
			return;
		}
		
		if(old_check.values.num_procs == 0)
		{
			g_shared_info->last_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->first_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->user_defined.security = MSH_DEFAULT_PERMISSIONS;                          /** default value **/
			g_shared_info->user_defined.sharetype = MSH_SHARETYPE;                                   /** default value **/
			g_shared_info->user_defined.thread_safety.values.is_thread_safe = MSH_THREAD_SAFETY;     /** default value **/
			g_shared_info->user_defined.will_gc = TRUE;		                  				    /** default value **/
			g_shared_info->is_initialized = TRUE;
		}
#endif
		
		/* wait until the shared memory is initialized to move on */
		while(!g_shared_info->is_initialized);
		
		msh_LockMemory((void*)g_local_info.shared_info_wrapper.ptr, sizeof(SharedInfo_t));
		
	}
}
