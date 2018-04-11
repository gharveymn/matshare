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

void* memCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz)
{
	unsigned char mxmalloc_sig[MXMALLOC_SIG_LEN];
	makeMxMallocSignature(mxmalloc_sig, cpy_sz);

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


void locateDataPointers(ShmData_t* data_ptrs, Header_t* hdr, byte_t* shm_anchor)
{
	data_ptrs->dims = hdr->data_offsets.dims == SIZE_MAX? NULL : (mwSize*)(shm_anchor + hdr->data_offsets.dims);
	data_ptrs->pr = hdr->data_offsets.pr == SIZE_MAX? NULL : shm_anchor + hdr->data_offsets.pr;
	data_ptrs->pi = hdr->data_offsets.pi == SIZE_MAX? NULL : shm_anchor + hdr->data_offsets.pi;
	data_ptrs->ir = hdr->data_offsets.ir == SIZE_MAX? NULL : (mwIndex*)(shm_anchor + hdr->data_offsets.ir);
	data_ptrs->jc = hdr->data_offsets.jc == SIZE_MAX? NULL : (mwIndex*)(shm_anchor + hdr->data_offsets.jc);
	data_ptrs->field_str = hdr->data_offsets.field_str == SIZE_MAX? NULL : shm_anchor + hdr->data_offsets.field_str;
	data_ptrs->child_hdrs = hdr->data_offsets.child_hdrs == SIZE_MAX? NULL : (size_t*)(shm_anchor + hdr->data_offsets.child_hdrs);
}


/* field names of a structure	*/
size_t getFieldNamesSize(const mxArray* mxStruct)
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


void getNextFieldName(const char_t** field_str)
{
	*field_str = *field_str + strlen(*field_str) + 1;
}


void onExit(void)
{
	bool_t will_remove_data;
	VariableNode_t* curr_var_node,* next_var_node;
	SegmentNode_t* curr_seg_node,* next_seg_node;
	
	mshUpdateSegments();
	
	if(g_info->flags.is_proc_lock_init)
	{
		acquireProcLock();
	}
	
	curr_var_node = g_info->var_list.front;
	while(curr_var_node != NULL)
	{
		next_var_node = curr_var_node->next;
		if(!mxIsEmpty(curr_var_node->var))
		{
			/* NULL all of the Matlab pointers */
			shmDetach(curr_var_node->var);
		}
		mxDestroyArray(curr_var_node->var);
		mxFree(curr_var_node);
		curr_var_node = next_var_node;
	}
	
#ifdef MSH_WIN


	if(g_info->shm_info_seg.is_mapped)
	{
		shm_info->num_procs -= 1;
	}

	curr_seg_node = g_info->seg_list.front;
	while(curr_seg_node != NULL)
	{
		
		next_seg_node = curr_seg_node->next;
		if(curr_seg_node->data_seg.is_mapped)
		{
			
			if(UnmapViewOfFile(curr_seg_node->data_seg.ptr) == 0)
			{
				if(g_info->flags.is_proc_lock_init)
				{ releaseProcLock(); }
				readErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
			}
			curr_seg_node->data_seg.is_mapped = FALSE;
		}

		if(curr_seg_node->data_seg.is_init)
		{
			if(CloseHandle(curr_seg_node->data_seg.handle) == 0)
			{
				if(g_info->flags.is_proc_lock_init)
				{ releaseProcLock(); }
				readErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
			}
			curr_seg_node->data_seg.is_init = FALSE;
		}

		if(next_seg_node != NULL)
		{
			next_seg_node->prev = NULL;
			next_seg_node->prev_seg_num = -1;
		}

		/* repoint top of the stack in case of crash */
		g_info->seg_list.front = next_seg_node;
		
		mxFree(curr_var_node);
		curr_var_node = next_var_node;

	}

	if(g_info->shm_info_seg.is_mapped)
	{
		if(UnmapViewOfFile(shm_info) == 0)
		{
			if(g_info->flags.is_proc_lock_init)
			{ releaseProcLock(); }
			readErrorMex("UnmapFileError", "Error unmapping the update file (Error Number %u)", GetLastError());
		}
		g_info->shm_info_seg.is_mapped = FALSE;
	}

	if(g_info->shm_info_seg.is_init)
	{
		if(CloseHandle(g_info->shm_info_seg.handle) == 0)
		{
			if(g_info->flags.is_proc_lock_init)
			{ releaseProcLock(); }
			readErrorMex("CloseHandleError", "Error closing the update file handle (Error Number %u)", GetLastError());
		}
		g_info->shm_info_seg.is_init = FALSE;
	}

#ifdef MSH_AUTO_INIT
	if(g_info->lcl_init_seg.is_init)
	{
		if(CloseHandle(g_info->lcl_init_seg.handle) == 0)
		{
			if(g_info->flags.is_proc_lock_init)
			{ releaseProcLock(); }
			readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
		}
		g_info->lcl_init_seg.is_init = FALSE;
	}
#endif

	if(g_info->flags.is_proc_lock_init)
	{
		releaseProcLock();
		if(CloseHandle(g_info->proc_lock) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the process lock handle (Error Number %u)", GetLastError());
		}
		g_info->flags.is_proc_lock_init = FALSE;
	}

#else

	bool_t will_remove_info = FALSE;
	

	if(g_info->shm_info_seg.is_mapped)
	{
		shm_info->num_procs -= 1;
		will_remove_info = (bool_t)(shm_info->num_procs == 0);
	}

	curr_seg_node = g_info->seg_list.front;
	while(curr_seg_node != NULL)
	{
		next_seg_node = curr_seg_node->next;
		
		will_remove_data = FALSE;
		if(curr_seg_node->data_seg.is_mapped)
		{

			will_remove_data = (bool_t) (curr_seg_node->data_seg.ptr->procs_using == 1);

			if(munmap(curr_seg_node->data_seg.ptr, curr_seg_node->data_seg.seg_sz) != 0)
			{
				if(g_info->flags.is_proc_lock_init)
				{ releaseProcLock(); }
				readMunmapError(errno);
			}
			curr_seg_node->data_seg.is_mapped = FALSE;
		}

		if(curr_seg_node->data_seg.is_init)
		{
			if(will_remove_data)
			{
				if(shm_unlink(curr_seg_node->data_seg.name) != 0)
				{
					if(g_info->flags.is_proc_lock_init)
					{ releaseProcLock(); }
					readShmUnlinkError(errno);
				}
			}
			curr_seg_node->data_seg.is_init = FALSE;
		}
		
		
		if(next_seg_node != NULL)
		{
			next_seg_node->prev = NULL;
			next_seg_node->prev_seg_num = -1;
		}

		/* repoint top of the stack in case of crash */
		g_info->seg_list.front = next_seg_node;

		mxFree(curr_seg_node);
		curr_seg_node = next_seg_node;

	}

	if(g_info->shm_info_seg.is_mapped)
	{
		if(munmap(shm_info, g_info->shm_info_seg.seg_sz) != 0)
		{
			if(g_info->flags.is_proc_lock_init){releaseProcLock();}
			readMunmapError(errno);
		}
		g_info->shm_info_seg.is_mapped = FALSE;
	}

	if(g_info->shm_info_seg.is_init)
	{
		if(will_remove_info)
		{
			if(shm_unlink(g_info->shm_info_seg.name) != 0)
			{
				if(g_info->flags.is_proc_lock_init){releaseProcLock();}
				readShmUnlinkError(errno);
			}
		}
		g_info->shm_info_seg.is_init = FALSE;
	}

#ifdef MSH_AUTO_INIT
	if(g_info->lcl_init_seg.is_init)
	{
		if(shm_unlink(g_info->lcl_init_seg.name) != 0)
		{
			if(g_info->flags.is_proc_lock_init){releaseProcLock();}
			readShmUnlinkError(errno);
		}
		g_info->lcl_init_seg.is_init = FALSE;
	}
#endif

	if(g_info->flags.is_proc_lock_init)
	{
		releaseProcLock();
		if(will_remove_info)
		{
			if(shm_unlink(MSH_LOCK_NAME) != 0)
			{
				readShmUnlinkError(errno);
			}
		}
		g_info->flags.is_proc_lock_init = FALSE;
	}

#endif

	mxFree(g_info);
	g_info = NULL;
	mexAtExit(nullfcn);

}


size_t padToAlign(size_t size)
{
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	return size + ALIGN_SIZE - (size & (ALIGN_SIZE-1));
}


void makeMxMallocSignature(unsigned char* sig, size_t seg_size)
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
		sig[0] = (unsigned char)((((seg_size + 0x0F)/multi) & (multi - 1))*multi);

		/* note: this only does bits 1 to 3 because of 64 bit precision limit (maybe implement bit 4 in the future?)*/
		for(i = 1; i < 4; i++)
		{
			multi = (size_t)1u << (1u << (2u + i));
			sig[i] = (unsigned char)(((seg_size + 0x0F)/multi) & (multi - 1));
		}
	}
}


void acquireProcLock(void)
{
#ifdef MSH_THREAD_SAFE
	/* only request a lock if there is more than one process */
	if(shm_info->num_procs > 1 && shm_info->is_thread_safe && !g_info->flags.is_proc_locked)
	{
#ifdef MSH_WIN
		DWORD ret = WaitForSingleObject(g_info->proc_lock, INFINITE);
		if(ret == WAIT_ABANDONED)
		{
			readErrorMex("WaitProcLockAbandonedError", "One of the processes failed while using the lock. Cannot safely continue (Error number: %u).", GetLastError());
		}
		else if(ret == WAIT_FAILED)
		{
			readErrorMex("WaitProcLockFailedError", "The wait for process lock failed (Error number: %u).", GetLastError());
		}
#else

		if(lockf(g_info->proc_lock, F_LOCK, 0) != 0)
		{
			switch(errno)
			{
				case EBADF:
					readErrorMex("LockfBadFileError", "The fildes argument is not a valid open file descriptor; "
							"or function is F_LOCK or F_TLOCK and fildes is not a valid file descriptor open for writing.");
				case EACCES:
					readErrorMex("LockfAccessError", "The function argument is F_TLOCK or F_TEST and the section is already locked by another process.");
				case EDEADLK:
					readErrorMex("LockfDeadlockError", "lockf failed because of one of the following:\n"
							"\tThe function argument is F_LOCK and a deadlock is detected.\n"
							"\tThe function argument is F_LOCK, F_TLOCK, or F_ULOCK, and the request would cause the number of locks to exceed a system-imposed limit.");
				case EOVERFLOW:
					readErrorMex("LockfOverflowError", "The offset of the first, or if size is not 0 then the last, "
							"byte in the requested section cannot be represented correctly in an object of type off_t.");
				case EAGAIN:
					readErrorMex("LockfAgainError", "The function argument is F_TLOCK or F_TEST and the section is already locked by another process.");
				case ENOLCK:
					readErrorMex("LockfNoLockError", "The function argument is F_LOCK, F_TLOCK, or F_ULOCK, and the request would cause the number of locks to exceed a system-imposed limit.");
				case EOPNOTSUPP:
				case EINVAL:
					readErrorMex("LockfOperationNotSupportedError", "The implementation does not support the locking of files of the type indicated by the fildes argument.");
				default:
					readErrorMex("LockfUnknownError", "An unknown error occurred (Error number: %i)", errno);
			}
		}

#endif
		g_info->flags.is_proc_locked = TRUE;
	}
#endif
}


void releaseProcLock(void)
{
#ifdef MSH_THREAD_SAFE
	if(g_info->flags.is_proc_locked)
	{

#ifdef MSH_WIN
		if(ReleaseMutex(g_info->proc_lock) == 0)
		{
			readErrorMex("ReleaseMutexError", "The process lock release failed (Error number: %u).", GetLastError());
		}
#else
		if(lockf(g_info->proc_lock, F_ULOCK, 0) != 0)
		{
			switch(errno)
			{
				case EBADF:
					readErrorMex("ULockfBadFileError", "The fildes argument is not a valid open file descriptor.");
				case EINTR:
					readErrorMex("ULockfInterruptError", "A signal was caught during execution of the function.");
				case EOVERFLOW:
					readErrorMex("ULockfOverflowError", "The offset of the first, or if size is not 0 then the last, "
							"byte in the requested section cannot be represented correctly in an object of type off_t.");
				default:
					readErrorMex("ULockfUnknownError", "An unknown error occurred (Error number: %i)", errno);
			}
		}
#endif
		g_info->flags.is_proc_locked = FALSE;
	}
#endif
}


mshdirective_t parseDirective(const mxArray* in)
{

	if(mxIsNumeric(in))
	{
		return (mshdirective_t)*((unsigned char*)(mxGetData(in)));
	}
	else if(mxIsChar(in))
	{
		char_t dir_str[9];
		mxGetString(in, dir_str, 9);
		for(int i = 0; dir_str[i]; i++)
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
		else if(strcmp(dir_str, "shmCopy_") == 0)
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
			readErrorMex("InvalidDirectiveError", "Directive not recognized.");
		}

	}
	else
	{
		readErrorMex("InvalidDirectiveError", "Directive must either be 'uint8' or 'char_t'.");
	}

	return msh_DEBUG;

}


void parseParams(int num_params, const mxArray* in[])
{
	size_t i, ps_len, vs_len;
	char* param_str,* val_str;
	char param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param,* val;
	for(i = 0; i < num_params/2; i++)
	{
		param = in[2*i];
		val = in[2*i + 1];

		if(!mxIsChar(param) || !mxIsChar(val))
		{
			readErrorMex("InvalidArgumentError", "All parameters and values must be input as character arrays.");
		}


		param_str = mxArrayToString(param);
		val_str = mxArrayToString(val);

		ps_len = mxGetNumberOfElements(param);
		vs_len =  mxGetNumberOfElements(val);

		if(ps_len >= MSH_MAX_NAME_LEN)
		{
			readErrorMex("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		else if(vs_len >= MSH_MAX_NAME_LEN)
		{
			readErrorMex("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, param_str);
		}

		for(int j = 0; j < ps_len; j++)
		{
			param_str_l[j] = (char)tolower(param_str[j]);
		}

		for(int j = 0; j < vs_len; j++)
		{
			val_str_l[j] = (char)tolower(val_str[j]);
		}

		if(strcmp(param_str_l, MSH_PARAM_THRSAFE_L) == 0)
		{
#ifdef MSH_THREAD_SAFE
			acquireProcLock();
			if(strcmp(val_str_l, "true") == 0
					|| strcmp(val_str_l, "on") == 0
					|| strcmp(val_str_l, "enable") == 0)
			{
				shm_info->is_thread_safe = TRUE;
			}
			if(strcmp(val_str_l, "false") == 0
			   || strcmp(val_str_l, "off") == 0
			   || strcmp(val_str_l, "disable") == 0)
			{
				shm_info->is_thread_safe = FALSE;
			}
			else
			{
				releaseProcLock();
				readErrorMex("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_THRSAFE_U);
			}
			updateAll();
			releaseProcLock();
#else
			readErrorMex("InvalidParamError", "Cannot change the state of thread safety for matshare compiled with thread safety turned off.");
#endif

		}
		else if(strcmp(param_str, MSH_PARAM_SECURITY_L) == 0)
		{
#ifdef MSH_UNIX
			if(vs_len < 3 || vs_len > 4)
			{
				readErrorMex("InvalidParamValueError", "Too many or too few digits in \"%s\" for parameter \"%s\". Must have either 3 or 4 digits.", val_str, MSH_PARAM_SECURITY_U);
			}
			else
			{
				acquireProcLock();
				shm_info->security = (mode_t)strtol(val_str_l, NULL, 8);
				if(fchmod(g_info->shm_info_seg.handle, shm_info->security) != 0)
				{
					readFchmodError(errno);
				}
				SegmentNode_t* curr_seg_node = g_info->seg_list.front;
				while(curr_seg_node != NULL)
				{
					if(fchmod(curr_seg_node->data_seg.handle, shm_info->security) != 0)
					{
						readFchmodError(errno);
					}
					curr_seg_node = curr_seg_node->next;
				}
#ifdef MSH_THREAD_SAFE
				if(fchmod(g_info->proc_lock, shm_info->security) != 0)
				{
					readFchmodError(errno);
				}
#endif
				updateAll();
				releaseProcLock();
			}
#else
			readErrorMex("InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", param_str);
#endif
		}
		else
		{
			readErrorMex("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}

		mxFree(param_str);
		mxFree(val_str);

	}
}


void updateAll(void)
{
	shm_info->update_pid = g_info->this_pid;
	shm_info->overwrite_info.seg_sz = g_info->seg_list.back->data_seg.seg_sz;
	
	SegmentNode_t* curr_seg_node = g_info->seg_list.front;
	while(curr_seg_node != NULL)
	{
#ifdef MSH_WIN
		/* not sure if this is required on windows, but it doesn't hurt */
		if(FlushViewOfFile(shm_info, g_info->shm_info_seg.seg_sz) == 0)
		{
			readErrorMex("FlushFileError", "Error flushing the update file (Error Number %u)", GetLastError());
		}
		if(FlushViewOfFile(curr_seg_node->data_seg.ptr, curr_seg_node->data_seg.seg_sz) == 0)
		{
			readErrorMex("FlushFileError", "Error flushing the data file (Error Number %u)", GetLastError());
		}
#else
		/* yes, this is required to ensure the changes are written (mmap creates a virtual address space)
		 * no, I don't know how this is possible without doubling the actual amount of RAM needed */
		if(msync(shm_info, g_info->shm_info_seg.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
		{
			releaseProcLock();
			readMsyncError(errno);
		}
		if(msync(curr_seg_node->data_seg.ptr, curr_seg_node->data_seg.seg_sz, MS_SYNC | MS_INVALIDATE) != 0)
		{
			releaseProcLock();
			readMsyncError(errno);
		}
#endif
		curr_seg_node = curr_seg_node->next;
	}
}


/* checks if everything is in working order before doing the operation */
bool_t precheck(void)
{
	bool_t ret = g_info->shm_info_seg.is_init & g_info->shm_info_seg.is_mapped;
#ifdef MSH_THREAD_SAFE
	return ret & g_info->flags.is_proc_lock_init;
#else
	return ret;
#endif
}


void removeUnused(void)
{
	VariableNode_t* curr_var_node = g_info->var_list.front,* prev_var_node,* next_var_node;
	SegmentNode_t* curr_seg_node,* prev_seg_node,* next_seg_node;
	
	while(curr_var_node != NULL)
	{
		next_var_node = curr_var_node->next;
		if(*curr_var_node->crosslink == NULL && curr_var_node->seg_node->data_seg.ptr->is_fetched)
		{

			prev_var_node = curr_var_node->prev;

			/* if there are no references to this variable do a destroy operation */
			if(!mxIsEmpty(curr_var_node->var))
			{
				/* NULL all of the Matlab pointers */
				shmDetach(curr_var_node->var);
			}
			mxDestroyArray(curr_var_node->var);
			
			curr_seg_node = curr_var_node->seg_node;
			next_seg_node = curr_seg_node->next;
			prev_seg_node = curr_seg_node->prev;
			
			bool_t will_detach = FALSE;
			if(curr_seg_node->data_seg.is_mapped)
			{
				if(curr_seg_node->data_seg.ptr->procs_using == 1)
				{
					/* if this is the last process using the memory, totally unlink */
					will_detach = TRUE;
					
					/* reset all references in shared memory */
					if(prev_seg_node != NULL)
					{
						prev_seg_node->data_seg.ptr->next_seg_num = curr_seg_node->data_seg.ptr->next_seg_num;
						prev_seg_node->next = next_seg_node;
					}
					
					if(next_seg_node != NULL)
					{
						next_seg_node->data_seg.ptr->prev_seg_num = curr_seg_node->data_seg.ptr->prev_seg_num;
						next_seg_node->prev = prev_seg_node;
					}
					
					if(g_info->seg_list.front == curr_seg_node)
					{
						g_info->seg_list.front = curr_seg_node->next;
					}
					
					curr_seg_node->data_seg.ptr->procs_tracking -= 1;
					
				}
				
				curr_seg_node->data_seg.ptr->procs_using -= 1;
				
#ifdef MSH_WIN
				if(UnmapViewOfFile(curr_seg_node->data_seg.ptr) == 0)
				{
					readErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
				}
#else
				if(munmap(curr_seg_node->data_seg.ptr, curr_seg_node->data_seg.seg_sz) != 0)
				{
					readMunmapError(errno);
				}
#endif

				curr_seg_node->data_seg.is_mapped = FALSE;
			}

			if(curr_seg_node->data_seg.is_init)
			{
#ifdef MSH_WIN
				if(CloseHandle(curr_seg_node->data_seg.handle) == 0)
				{
					readErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
				}
				
				if(will_detach)
				{
					shm_info->num_shared_vars -= 1;
				}
#else
				if(will_detach)
				{
					if(shm_unlink(curr_seg_node->data_seg.name) != 0)
					{
						readShmUnlinkError(errno);
					}
					shm_info->num_shared_vars -= 1;
				}
#endif
				curr_seg_node->data_seg.is_init = FALSE;
			}


			/* reset references in prev and next var node */
			if(prev_var_node != NULL)
			{
				prev_var_node->next = next_var_node;
			}

			if(next_var_node != NULL)
			{
				next_var_node->prev = prev_var_node;
			}

			if(g_info->var_list.front == curr_var_node)
			{
				g_info->var_list.front = next_var_node;
			}
			
			if(will_detach)
			{
				mxFree(curr_seg_node);
			}
			mxFree(curr_var_node);

		}

		curr_var_node = next_var_node;
		
	}
	
	if(shm_info->num_shared_vars == 0)
	{
		shm_info->first_seg_num = -1;
	}

}


void nullfcn(void)
{
	/* does nothing (so we can reset the mexAtExit function, since NULL is undocumented) */
}