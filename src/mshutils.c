#include "headers/mshutils.h"
#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/matlabutils.h"

void msh_OnExit(void)
{
	
	if(g_local_info->shm_info_seg.is_mapped)
	{
		msh_AtomicDecrement(&g_shared_info->num_procs);
		msh_DetachSegmentList(&g_local_seg_list);
	}

#ifdef MSH_WIN
	
	
	
	if(g_local_info->shm_info_seg.is_mapped)
	{
		/* unlock this set of pages */
		VirtualUnlock(g_local_info->shm_info_seg.shared_memory_ptr, g_local_info->shm_info_seg.seg_sz);
		
		/* unmap the shared info segment */
		if(UnmapViewOfFile(g_local_info->shm_info_seg.shared_memory_ptr) == 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "UnmapFileError", "Error unmapping the update file.");
		}
		g_local_info->shm_info_seg.is_mapped = FALSE;
	}
	
	if(g_local_info->shm_info_seg.is_init)
	{
		if(CloseHandle(g_local_info->shm_info_seg.handle) == 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CloseHandleError", "Error closing the update file handle.");
		}
		g_local_info->shm_info_seg.is_init = FALSE;
	}
	
	if(g_local_info->flags.is_proc_lock_init)
	{
		if(CloseHandle(g_local_info->proc_lock) == 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CloseHandleError", "Error closing the process lock handle.");
		}
		g_local_info->flags.is_proc_lock_init = FALSE;
	}

#else
	
	bool_t will_remove_info = FALSE;
	
	if(g_local_info->shm_info_seg.is_mapped)
	{
		will_remove_info = (g_shared_info->num_procs == 0);
		if(munmap((void*)g_shared_info, g_local_info->shm_info_seg.seg_sz) != 0)
		{
			ReadMunmapError(errno);
		}
		g_local_info->shm_info_seg.is_mapped = FALSE;
	}
	
	if(g_local_info->shm_info_seg.is_init)
	{
		
		if(close(g_local_info->shm_info_seg.handle) == -1)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CloseHandleError", "Error closing the data file handle.");
		}
		
		if(will_remove_info)
		{
			if(shm_unlink(MSH_SHARED_INFO_SEGMENT_NAME) != 0)
			{
				ReadShmUnlinkError(errno);
			}
		}
		g_local_info->shm_info_seg.is_init = FALSE;
	}
	
	if(g_local_info->flags.is_proc_lock_init)
	{
		if(will_remove_info)
		{
			if(shm_unlink(MSH_LOCK_NAME) != 0)
			{
				ReadShmUnlinkError(errno);
			}
		}
		g_local_info->flags.is_proc_lock_init = FALSE;
	}

#endif

	msh_DestroyTable(&g_local_info->seg_list.seg_table);
	
	mxFree(g_local_info);
	g_local_info = NULL;
	mexAtExit(msh_NullFunction);
	
	if(mexIsLocked())
	{
		mexUnlock();
	}
	
}


void msh_OnError(void)
{
	SetMexErrorCallback(NULL);
	if(g_local_info != NULL)
	{
		/* set the process lock at a level where it can be released if needed */
		if(g_local_info->flags.proc_lock_level > 1)
		{
			g_local_info->flags.proc_lock_level = 1;
		}
		msh_ReleaseProcessLock();
	}
}


void msh_AcquireProcessLock(void)
{
#ifdef MSH_THREAD_SAFE
	/* only request a lock if there is more than one process */
	if(g_shared_info->user_def.is_thread_safe)
	{
		if(g_local_info->flags.proc_lock_level == 0)
		{
		
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
		}
		
		g_local_info->flags.proc_lock_level += 1;
		
	}
#endif
}


void msh_ReleaseProcessLock(void)
{
#ifdef MSH_THREAD_SAFE
	if(g_local_info->flags.proc_lock_level > 0)
	{
		if(g_local_info->flags.proc_lock_level == 1)
		{
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
		}
		
		g_local_info->flags.proc_lock_level -= 1;
		
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

#ifdef MSH_WIN
	
	/* only flush the memory when there is more than one process
	 * this appears to only write to pagefile.sys, but it's difficult to find information
	if(g_shared_info->num_procs > 1)
	{
		SegmentNode_t* curr_seg_node = g_local_seg_list.first;
		while(curr_seg_node != NULL)
		{
			if(FlushViewOfFile(g_shared_info, g_local_info->shm_info_seg.seg_sz) == 0)
			{
				__ReadMexErrorWithCode__(__FILE__, __LINE__, GetLastError(), "FlushFileError", "Error flushing the update file.");
			}
			if(FlushViewOfFile(curr_seg_node->seg_info.shared_memory_ptr, curr_seg_node->seg_info.seg_sz) == 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "FlushFileError", "Error flushing the data file.");
			}

			curr_seg_node = curr_seg_node->next;
		}
	}
	*/
#else
	
	/* only flush the memory when there is more than one process */
	if(g_shared_info->num_procs > 1)
	{
		SegmentNode_t* curr_seg_node = g_local_seg_list.first;
		while(curr_seg_node != NULL)
		{
			/* flushes any possible local caches to the shared memory */
			if(msync((void*)g_shared_info, g_local_info->shm_info_seg.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReadMsyncError(errno);
			}
			if(msync(curr_seg_node->seg_info.shared_memory_ptr, curr_seg_node->seg_info.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReadMsyncError(errno);
			}
			curr_seg_node = curr_seg_node->next;
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
