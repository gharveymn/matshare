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


void MshExit(void)
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
			ReadErrorMex("UnmapFileError", "Error unmapping the update file (Error Number %u)", GetLastError());
		}
		g_info->shm_info_seg.is_mapped = FALSE;
	}
	
	if(g_info->shm_info_seg.is_init)
	{
		if(CloseHandle(g_info->shm_info_seg.handle) == 0)
		{
			ReadErrorMex("CloseHandleError", "Error closing the update file handle (Error Number %u)", GetLastError());
		}
		g_info->shm_info_seg.is_init = FALSE;
	}
	
	if(g_info->flags.is_proc_lock_init)
	{
		ReleaseProcessLock();
		if(CloseHandle(g_info->proc_lock) == 0)
		{
			ReadErrorMex("CloseHandleError", "Error closing the process lock handle (Error Number %u)", GetLastError());
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
	if(s_info->num_procs > 1 && s_info->is_thread_safe && !g_info->flags.is_proc_locked)
	{
#ifdef MSH_WIN
		DWORD ret = WaitForSingleObject(g_info->proc_lock, INFINITE);
		if(ret == WAIT_ABANDONED)
		{
			ReadErrorMex("WaitProcLockAbandonedError", "One of the processes failed while using the lock. Cannot safely continue (Error number: %u).", GetLastError());
		}
		else if(ret == WAIT_FAILED)
		{
			ReadErrorMex("WaitProcLockFailedError", "The wait for process lock failed (Error number: %u).", GetLastError());
		}
#else
		
		if(lockf(g_info->proc_lock, F_LOCK, 0) != 0)
		{
			switch(errno)
			{
				case EBADF:
					ReadErrorMex("LockfBadFileError", "The fildes argument is not a valid open file descriptor; "
											    "or function is F_LOCK or F_TLOCK and fildes is not a valid file descriptor open for writing.");
				case EACCES:
					ReadErrorMex("LockfAccessError", "The function argument is F_TLOCK or F_TEST and the section is already locked by another process.");
				case EDEADLK:
					ReadErrorMex("LockfDeadlockError", "lockf failed because of one of the following:\n"
												"\tThe function argument is F_LOCK and a deadlock is detected.\n"
												"\tThe function argument is F_LOCK, F_TLOCK, or F_ULOCK, and the request would cause the number of locks to exceed a system-imposed limit.");
				case EOVERFLOW:
					ReadErrorMex("LockfOverflowError", "The offset of the first, or if size is not 0 then the last, "
												"byte in the requested section cannot be represented correctly in an object of type off_t.");
				case EAGAIN:
					ReadErrorMex("LockfAgainError", "The function argument is F_TLOCK or F_TEST and the section is already locked by another process.");
				case ENOLCK:
					ReadErrorMex("LockfNoLockError", "The function argument is F_LOCK, F_TLOCK, or F_ULOCK, and the request would cause the number of locks to exceed a system-imposed limit.");
				case EOPNOTSUPP:
				case EINVAL:
					ReadErrorMex("LockfOperationNotSupportedError", "The implementation does not support the locking of files of the type indicated by the fildes argument.");
				default:
					ReadErrorMex("LockfUnknownError", "An unknown error occurred (Error number: %i)", errno);
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
			ReadErrorMex("ReleaseMutexError", "The process lock release failed (Error number: %u).", GetLastError());
		}
#else
		if(lockf(g_info->proc_lock, F_ULOCK, 0) != 0)
		{
			switch(errno)
			{
				case EBADF:
					ReadErrorMex("ULockfBadFileError", "The fildes argument is not a valid open file descriptor.");
				case EINTR:
					ReadErrorMex("ULockfInterruptError", "A signal was caught during execution of the function.");
				case EOVERFLOW:
					ReadErrorMex("ULockfOverflowError", "The offset of the first, or if size is not 0 then the last, "
												 "byte in the requested section cannot be represented correctly in an object of type off_t.");
				default:
					ReadErrorMex("ULockfUnknownError", "An unknown error occurred (Error number: %i)", errno);
			}
		}
#endif
		g_info->flags.is_proc_locked = FALSE;
	}
#endif
}


mshdirective_t ParseDirective(const mxArray* in)
{
	int i;
	if(mxIsNumeric(in))
	{
		return (mshdirective_t)*((unsigned char*)(mxGetData(in)));
	}
	else if(mxIsChar(in))
	{
		char_t dir_str[9];
		mxGetString(in, dir_str, 9);
		for(i = 0; dir_str[i]; i++)
		{
			dir_str[i] = (char_t)tolower(dir_str[i]);
		}
		
		if(strcmp(dir_str, "share") == 0)
		{
			return msh_SHARE;
		}
		else if(strcmp(dir_str, "detach") == 0)
		{
			return msh_DETACH;
		}
		else if(strcmp(dir_str, "fetch") == 0)
		{
			return msh_FETCH;
		}
		else if(strcmp(dir_str, "ShmCopy_") == 0)
		{
			return msh_DEEPCOPY;
		}
		else if(strcmp(dir_str, "debug") == 0)
		{
			return msh_DEBUG;
		}
		else if(strcmp(dir_str, "clone") == 0)
		{
			return msh_SHARE;
		}
		else
		{
			ReadErrorMex("InvalidDirectiveError", "Directive not recognized.");
		}
		
	}
	else
	{
		ReadErrorMex("InvalidDirectiveError", "Directive must either be 'uint8' or 'char_t'.");
	}
	
	return msh_DEBUG;
	
}


void ParseParams(int num_params, const mxArray** in)
{
	size_t i, j, ps_len, vs_len;
	char param_str[MSH_MAX_NAME_LEN] = {0}, val_str[MSH_MAX_NAME_LEN] = {0}, param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
	
	if(num_params % 2 != 0)
	{
		ReadErrorMex("InvalNumArgsError", "The number of parameters input must be a multiple of two.");
	}
	
	for(i = 0; i < num_params/2; i++)
	{
		param = in[2*i];
		val = in[2*i + 1];
		
		if(!mxIsChar(param) || !mxIsChar(val))
		{
			ReadErrorMex("InvalidArgumentError", "All parameters and values must be input as character arrays.");
		}
		
		ps_len = mxGetNumberOfElements(param);
		vs_len = mxGetNumberOfElements(val);
		
		if(ps_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadErrorMex("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		else if(vs_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadErrorMex("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, param_str);
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
		
		if(strcmp(param_str_l, MSH_PARAM_THRSAFE_L) == 0)
		{
#ifdef MSH_THREAD_SAFE
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				s_info->is_thread_safe = TRUE;
			}
			if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				s_info->is_thread_safe = FALSE;
			}
			else
			{
				ReleaseProcessLock();
				ReadErrorMex("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_THRSAFE_U);
			}
			UpdateAll();
			ReleaseProcessLock();
#else
			ReadErrorMex("InvalidParamError", "Cannot change the state of thread safety for matshare compiled with thread safety turned off.");
#endif
		
		}
		else if(strcmp(param_str_l, MSH_PARAM_SECURITY_L) == 0)
		{
#ifdef MSH_UNIX
			if(vs_len < 3 || vs_len > 4)
			{
				ReadErrorMex("InvalidParamValueError", "Too many or too few digits in \"%s\" for parameter \"%s\". Must have either 3 or 4 digits.", val_str, MSH_PARAM_SECURITY_U);
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
			ReadErrorMex("InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", param_str);
#endif
		}
		else if(strcmp(param_str_l, MSH_PARAM_COPYONWRITE_L) == 0)
		{
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				s_info->sharetype = msh_SHARETYPE_COPY;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				s_info->sharetype = msh_SHARETYPE_OVERWRITE;
			}
			else
			{
				ReleaseProcessLock();
				ReadErrorMex("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_COPYONWRITE_U);
			}
			ReleaseProcessLock();
		}
		else
		{
			ReadErrorMex("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		
	}
}


void UpdateAll(void)
{
	/* s_info->update_pid = g_info->this_pid; */
	
	/* only flush the memory when there is more than one process */
	if(s_info->num_procs > 1)
	{
		SegmentNode_t* curr_seg_node = g_seg_list.first;
		while(curr_seg_node != NULL)
		{
#ifdef MSH_WIN
			/* not sure if this is required on windows, but it doesn't hurt */
			if(FlushViewOfFile(s_info, g_info->shm_info_seg.seg_sz) == 0)
			{
				ReadErrorMex("FlushFileError", "Error flushing the update file (Error Number %u)", GetLastError());
			}
			if(FlushViewOfFile(curr_seg_node->seg_info.s_ptr, curr_seg_node->seg_info.seg_sz) == 0)
			{
				ReadErrorMex("FlushFileError", "Error flushing the data file (Error Number %u)", GetLastError());
			}
#else
			/* yes, this is required to ensure the changes are written (mmap creates a virtual address space)
			 * no, I don't know how this is possible without doubling the actual amount of RAM needed */
			if(msync(s_info, g_info->shm_info_seg.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReleaseProcessLock();
				ReadMsyncError(errno);
			}
			if(msync(curr_seg_node->seg_info.s_ptr, curr_seg_node->seg_info.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
			{
				ReleaseProcessLock();
				ReadMsyncError(errno);
			}
#endif
			curr_seg_node = curr_seg_node->next;
		}
	}
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


void RemoveUnusedVariables(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node, * next_var_node;
	
	if(var_list != NULL)
	{
		curr_var_node = var_list->first;
		while(curr_var_node != NULL)
		{
			next_var_node = curr_var_node->next;
			if(*curr_var_node->crosslink == NULL && curr_var_node->seg_node->seg_info.s_ptr->is_used)
			{
				if(curr_var_node->seg_node->seg_info.s_ptr->procs_using == 1)
				{
					/* if this is the last process using this variable, destroy the segment completely */
					DestroySegmentNode(curr_var_node->seg_node);
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
	
}


void NullFunction(void)
{
	/* does nothing (so we can reset the mexAtExit function, since NULL is undocumented) */
}