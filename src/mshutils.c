#include "headers/mshutils.h"
#include "headers/mshtypes.h"


/*
 * NEW MXMALLOC SIGNATURE INFO:
 * HEADER:
 * 		bytes 0-7 - size iterator, increases as (size/16+1)*16 mod 256
 * 			byte 0 = (((size+16-1)/2^4)%2^4)*2^4
 * 			byte 1 = (((size+16-1)/2^8)%2^8)
 * 			byte 2 = (((size+16-1)/2^16)%2^16)
 * 			byte n = (((size+16-1)/2^(2^(2+n)))%2^(2^(2+n)))
 *
 * 		bytes  8-11 - a signature for the vector check
 * 		bytes 12-13 - the alignment (should be 32 bytes for new MATLAB)
 * 		bytes 14-15 - the offset from the original pointer to the newly aligned pointer (should be 16 or 32)
 */
static const unsigned char c_MXMALLOC_SIGNATURE[MXMALLOC_SIG_LEN] = {16, 0, 0, 0, 0, 0, 0, 0, 206, 250, 237, 254, 32, 0, 32, 0};


void* MemCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz)
{
	uchar_t mxmalloc_sig[MXMALLOC_SIG_LEN];
	MakeMxMallocSignature(mxmalloc_sig, cpy_sz);
	
	memcpy(dest - MXMALLOC_SIG_LEN, mxmalloc_sig, MXMALLOC_SIG_LEN);
	if(orig != NULL)
	{
		memcpy(dest, orig, cpy_sz);
	}
	else
	{
		memset(dest, 0, cpy_sz);
	}
	return dest;
}

/* field names of a structure	*/
size_t GetFieldNamesSize(const mxArray* mxStruct)
{
	const char_t* field_name;
	int i, num_fields;
	size_t cml_sz = 0;
	
	/* Go through them */
	num_fields = mxGetNumberOfFields(mxStruct);
	for(i = 0; i < num_fields; i++)
	{
		/* This field */
		field_name = mxGetFieldNameByNumber(mxStruct, i);
		cml_sz += strlen(field_name) + 1; /* remember to add the null termination */
	}
	
	return cml_sz;
	
}


void GetNextFieldName(const char_t** field_str)
{
	*field_str = *field_str + strlen(*field_str) + 1;
}


void MshOnExit(void)
{
	
	if(g_info->flags.is_proc_lock_init)
	{
		AcquireProcessLock();
	}
	
	if(g_info->shm_info_seg.is_mapped)
	{
		UpdateSharedSegments();
	}
	
	CleanSegmentList(&g_seg_list);

#ifdef MSH_WIN
	
	if(g_info->shm_info_seg.is_mapped)
	{
		s_info->num_procs -= 1;
	}
	
	if(g_info->shm_info_seg.is_mapped)
	{
		
		/* unlock this set of pages */
		VirtualUnlock(g_info->shm_info_seg.s_ptr, g_info->shm_info_seg.seg_sz);
		
		if(UnmapViewOfFile(g_info->shm_info_seg.s_ptr) == 0)
		{
			ReadMexError("UnmapFileError", "Error unmapping the update file (Error Number %u)", GetLastError());
		}
		g_info->shm_info_seg.is_mapped = FALSE;
	}
	
	if(g_info->shm_info_seg.is_init)
	{
		if(CloseHandle(g_info->shm_info_seg.handle) == 0)
		{
			ReadMexError("CloseHandleError", "Error closing the update file handle (Error Number %u)", GetLastError());
		}
		g_info->shm_info_seg.is_init = FALSE;
	}
	
	if(g_info->flags.is_proc_lock_init)
	{
		ReleaseProcessLock();
		if(CloseHandle(g_info->proc_lock) == 0)
		{
			ReadMexError("CloseHandleError", "Error closing the process lock handle (Error Number %u)", GetLastError());
		}
		g_info->flags.is_proc_lock_init = FALSE;
	}

#else
	
	bool_t will_remove_info = FALSE;
	
	
	if(g_info->shm_info_seg.is_mapped)
	{
		s_info->num_procs -= 1;
		will_remove_info = (bool_t)(s_info->num_procs == 0);
	}
	
	if(g_info->shm_info_seg.is_mapped)
	{
		if(munmap(s_info, g_info->shm_info_seg.seg_sz) != 0)
		{
			if(g_info->flags.is_proc_lock_init)
			{
				ReleaseProcessLock();
			}
			ReadMunmapError(errno);
		}
		g_info->shm_info_seg.is_mapped = FALSE;
	}
	
	if(g_info->shm_info_seg.is_init)
	{
		if(will_remove_info)
		{
			if(shm_unlink(g_info->shm_info_seg.name) != 0)
			{
				if(g_info->flags.is_proc_lock_init)
				{
					ReleaseProcessLock();
				}
				ReadShmUnlinkError(errno);
			}
		}
		g_info->shm_info_seg.is_init = FALSE;
	}
	
	if(g_info->flags.is_proc_lock_init)
	{
		ReleaseProcessLock();
		if(will_remove_info)
		{
			if(shm_unlink(MSH_LOCK_NAME) != 0)
			{
				ReadShmUnlinkError(errno);
			}
		}
		g_info->flags.is_proc_lock_init = FALSE;
	}

#endif
	
	mxFree(g_info);
	g_info = NULL;
	mexAtExit(NullFunction);
	
	if(mexIsLocked())
	{
		mexUnlock();
	}
	
}


void MshOnError(void)
{
	if(g_info != NULL)
	{
		ReleaseProcessLock();
	}
}


void MakeMxMallocSignature(uchar_t* sig, size_t seg_size)
{
	/*
	 * MXMALLOC SIGNATURE INFO:
	 * 	mxMalloc adjusts the size of memory for 32 byte alignment (shifts either 16 or 32 bytes)
	 * 	it appends the following 16 byte header immediately before data segment
	 *
	 * HEADER:
	 * 		bytes 0-7 - size iterator, increases as (size/16+1)*16 mod 256
	 * 			byte 0 = (((size+16-1)/2^4)%2^4)*2^4
	 * 			byte 1 = (((size+16-1)/2^8)%2^8)
	 * 			byte 2 = (((size+16-1)/2^16)%2^16)
	 * 			byte n = (((size+16-1)/2^(2^(2+n)))%2^(2^(2+n)))
	 *
	 * 		bytes  8-11 - a signature for the vector check
	 * 		bytes 12-13 - the alignment (should be 32 bytes for new MATLAB)
	 * 		bytes 14-15 - the offset from the original pointer to the newly aligned pointer (should be 16 or 32)
	 */
	
	unsigned int i;
	
	memcpy(sig, c_MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
	size_t multi = 1u << 4u;
	
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	if(seg_size > 0)
	{
		sig[0] = (uchar_t)((((seg_size + 0x0F)/multi) & (multi - 1))*multi);
		
		/* note: this only does bits 1 to 3 because of 64 bit precision limit (maybe implement bit 4 in the future?)*/
		for(i = 1; i < 4; i++)
		{
			multi = (size_t)1u << (1u << (2u + i));
			sig[i] = (uchar_t)(((seg_size + 0x0F)/multi) & (multi - 1));
		}
	}
}


void AcquireProcessLock(void)
{
#ifdef MSH_THREAD_SAFE
	/* only request a lock if there is more than one process */
	if(s_info->num_procs > 1 && s_info->user_def.is_thread_safe && !g_info->flags.is_proc_locked)
	{
#ifdef MSH_WIN
		DWORD ret = WaitForSingleObject(g_info->proc_lock, INFINITE);
		if(ret == WAIT_ABANDONED)
		{
			ReadMexError("WaitProcLockAbandonedError", "One of the processes failed while using the lock. Cannot safely continue (Error number: %u).", GetLastError());
		}
		else if(ret == WAIT_FAILED)
		{
			ReadMexError("WaitProcLockFailedError", "The wait for process lock failed (Error number: %u).", GetLastError());
		}
#else
		
		if(lockf(g_info->proc_lock, F_LOCK, 0) != 0)
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
		g_info->flags.is_proc_locked = TRUE;
	}
#endif
}


void ReleaseProcessLock(void)
{
#ifdef MSH_THREAD_SAFE
	if(g_info->flags.is_proc_locked)
	{

#ifdef MSH_WIN
		if(ReleaseMutex(g_info->proc_lock) == 0)
		{
			ReadMexError("ReleaseMutexError", "The process lock release failed (Error number: %u).", GetLastError());
		}
#else
		if(lockf(g_info->proc_lock, F_ULOCK, 0) != 0)
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
		g_info->flags.is_proc_locked = FALSE;
	}
#endif
}


msh_directive_t ParseDirective(const mxArray* in)
{
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


void ParseParams(int num_params, const mxArray** in)
{
	size_t i, j, ps_len, vs_len;
	char param_str[MSH_MAX_NAME_LEN] = {0}, val_str[MSH_MAX_NAME_LEN] = {0}, param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
	
	if(num_params == 0)
	{
#ifdef MSH_WIN
	mexPrintf(MSH_PARAM_INFO, s_info->user_def.is_thread_safe? "true" : "false", s_info->user_def.sharetype == msh_SHARETYPE_COPY? "true" : "false");
#else
	mexPrintf(MSH_PARAM_INFO, s_info->user_def.is_thread_safe? "true" : "false", s_info->user_def.sharetype == msh_SHARETYPE_COPY? "true" : "false", s_info->security);
#endif
	}
	
	if(num_params % 2 != 0)
	{
		ReadMexError("InvalNumArgsError", "The number of parameters input must be a multiple of two.");
	}
	
	for(i = 0; i < num_params/2; i++)
	{
		param = in[2*i];
		val = in[2*i + 1];
		
		if(!mxIsChar(param) || !mxIsChar(val))
		{
			ReadMexError("InvalidArgumentError", "All parameters and values must be input as character arrays.");
		}
		
		ps_len = mxGetNumberOfElements(param);
		vs_len = mxGetNumberOfElements(val);
		
		if(ps_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadMexError("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		else if(vs_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, param_str);
		}
		
		mxGetString(param, param_str, MSH_MAX_NAME_LEN);
		mxGetString(val, val_str, MSH_MAX_NAME_LEN);
		
		for(j = 0; j < ps_len; j++)
		{
			param_str_l[j] = (char)tolower(param_str[j]);
		}
		
		for(j = 0; j < vs_len; j++)
		{
			val_str_l[j] = (char)tolower(val_str[j]);
		}
		
		if(strcmp(param_str_l, MSH_PARAM_THREADSAFETY_L) == 0)
		{
#ifdef MSH_THREAD_SAFE
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				s_info->user_def.is_thread_safe = TRUE;
			}
			if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				s_info->user_def.is_thread_safe = FALSE;
			}
			else
			{
				ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_THREADSAFETY);
			}
			UpdateAll();
			ReleaseProcessLock();
#else
			ReadMexError("InvalidParamError", "Cannot change the state of thread safety for matshare compiled with thread safety turned off.");
#endif
		
		}
		else if(strcmp(param_str_l, MSH_PARAM_SECURITY_L) == 0)
		{
#ifdef MSH_UNIX
			if(vs_len < 3 || vs_len > 4)
			{
				ReadMexError("InvalidParamValueError", "Too many or too few digits in \"%s\" for parameter \"%s\". Must have either 3 or 4 digits.", val_str, MSH_PARAM_SECURITY_U);
			}
			else
			{
				AcquireProcessLock();
				s_info->security = (mode_t)strtol(val_str_l, NULL, 8);
				if(fchmod(g_info->shm_info_seg.handle, s_info->security) != 0)
				{
					ReadFchmodError(errno);
				}
				SegmentNode_t* curr_seg_node = g_seg_list.first;
				while(curr_seg_node != NULL)
				{
					if(fchmod(curr_seg_node->seg_info.handle, s_info->security) != 0)
					{
						ReadFchmodError(errno);
					}
					curr_seg_node = curr_seg_node->next;
				}
#ifdef MSH_THREAD_SAFE
				if(fchmod(g_info->proc_lock, s_info->security) != 0)
				{
					ReadFchmodError(errno);
				}
#endif
				UpdateAll();
				ReleaseProcessLock();
			}
#else
			ReadMexError("InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", param_str);
#endif
		}
		else if(strcmp(param_str_l, MSH_PARAM_COPYONWRITE_L) == 0)
		{
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				s_info->user_def.sharetype = msh_SHARETYPE_COPY;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				s_info->user_def.sharetype = msh_SHARETYPE_OVERWRITE;
			}
			else
			{
				ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_COPYONWRITE_U);
			}
			ReleaseProcessLock();
		}
		else if(strcmp(param_str_l, MSH_PARAM_REMOVEUNUSED_L) == 0)
		{
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				s_info->user_def.will_remove_unused = TRUE;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				s_info->user_def.will_remove_unused = FALSE;
			}
			else
			{
				ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_REMOVEUNUSED_U);
			}
			ReleaseProcessLock();
		}
		else
		{
			ReadMexError("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		
	}
}


void UpdateAll(void)
{

#ifdef MSH_WIN
	
	/* only flush the memory when there is more than one process
	 * this appears to only write to pagefile.sys, but it's difficult to find information
	if(s_info->num_procs > 1)
	{
		SegmentNode_t* curr_seg_node = g_seg_list.first;
		while(curr_seg_node != NULL)
		{
			if(FlushViewOfFile(s_info, g_info->shm_info_seg.seg_sz) == 0)
			{
				ReadMexError("FlushFileError", "Error flushing the update file (Error Number %u)", GetLastError());
			}
			if(FlushViewOfFile(curr_seg_node->seg_info.s_ptr, curr_seg_node->seg_info.seg_sz) == 0)
			{
				ReadMexError("FlushFileError", "Error flushing the data file (Error Number %u)", GetLastError());
			}

			curr_seg_node = curr_seg_node->next;
		}
	}
	*/
#else
	
	/* only flush the memory when there is more than one process */
	if(s_info->num_procs > 1)
	{
		SegmentNode_t* curr_seg_node = g_seg_list.first;
		while(curr_seg_node != NULL)
		{
			/* flushes any possible local caches to the shared memory */
			if(msync(s_info, g_info->shm_info_seg.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReadMsyncError(errno);
			}
			if(msync(curr_seg_node->seg_info.s_ptr, curr_seg_node->seg_info.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReadMsyncError(errno);
			}
			curr_seg_node = curr_seg_node->next;
		}
	}

#endif
}


/* checks if everything is in working order before doing the operation */
bool_t Precheck(void)
{
	bool_t ret = g_info->shm_info_seg.is_init & g_info->shm_info_seg.is_mapped;
#ifdef MSH_THREAD_SAFE
	return ret & g_info->flags.is_proc_lock_init;
#else
	return ret;
#endif
}


void SetDataPointers(mxArray* var, SharedDataPointers_t* data_ptrs)
{
	
	mxSetData(var, data_ptrs->data);
	mxSetImagData(var, data_ptrs->imag_data);
	
	if(mxIsSparse(var))
	{
		mxSetIr(var, data_ptrs->ir);
		mxSetJc(var, data_ptrs->jc);
	}
	
}

void WriteSegmentName(char name_buffer[static MSH_MAX_NAME_LEN], msh_segmentnumber_t seg_num)
{
	snprintf(name_buffer, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, seg_num);
}


void RemoveUnusedVariables(void)
{
	VariableNode_t* curr_var_node, * next_var_node;
	
	curr_var_node = g_var_list.first;
	
	while(curr_var_node != NULL)
	{
		next_var_node = curr_var_node->next;
		if(*curr_var_node->crosslink == NULL && SegmentMetadata(curr_var_node->seg_node)->is_used)
		{
			if(SegmentMetadata(curr_var_node->seg_node)->procs_using == 1)
			{
				/* if this is the last process using this variable, destroy the segment completely */
				DestroySegmentNode(&g_seg_list, curr_var_node->seg_node);
			}
			else
			{
				/* otherwise just take out the variable */
				DestroyVariableNode(curr_var_node);
			}
		}
		curr_var_node = next_var_node;
	}
	
}


void NullFunction(void)
{
	/* does nothing (so we can reset the mexAtExit function, since NULL is undocumented) */
}