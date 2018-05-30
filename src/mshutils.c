#include "headers/mshutils.h"
#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/matlabutils.h"

static LockCheck_t new_lock_check, old_lock_check;

void msh_OnExit(void)
{

#ifdef MSH_UNIX
	SharedInfoCheck_t new_check, old_check;
#endif
	
	g_local_info.is_initialized = FALSE;
	
	msh_DetachSegmentList(&g_local_seg_list);
	msh_DestroyTable(&g_local_seg_list.seg_table);
	
	if(g_local_info.shared_info_wrapper.ptr != NULL)
	{
	
#ifdef MSH_WIN
		if(!g_local_info.has_detached)
		{
			msh_AtomicDecrement(&g_shared_info->num_procs);
			g_local_info.has_detached = TRUE;
		}
#else
		if(!g_local_info.has_detached)
		{
			do
			{
				old_check.span = g_shared_info->shared_info_check.span;
				new_check.span = old_check.span;
				new_check.values.num_procs -= 1;
				new_check.values.will_unlink = (uint16_T)(new_check.values.num_procs == 0);
			} while(msh_AtomicCompareSwap(&g_shared_info->shared_info_check.span, old_check.span, new_check.span) != old_check.span);
			g_local_info.has_detached = TRUE;
		}
		
		if(new_check.values.will_unlink)
		{
			if(shm_unlink(MSH_SHARED_INFO_SEGMENT_NAME) != 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, errno, "UnlinkError", "There was an error unlinking the shared info segment.");
			}
		}
#endif
		msh_UnlockMemory((void*)g_local_info.shared_info_wrapper.ptr, sizeof(SharedInfo_t));
		msh_UnmapMemory((void*)g_local_info.shared_info_wrapper.ptr, sizeof(SharedInfo_t));
		g_local_info.shared_info_wrapper.ptr = NULL;
	}
	
	if(g_local_info.shared_info_wrapper.handle != MSH_INVALID_HANDLE)
	{
		msh_CloseSharedMemory(g_local_info.shared_info_wrapper.handle);
		g_local_info.shared_info_wrapper.handle = MSH_INVALID_HANDLE;
	}

#ifdef MSH_WIN
	if(g_local_info.process_lock != MSH_INVALID_HANDLE)
	{
		if(CloseHandle(g_local_info.process_lock) == 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CloseHandleError", "Error closing the process lock handle.");
		}
		g_local_info.process_lock = MSH_INVALID_HANDLE;
	}
#endif
	
	if(g_local_info.is_mex_locked)
	{
		if(!mexIsLocked())
		{
			ReadMexError(__FILE__, __LINE__, "MexUnlockedError", "Matshare tried to unlock its file when it was already unlocked.");
		}
		mexUnlock();
		g_local_info.is_mex_locked = FALSE;
	}
	mexAtExit(msh_NullFunction);
	
}


void msh_OnError(void)
{
	SetMexErrorCallback(NULL);

#if MSH_THREAD_SAFETY==TRUE
	/* set the process lock at a level where it can be released if needed */
	if(g_local_info.lock_level > 1)
	{
		g_local_info.lock_level = 1;
	}
	msh_ReleaseProcessLock();
#endif

}


void msh_AcquireProcessLock(void)
{

#if MSH_THREAD_SAFETY==TRUE

#ifdef MSH_WIN
	DWORD status;
#endif
	
	do
	{
		old_lock_check.span = g_shared_info->user_defined.thread_safety.span;
		new_lock_check.span = old_lock_check.span;
		new_lock_check.values.virtual_lock_count += 1;
	} while(msh_AtomicCompareSwap(&g_shared_info->user_defined.thread_safety.span, old_lock_check.span, new_lock_check.span) != old_lock_check.span);
	
	if(g_shared_info->user_defined.thread_safety.values.is_thread_safe)
	{
		if(g_local_info.lock_level == 0)
		{
		
#ifdef MSH_WIN
			status = WaitForSingleObject(g_process_lock, INFINITE);
			if(status == WAIT_ABANDONED)
			{
				ReadMexError(__FILE__, __LINE__, "ProcessLockAbandonedError",  "Another process has failed. Cannot safely continue.");
			}
			else if(status == WAIT_FAILED)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "ProcessLockError",  "Failed to lock acquire the process lock.");
			}
#else
			
			if(lockf(g_process_lock, F_LOCK, sizeof(SharedInfo_t)) != 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, errno, "ProcessLockError",  "Failed to lock acquire the process lock.");
			}
#endif
		}
		
		g_local_info.lock_level += 1;
		
	}
#endif

}


void msh_ReleaseProcessLock(void)
{
#if MSH_THREAD_SAFETY==TRUE
	
	do
	{
		old_lock_check.span = g_shared_info->user_defined.thread_safety.span;
		new_lock_check.span = old_lock_check.span;
		new_lock_check.values.virtual_lock_count -= 1;
	} while(msh_AtomicCompareSwap(&g_shared_info->user_defined.thread_safety.span, old_lock_check.span, new_lock_check.span) != old_lock_check.span);
	
	if(g_local_info.lock_level > 0)
	{
		if(g_local_info.lock_level == 1)
		{
#ifdef MSH_WIN
			if(ReleaseMutex(g_process_lock) == 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "ProcessUnlockError", "Failed to release the process lock.");
			}
#else
			if(lockf(g_process_lock, F_ULOCK, sizeof(SharedInfo_t)) != 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, errno, "ProcessUnlockError", "Failed to release the process lock.");
			}
#endif
		}
		
		g_local_info.lock_level -= 1;
		
	}
#endif

}


msh_directive_t msh_ParseDirective(const mxArray* in)
{
	/* easiest way to deal with implementation defined enum sizes */
	if(mxGetClassID(in) == mxUINT8_CLASS)
	{
		return (msh_directive_t)*((uint8_T*)(mxGetData(in)));
	}
	else
	{
		ReadMexError(__FILE__, __LINE__, "InvalidDirectiveError", "Directive must be type 'uint8'.");
	}
	return msh_DEBUG;
}


void msh_UpdateAll(void)
{
	/* SegmentNode_t* curr_seg_node; */
#ifdef MSH_WIN
	/* only flush the memory when there is more than one process
	 * this appears to only write to pagefile.sys, but it's difficult to find information
	if(FlushViewOfFile(g_shared_info, g_local_info.shm_info_seg.total_seg_sz) == 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "FlushFileError", "Error flushing the update file.");
	}
	
	for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = curr_seg_node->next)
	{
		if(FlushViewOfFile(curr_seg_node->seg_info.shared_memory_ptr, curr_seg_node->seg_info.total_seg_sz) == 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "FlushFileError", "Error flushing the data file.");
		}
	}
	*/
#else
	
	SegmentNode_t* curr_seg_node;
	
	/* I _believe_ that this is unnecessary since I locked the segment to physical memory earlier, so there shouldn't be any caches right?
	if(msync((void*)g_shared_info, g_local_info.shm_info_seg.total_seg_sz, MS_SYNC) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "MsyncError", "There was an error with syncing the shared info segment.");
	}
	 */
	
	for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = curr_seg_node->next)
	{
		/* flushes any possible local caches to the shared memory */
		if(msync(curr_seg_node->seg_info.raw_ptr, curr_seg_node->seg_info.total_segment_size, MS_SYNC | MS_INVALIDATE) != 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "MsyncMatlabcError", "There was an error with syncing a shared data segment.");
		}
	}
#endif
}


size_t PadToAlignData(size_t curr_sz)
{
	return curr_sz + (MXMALLOC_ALIGNMENT_SHIFT - ((curr_sz - 1) & MXMALLOC_ALIGNMENT_SHIFT));
}


void msh_WriteSegmentName(char* name_buffer, msh_segmentnumber_t seg_num)
{
	sprintf(name_buffer, MSH_SEGMENT_NAME, (unsigned long)seg_num);
}


void msh_NullFunction(void)
{
	/* does nothing (so we can reset the mexAtExit function, since NULL is undocumented) */
}


pid_t msh_GetPid(void)
{
#ifdef MSH_WIN
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}


long msh_AtomicIncrement(volatile long* val_ptr)
{
#ifdef MSH_WIN
	return InterlockedIncrement(val_ptr);
#else
	return __sync_add_and_fetch(val_ptr, 1);
#endif
}

long msh_AtomicDecrement(volatile long* val_ptr)
{
#ifdef MSH_WIN
	return InterlockedDecrement(val_ptr);
#else
	return __sync_sub_and_fetch(val_ptr, 1);
#endif
}

long msh_AtomicCompareSwap(volatile long* val_ptr, long compare_value, long swap_value)
{
#ifdef MSH_WIN
	return InterlockedCompareExchange(val_ptr, swap_value, compare_value);
#else
	return __sync_val_compare_and_swap(val_ptr, compare_value, swap_value);
#endif
}
