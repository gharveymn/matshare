#include "headers/mshutils.h"
#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/matlabutils.h"
#include "headers/mshtypes.h"

void msh_OnExit(void)
{
	
	g_local_info.is_initialized = FALSE;
	
	msh_DetachSegmentList(&g_local_seg_list);
	msh_DestroyTable(&g_local_seg_list.seg_table);
	
	if(g_local_info.shared_info_wrapper.ptr != NULL)
	{
	
#ifdef MSH_WIN
		msh_AtomicDecrement(&g_shared_info->num_procs);
#else
		
		/* this will set the unlink flag to TRUE if it hits zero atomically, and only return true if this process did the operation */
		if(msh_DecrementCounter(&g_shared_info->num_procs, TRUE))
		{
			if(shm_unlink(MSH_SHARED_INFO_SEGMENT_NAME) != 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, errno, "UnlinkError", "There was an error unlinking the shared info segment. This is a critical error, please restart.");
			}
			msh_SetCounterPost(&g_shared_info->num_procs, TRUE);
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
	
	msh_IncrementCounter(&g_shared_info->user_defined.lock_counter);
	
	if(msh_GetCounterFlag(&g_shared_info->user_defined.lock_counter))
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
	
	msh_DecrementCounter(&g_shared_info->user_defined.lock_counter, FALSE);
	
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

/* returns the state of the flag after the operation */
LockFreeCounter_t msh_IncrementCounter(LockFreeCounter_t* counter)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.count += 1;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
	return new_counter;
}

/* returns whether the decrement changed the flag to TRUE */
bool_t msh_DecrementCounter(LockFreeCounter_t* counter, bool_t set_flag)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.count -= 1;
		if(set_flag && old_counter.values.flag == FALSE)
		{
			new_counter.values.flag = (unsigned long)(new_counter.values.count == 0);
		}
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
	return (old_counter.values.flag != new_counter.values.flag);
}


void msh_SetCounterFlag(LockFreeCounter_t* counter, unsigned long val)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.flag = val;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
}


void msh_SetCounterPost(LockFreeCounter_t* counter, unsigned long val)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.post = val;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
}

unsigned long msh_GetCounterCount(LockFreeCounter_t* counter)
{
	return counter->values.count;
}

unsigned long msh_GetCounterFlag(LockFreeCounter_t* counter)
{
	return counter->values.flag;
}

unsigned long msh_GetCounterPost(LockFreeCounter_t* counter)
{
	return counter->values.post;
}

void msh_WaitSetCounter(LockFreeCounter_t* counter, unsigned long val)
{
	LockFreeCounter_t old_counter, new_counter;
	old_counter.values.count = 0;
	new_counter.values.count = 0;
	new_counter.values.flag = val;
	do
	{
		old_counter.values.flag = counter->values.flag;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
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
