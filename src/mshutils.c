#include "headers/mshutils.h"
#include "headers/mshlists.h"
#include "headers/matlabutils.h"

void msh_OnExit(void)
{
	
	if(g_local_info->flags.is_proc_lock_init)
	{
		msh_AcquireProcessLock();
	}
	
	if(g_local_info->shm_info_seg.is_mapped)
	{
		g_shared_info->num_procs -= 1;
		msh_UpdateSegmentTracking();
		msh_DetachSegmentList(&g_local_seg_list);
	}

#ifdef MSH_WIN
	
	
	
	if(g_local_info->shm_info_seg.is_mapped)
	{
		/* unlock this set of pages */
		VirtualUnlock((void*)g_local_info->shm_info_seg.shared_memory_ptr, g_local_info->shm_info_seg.seg_sz);
		
		/* unmap the shared info segment */
		if(UnmapViewOfFile((void*)g_local_info->shm_info_seg.shared_memory_ptr) == 0)
		{
			ReadMexErrorWithCode(GetLastError(), "UnmapFileError", "Error unmapping the update file.");
		}
		g_local_info->shm_info_seg.is_mapped = FALSE;
	}
	
	if(g_local_info->shm_info_seg.is_init)
	{
		if(CloseHandle(g_local_info->shm_info_seg.handle) == 0)
		{
			ReadMexErrorWithCode(GetLastError(), "CloseHandleError", "Error closing the update file handle.");
		}
		g_local_info->shm_info_seg.is_init = FALSE;
	}
	
	if(g_local_info->flags.is_proc_lock_init)
	{
		msh_ReleaseProcessLock();
		if(CloseHandle(g_local_info->proc_lock) == 0)
		{
			ReadMexErrorWithCode(GetLastError(), "CloseHandleError", "Error closing the process lock handle.");
		}
		g_local_info->flags.is_proc_lock_init = FALSE;
	}

#else
	
	bool_t will_remove_info = FALSE;
	
	if(g_local_info->shm_info_seg.is_mapped)
	{
		will_remove_info = (g_shared_info->num_procs == 0);
		if(munmap(g_shared_info, g_local_info->shm_info_seg.seg_sz) != 0)
		{
			if(g_local_info->flags.is_proc_lock_init)
			{
				msh_ReleaseProcessLock();
			}
			ReadMunmapError(errno);
		}
		g_local_info->shm_info_seg.is_mapped = FALSE;
	}
	
	if(g_local_info->shm_info_seg.is_init)
	{
		if(will_remove_info)
		{
			if(shm_unlink(g_local_info->shm_info_seg.name) != 0)
			{
				if(g_local_info->flags.is_proc_lock_init)
				{
					msh_ReleaseProcessLock();
				}
				ReadShmUnlinkError(errno);
			}
		}
		g_local_info->shm_info_seg.is_init = FALSE;
	}
	
	if(g_local_info->flags.is_proc_lock_init)
	{
		msh_ReleaseProcessLock();
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
	if(g_local_info != NULL)
	{
		msh_ReleaseProcessLock();
	}
}


void msh_AcquireProcessLock(void)
{
#ifdef MSH_THREAD_SAFE
	/* only request a lock if there is more than one process */
	if(g_shared_info->num_procs > 1 && g_shared_info->user_def.is_thread_safe && !g_local_info->flags.is_proc_locked)
	{
#ifdef MSH_WIN
		DWORD ret = WaitForSingleObject(g_local_info->proc_lock, INFINITE);
		if(ret == WAIT_ABANDONED)
		{
			ReadMexError("WaitProcLockAbandonedError", "One of the processes failed while using the lock. Cannot safely continue (Error number: %u).", GetLastError());
		}
		else if(ret == WAIT_FAILED)
		{
			ReadMexError("WaitProcLockFailedError", "The wait for process lock failed (Error number: %u).", GetLastError());
		}
#else
		
		if(lockf(g_local_info->proc_lock, F_LOCK, 0) != 0)
		{
			switch(errno)
			{
				case EBADF:
					ReadMexError("LockfBadFileError", "The fildes argument is not a valid open file descriptor; "
											    "or function is F_LOCK or F_TLOCK and fildes is not a valid file descriptor open for writing.");
				case EACCES:
					ReadMexError("LockfAccessError", "The function argument is F_TLOCK or F_TEST and the section is already locked by another process.");
				case EDEADLK:
					ReadMexError("LockfDeadlockError", "lockf failed because of one of the following:\n"
												"\tThe function argument is F_LOCK and a deadlock is detected.\n"
												"\tThe function argument is F_LOCK, F_TLOCK, or F_ULOCK, and the request would cause the number of locks to exceed a system-imposed limit.");
				case EOVERFLOW:
					ReadMexError("LockfOverflowError", "The offset of the first, or if size is not 0 then the last, "
												"byte in the requested section cannot be represented correctly in an object of type off_t.");
				case EAGAIN:
					ReadMexError("LockfAgainError", "The function argument is F_TLOCK or F_TEST and the section is already locked by another process.");
				case ENOLCK:
					ReadMexError("LockfNoLockError", "The function argument is F_LOCK, F_TLOCK, or F_ULOCK, and the request would cause the number of locks to exceed a system-imposed limit.");
				case EOPNOTSUPP:
				case EINVAL:
					ReadMexError("LockfOperationNotSupportedError", "The implementation does not support the locking of files of the type indicated by the fildes argument.");
				default:
					ReadMexError("LockfUnknownError", "An unknown error occurred (Error number: %i)", errno);
			}
		}

#endif
		g_local_info->flags.is_proc_locked = TRUE;
	}
#endif
}


void msh_ReleaseProcessLock(void)
{
#ifdef MSH_THREAD_SAFE
	if(g_local_info->flags.is_proc_locked)
	{

#ifdef MSH_WIN
		if(ReleaseMutex(g_local_info->proc_lock) == 0)
		{
			ReadMexError("ReleaseMutexError", "The process lock release failed (Error number: %u).", GetLastError());
		}
#else
		if(lockf(g_local_info->proc_lock, F_ULOCK, 0) != 0)
		{
			switch(errno)
			{
				case EBADF:
					ReadMexError("ULockfBadFileError", "The fildes argument is not a valid open file descriptor.");
				case EINTR:
					ReadMexError("ULockfInterruptError", "A signal was caught during execution of the function.");
				case EOVERFLOW:
					ReadMexError("ULockfOverflowError", "The offset of the first, or if size is not 0 then the last, "
												 "byte in the requested section cannot be represented correctly in an object of type off_t.");
				default:
					ReadMexError("ULockfUnknownError", "An unknown error occurred (Error number: %i)", errno);
			}
		}
#endif
		g_local_info->flags.is_proc_locked = FALSE;
	}
#endif
}


msh_directive_t msh_ParseDirective(const mxArray* in)
{
	/* easiest way to deal with implementation defined enum sizes */
	if(mxGetClassID(in) == mxUINT8_CLASS)
	{
		return (msh_directive_t)*((uchar_t*)(mxGetData(in)));
	}
	else
	{
		ReadMexError("InvalidDirectiveError", "Directive must be type 'uint8'.");
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
				ReadMexErrorWithCode(GetLastError(), "FlushFileError", "Error flushing the update file.");
			}
			if(FlushViewOfFile(curr_seg_node->seg_info.shared_memory_ptr, curr_seg_node->seg_info.seg_sz) == 0)
			{
				ReadMexErrorWithCode(GetLastError(), "FlushFileError", "Error flushing the data file.");
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
			if(msync(g_shared_info, g_local_info->shm_info_seg.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReadMsyncError(errno);
			}
			if(msync((void*)curr_seg_node->seg_info.shared_memory_ptr, curr_seg_node->seg_info.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReadMsyncError(errno);
			}
			curr_seg_node = curr_seg_node->next;
		}
	}

#endif
}


size_t PadToAlign(size_t curr_sz)
{
	return curr_sz + (ALIGN_SHIFT - ((curr_sz - 1) & ALIGN_SHIFT));
}


void msh_WriteSegmentName(char* name_buffer, msh_segmentnumber_t seg_num)
{
	snprintf(name_buffer, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, seg_num);
}


void msh_VariableGC(void)
{
	VariableNode_t* curr_var_node, * next_var_node;
	
	if(!g_shared_info->user_def.will_gc)
	{
		return;
	}
	
	curr_var_node = g_local_var_list.first;
	while(curr_var_node != NULL)
	{
		next_var_node = curr_var_node->next;
		if(msh_GetCrosslink(curr_var_node->var) == NULL && msh_GetSegmentMetadata(curr_var_node->seg_node)->is_used)
		{
			if(msh_GetSegmentMetadata(curr_var_node->seg_node)->procs_using == 1)
			{
				/* if this is the last process using this variable, destroy the segment completely */
				msh_DestroySegment(curr_var_node->seg_node);
			}
			else
			{
				/* otherwise just take out the variable */
				msh_DestroyVariable(curr_var_node);
			}
		}
		curr_var_node = next_var_node;
	}
	
}


void msh_NullFunction(void)
{
	/* does nothing (so we can reset the mexAtExit function, since NULL is undocumented) */
}