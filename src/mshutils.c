#include "headers/mshutils.h"

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
static const uint8_t c_MXMALLOC_SIGNATURE[MXMALLOC_SIG_LEN] = {16, 0, 0, 0, 0, 0, 0, 0, 206, 250, 237, 254, 32, 0, 32, 0};

void* memCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz)
{
	uint8_t mxmalloc_sig[MXMALLOC_SIG_LEN];
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
	data_ptrs->dims = (mwSize*)(shm_anchor + hdr->data_offsets.dims);
	data_ptrs->pr = shm_anchor + hdr->data_offsets.pr;
	data_ptrs->pi = shm_anchor + hdr->data_offsets.pi;
	data_ptrs->ir = (mwIndex*)(shm_anchor + hdr->data_offsets.ir);
	data_ptrs->jc = (mwIndex*)(shm_anchor + hdr->data_offsets.jc);
	data_ptrs->field_str = shm_anchor + hdr->data_offsets.field_str;
	data_ptrs->child_hdrs = (size_t*)(shm_anchor + hdr->data_offsets.child_hdrs);
}


/* field names of a structure									     */
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
	
	if(g_info->flags.is_glob_shm_var_init)
	{
		if(!mxIsEmpty(g_shm_var))
		{
			/* NULL all of the Matlab pointers */
			shmDetach(g_shm_var);
		}
		mxDestroyArray(g_shm_var);
	}

#ifdef MSH_WIN

#ifdef MSH_THREAD_SAFE
	if(g_info->flags.is_proc_lock_init)
	{
		acquireProcLock();
		if(g_info->shm_update_seg.is_mapped)
		{
			shm_update_info->num_procs -= 1;
		}
		releaseProcLock();
		
		if(CloseHandle(g_info->proc_lock) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the process lock handle (Error Number %u)", GetLastError());
		}
		g_info->flags.is_proc_lock_init = FALSE;
		
	}
#else
	if(g_info->shm_update_seg.is_mapped)
	{
		shm_update_info->num_procs -= 1;
	}
#endif
	
	if(g_info->shm_data_seg.is_mapped)
	{
		if(UnmapViewOfFile(shm_data_ptr) == 0)
		{
			readErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
		}
		g_info->shm_data_seg.is_mapped = FALSE;
	}
	
	if(g_info->shm_data_seg.is_init)
	{
		if(CloseHandle(g_info->shm_data_seg.handle) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
		}
		g_info->shm_data_seg.is_init = FALSE;
	}
	
	if(g_info->swap_shm_data_seg.is_mapped)
	{
		if(UnmapViewOfFile(g_info->swap_shm_data_seg.ptr) == 0)
		{
			readErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
		}
		g_info->swap_shm_data_seg.is_mapped = FALSE;
	}
	
	if(g_info->swap_shm_data_seg.is_init)
	{
		if(CloseHandle(g_info->swap_shm_data_seg.handle) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
		}
		g_info->swap_shm_data_seg.is_init = FALSE;
	}
	
	if(g_info->shm_update_seg.is_mapped)
	{
		if(UnmapViewOfFile(shm_update_info) == 0)
		{
			readErrorMex("UnmapFileError", "Error unmapping the update file (Error Number %u)", GetLastError());
		}
		g_info->shm_update_seg.is_mapped = FALSE;
	}
	
	if(g_info->shm_update_seg.is_init)
	{
		if(CloseHandle(g_info->shm_update_seg.handle) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the update file handle (Error Number %u)", GetLastError());
		}
		g_info->shm_update_seg.is_init = FALSE;
	}
	
#ifdef MSH_AUTO_INIT
	if(g_info->lcl_init_seg.is_init)
	{
		if(CloseHandle(g_info->lcl_init_seg.handle) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
		}
		g_info->lcl_init_seg.is_init = FALSE;
	}
#endif

#else
	
	bool_t will_remove = FALSE;
	
#ifdef MSH_THREAD_SAFE
	if(g_info->flags.is_proc_lock_init)
	{
		acquireProcLock();
		if(g_info->shm_update_seg.is_mapped)
		{
			shm_update_info->num_procs -= 1;
			will_remove = (bool_t)(shm_update_info->num_procs == 0);
		}
		releaseProcLock();
		
		if(will_remove)
		{
			if(shm_unlink(MSH_LOCK_NAME) != 0)
			{
				readShmUnlinkError(errno);
			}
		}
		
	}
#else
	if(g_info->shm_update_seg.is_mapped)
	{
		shm_update_info->num_procs -= 1;
		will_remove = (bool_t)(shm_update_info->num_procs == 0);
	}
#endif
	
	
	if(g_info->shm_data_seg.is_mapped)
	{
		if(munmap(shm_data_ptr, g_info->shm_data_seg.seg_sz) != 0)
		{
			readMunmapError(errno);
		}
		g_info->shm_data_seg.is_mapped = FALSE;
	}
	
	if(g_info->shm_data_seg.is_init)
	{
		if(will_remove)
		{
			if(shm_unlink(g_info->shm_data_seg.name) != 0)
			{
				readShmUnlinkError(errno);
			}
		}
		g_info->shm_data_seg.is_init = FALSE;
	}
	
	if(g_info->shm_update_seg.is_mapped)
	{
		if(munmap(shm_update_info, g_info->shm_update_seg.seg_sz) != 0)
		{
			readMunmapError(errno);
		}
		g_info->shm_update_seg.is_mapped = FALSE;
	}
	
	if(g_info->shm_update_seg.is_init)
	{
		if(will_remove)
		{
			if(shm_unlink(g_info->shm_update_seg.name) != 0)
			{
				readShmUnlinkError(errno);
			}
		}
		g_info->shm_update_seg.is_init = FALSE;
	}
	
#ifdef MSH_AUTO_INIT
	if(g_info->lcl_init_seg.is_init)
	{
		if(shm_unlink(g_info->lcl_init_seg.name) != 0)
		{
			readShmUnlinkError(errno);
		}
		g_info->lcl_init_seg.is_init = FALSE;
	}
#endif

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


void makeMxMallocSignature(uint8_t* sig, size_t seg_size)
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
	
	memcpy(sig, c_MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
	size_t multi = 1 << 4;
	
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	if(seg_size > 0)
	{
		sig[0] = (uint8_t)((((seg_size + 0x0F)/multi) & (multi - 1))*multi);
		
		/* note: this only does bits 1 to 3 because of 64 bit precision limit (maybe implement bit 4 in the future?)*/
		for(int i = 1; i < 4; i++)
		{
			multi = (size_t)1 << (1 << (2 + i));
			sig[i] = (uint8_t)(((seg_size + 0x0F)/multi) & (multi - 1));
		}
	}
}


void acquireProcLock(void)
{
#ifdef MSH_THREAD_SAFE
	/* only request a lock if there is more than one process */
	if(shm_update_info->num_procs > 1 && shm_update_info->is_thread_safe && !g_info->flags.is_proc_locked)
	{
#ifdef MSH_WIN
		uint32_t ret = WaitForSingleObject(g_info->proc_lock, INFINITE);
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
		return (mshdirective_t)*((uint8_t*)(mxGetData(in)));
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
		else if(strcmp(dir_str, "shmCopy") == 0)
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
				shm_update_info->is_thread_safe = TRUE;
			}
			if(strcmp(val_str_l, "false") == 0
			   || strcmp(val_str_l, "off") == 0
			   || strcmp(val_str_l, "disable") == 0)
			{
				shm_update_info->is_thread_safe = FALSE;
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
				shm_update_info->security = (mode_t)strtol(val_str_l, NULL, 8);
				if(fchmod(g_info->shm_update_seg.handle, shm_update_info->security) != 0)
				{
					readFchmodError(errno);
				}
				if(fchmod(g_info->shm_data_seg.handle, shm_update_info->security) != 0)
				{
					readFchmodError(errno);
				}
#ifdef MSH_THREAD_SAFE
				if(fchmod(g_info->proc_lock, shm_update_info->security) != 0)
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
	shm_update_info->upd_pid = g_info->this_pid;
	shm_update_info->rev_num = g_info->cur_seg_info.rev_num;
	shm_update_info->seg_num = g_info->cur_seg_info.seg_num;
	shm_update_info->seg_sz = g_info->shm_data_seg.seg_sz;
#ifdef MSH_WIN
	/* not sure if this is required on windows, but it doesn't hurt */
	if(FlushViewOfFile(shm_update_info, g_info->shm_update_seg.seg_sz) == 0)
	{
		readErrorMex("FlushFileError", "Error flushing the update file (Error Number %u)", GetLastError());
	}
	if(FlushViewOfFile(shm_data_ptr, g_info->shm_data_seg.seg_sz) == 0)
	{
		readErrorMex("FlushFileError", "Error flushing the data file (Error Number %u)", GetLastError());
	}
#else
	/* yes, this is required to ensure the changes are written (mmap creates a virtual address space)
	 * no, I don't know how this is possible without doubling the actual amount of RAM needed */
	if(msync(shm_update_info, g_info->shm_update_seg.seg_sz, MS_SYNC|MS_INVALIDATE) != 0)
	{
		releaseProcLock();
		readMsyncError(errno);
	}
	if(msync(shm_data_ptr, g_info->shm_data_seg.seg_sz, MS_SYNC|MS_INVALIDATE) != 0)
	{
		releaseProcLock();
		readMsyncError(errno);
	}
#endif
}


/* checks if everything is in working order before doing the operation */
bool_t precheck(void)
{
	bool_t ret = g_info->shm_update_seg.is_init
			   & g_info->shm_update_seg.is_mapped
			   & g_info->shm_data_seg.is_init
			   & g_info->shm_data_seg.is_mapped
			   & g_info->flags.is_glob_shm_var_init;
#ifdef MSH_THREAD_SAFE
	return ret & g_info->flags.is_proc_lock_init;
#else
	return ret;
#endif
}


void makeDummyVar(mxArray** out)
{
	g_shm_var = mxCreateDoubleMatrix(0, 0, mxREAL);
	mexMakeArrayPersistent(g_shm_var);
	*out = mxCreateSharedDataCopy(g_shm_var);
	g_info->flags.is_glob_shm_var_init = TRUE;
}


void nullfcn(void)
{
	/* does nothing (so we can reset the mexAtExit function, since NULL is undocumented) */
}