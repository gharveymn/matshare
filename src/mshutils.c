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
	
	if(g_info->shm_info_seg.is_mapped && shm_info->sharetype == msh_SHARETYPE_COPY)
	{
		mshUpdateSegments();
	}
	
	if(g_info->flags.is_proc_lock_init)
	{
		acquireProcLock();
	}
	
	CleanVariableList(&g_var_list);
	
	CleanSegmentList(&g_seg_list);

#ifdef MSH_WIN
	
	
	if(g_info->shm_info_seg.is_mapped)
	{
		shm_info->num_procs -= 1;
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
	
	if(g_info->shm_info_seg.is_mapped)
	{
		if(munmap(shm_info, g_info->shm_info_seg.seg_sz) != 0)
		{
			if(g_info->flags.is_proc_lock_init)
			{
				releaseProcLock();
			}
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
				if(g_info->flags.is_proc_lock_init)
				{
					releaseProcLock();
				}
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
			if(g_info->flags.is_proc_lock_init)
			{
				releaseProcLock();
			}
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
	mexAtExit(NullFunction);
	
	if(mexIsLocked())
	{
		mexUnlock();
	}
	
}


size_t padToAlign(size_t size)
{
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	return size + ALIGN_SIZE - (size & (ALIGN_SIZE - 1));
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
	size_t i, j, ps_len, vs_len;
	char* param_str, * val_str;
	char param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
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
		vs_len = mxGetNumberOfElements(val);
		
		if(ps_len >= MSH_MAX_NAME_LEN)
		{
			readErrorMex("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		else if(vs_len >= MSH_MAX_NAME_LEN)
		{
			readErrorMex("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, param_str);
		}
		
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
			acquireProcLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				shm_info->is_thread_safe = TRUE;
			}
			if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
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
				SegmentNode_t* curr_seg_node = g_seg_list.first;
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
	
	SegmentNode_t* curr_seg_node = g_seg_list.first;
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


void RemoveUnusedVariables(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node, * next_var_node;
	
	if(var_list != NULL)
	{
		curr_var_node = var_list->first;
		while(curr_var_node != NULL)
		{
			next_var_node = curr_var_node->next;
			if(*curr_var_node->crosslink == NULL && curr_var_node->seg_node->data_seg.ptr->is_used)
			{
				RemoveVariable(var_list, curr_var_node);
			}
			curr_var_node = next_var_node;
		}
		
		if(shm_info->num_shared_vars == 0)
		{
			shm_info->first_seg_num = -1;
		}
	}
	
}


SegmentNode_t* CreateSegment(const size_t seg_sz)
{
#ifdef MSH_WIN
	DWORD hi_sz, lo_sz, err = 0;
	HANDLE temp_handle;
#else
	handle_t temp_handle;
	int err = 0;
#endif
	
	SegmentNode_t* new_seg_node = mxCalloc(1, sizeof(SegmentNode_t));
	mexMakeMemoryPersistent(new_seg_node);
	new_seg_node->prev_seg_num = -1;
	new_seg_node->next_seg_num = -1;
	
	/* create a unique new segment */
	new_seg_node->seg_num = (shm_info->lead_seg_num == INT_MAX)? 0 : shm_info->lead_seg_num + 1;
	
	/* set the size */
	new_seg_node->data_seg.seg_sz = seg_sz;

#ifdef MSH_WIN
	
	/* split the 64-bit size */
	lo_sz = (DWORD) (new_seg_node->data_seg.seg_sz & 0xFFFFFFFFL);
	hi_sz = (DWORD) ((new_seg_node->data_seg.seg_sz >> 32u) & 0xFFFFFFFFL);
	
	do
	{
		/* change the file name */
		err = 0;
		snprintf(new_seg_node->data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME,
			    (unsigned long long) new_seg_node->seg_num);
		temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz,
								  new_seg_node->data_seg.name);
		err = GetLastError();
		if(temp_handle == NULL)
		{
			releaseProcLock();
			readErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
		}
		else if(err == ERROR_ALREADY_EXISTS)
		{
			new_seg_node->seg_num = (new_seg_node->seg_num == INT_MAX)? 0 : new_seg_node->seg_num + 1;
			if(CloseHandle(temp_handle) == 0)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("CloseHandleError", "Error closing the file handle (Error Number %u).", err);
			}
		}
	} while(err == ERROR_ALREADY_EXISTS);
	new_seg_node->data_seg.handle = temp_handle;
	new_seg_node->data_seg.is_init = TRUE;
	
	new_seg_node->data_seg.ptr = MapViewOfFile(new_seg_node->data_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0,
									   new_seg_node->data_seg.seg_sz);
	if(new_seg_node->data_seg.ptr == NULL)
	{
		err = GetLastError();
		releaseProcLock();
		readErrorMex("MapDataSegError", "Could not map the data memory segment (Error number %u)", err);
	}
	new_seg_node->data_seg.is_mapped = TRUE;

#else
	
	/* find an available shared memory file name */
	do
	{
		/* change the file name */
		err = 0;
		snprintf(new_seg_node->data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, (unsigned long long)new_seg_node->seg_num);
		temp_handle = shm_open(new_seg_node->data_seg.name, O_RDWR | O_CREAT | O_EXCL, shm_info->security);
		if(temp_handle == -1)
		{
			err = errno;
			if(err == EEXIST)
			{
				new_seg_node->seg_num = (new_seg_node->seg_num == INT_MAX)? 0 : new_seg_node->seg_num + 1;
			}
			else
			{
				releaseProcLock();
				readShmOpenError(err);
			}
		}
	} while(err == EEXIST);
	new_seg_node->data_seg.handle = temp_handle;
	new_seg_node->data_seg.is_init = TRUE;
	
	/* change the map size */
	if(ftruncate(new_seg_node->data_seg.handle, new_seg_node->data_seg.seg_sz) != 0)
	{
		releaseProcLock();
		readFtruncateError(errno);
	}
	
	/* map the shared memory */
	new_seg_node->data_seg.ptr = mmap(NULL, new_seg_node->data_seg.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, new_seg_node->data_seg.handle, 0);
	if(new_seg_node->data_seg.ptr == MAP_FAILED)
	{
		releaseProcLock();
		readMmapError(errno);
	}
	new_seg_node->data_seg.is_mapped = TRUE;

#endif
	
	/*update the leading segment number */
	shm_info->lead_seg_num = new_seg_node->seg_num;
	
	/* update number of vars in shared memory */
	shm_info->num_shared_vars += 1;
	
	/* update the revision number to indicate to retrieve new segments */
	shm_info->rev_num += 1;
	g_info->rev_num = shm_info->rev_num;
	
	return new_seg_node;
	
}


SegmentNode_t* OpenSegment(const signed long seg_num)
{

#ifdef MSH_WIN
	DWORD err;
#else
	int err;
#endif
	
	SegmentMetadata_t* temp_map;
	
	/* if this has not been fetched yet make a new var node with a new map */
	SegmentNode_t* new_seg_node = mxCalloc(1, sizeof(SegmentNode_t));
	mexMakeMemoryPersistent(new_seg_node);
	new_seg_node->seg_num = seg_num;
	
	/* update the region name */
	snprintf(new_seg_node->data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, (unsigned long long)new_seg_node->seg_num);

#ifdef MSH_WIN
	/* get the new file handle */
			new_seg_node->data_seg.handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE,
												   new_seg_node->data_seg.name);
			if(new_seg_node->data_seg.handle == NULL)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("OpenFileError", "Error opening the file mapping (Error Number %u)", err);
			}
			new_seg_node->data_seg.is_init = TRUE;
			
			
			/* map the metadata to get the size of the segment */
			temp_map = MapViewOfFile(new_seg_node->data_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0,
								sizeof(SegmentMetadata_t));
			if(temp_map == NULL)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("MappingError", "Could not fetch the memory segment (Error number: %d).",
						   err);
			}
			else
			{
				/* get the segment size */
				new_seg_node->data_seg.seg_sz = temp_map->seg_sz;
			}
			
			/* unmap the temporary view */
			if(UnmapViewOfFile(temp_map) == 0)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("UnmapFileError", "Error unmapping the file (Error Number %u)", err);
			}
			
			/* now map the whole thing */
			new_seg_node->data_seg.ptr = MapViewOfFile(new_seg_node->data_seg.handle,
											   FILE_MAP_ALL_ACCESS, 0, 0,
											   new_seg_node->data_seg.seg_sz);
			if(new_seg_node->data_seg.ptr == NULL)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("MappingError", "Could not fetch the memory segment (Error number: %d).",
						   err);
			}
			new_seg_node->data_seg.is_mapped = TRUE;

#else
	/* get the new file handle */
	new_seg_node->data_seg.handle = shm_open(new_seg_node->data_seg.name, O_RDWR, shm_info->security);
	if(new_seg_node->data_seg.handle == -1)
	{
		err = errno;
		releaseProcLock();
		readShmOpenError(err);
	}
	new_seg_node->data_seg.is_init = TRUE;
	
	/* map the metadata to get the size of the segment */
	temp_map = mmap(NULL, sizeof(SegmentMetadata_t), PROT_READ | PROT_WRITE, MAP_SHARED, new_seg_node->data_seg.handle, 0);
	if(temp_map == MAP_FAILED)
	{
		err = errno;
		releaseProcLock();
		readMmapError(err);
	}
	
	/* get the segment size */
	new_seg_node->data_seg.seg_sz = temp_map->seg_sz;
	
	/* unmap the temporary map */
	if(munmap(temp_map, sizeof(SegmentMetadata_t)) != 0)
	{
		err = errno;
		releaseProcLock();
		readMunmapError(err);
	}
	
	/* now map the whole thing */
	new_seg_node->data_seg.ptr = mmap(NULL, new_seg_node->data_seg.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, new_seg_node->data_seg.handle, 0);
	if(new_seg_node->data_seg.ptr == MAP_FAILED)
	{
		err = errno;
		releaseProcLock();
		readMmapError(err);
	}
	new_seg_node->data_seg.is_mapped = TRUE;
#endif
	
	new_seg_node->prev_seg_num = new_seg_node->data_seg.ptr->prev_seg_num;
	new_seg_node->next_seg_num = new_seg_node->data_seg.ptr->next_seg_num;
	
	new_seg_node->data_seg.ptr->procs_tracking += 1;
	
	return new_seg_node;
}


void AddSegment(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	if(seg_list != NULL && seg_node != NULL)
	{
		/* make sure we're up to date first */
		if(seg_list == &g_seg_list)
		{
			mshUpdateSegments();
		}
		
		seg_node->parent_seg_list = seg_list;
		
		/* this will be appended to the end so make sure next points to nothing */
		seg_node->next = NULL;
		seg_node->next_seg_num = -1;
		
		/* always points to the end */
		seg_node->prev = seg_list->last;
		
		/* set new refs */
		if(seg_list->num_segs != 0)
		{
			/* set the relational segment numbers */
			seg_list->last->next_seg_num = seg_node->seg_num;
			seg_node->prev_seg_num = seg_list->last->seg_num;
			
			/* set list pointer */
			seg_list->last->next = seg_node;
		}
		else
		{
			/* if the last is NULL then the list is empty */
			seg_list->first = seg_node;
			
			/* make sure these are set properly */
			seg_node->prev_seg_num = -1;
			
			/* set the first segment number to this */
			shm_info->first_seg_num = seg_node->seg_num;
		}
		
		/* place this variable at the last of the list */
		seg_list->last = seg_node;
		
		/* increment number of segments */
		seg_list->num_segs += 1;
		
	}
	
}


void RemoveSegment(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	
	bool_t will_unlink = FALSE;
	
	if(seg_list != NULL && seg_node != NULL)
	{
		
		if(seg_list == &g_seg_list)
		{
			mshUpdateSegments();
		}
		
		if(seg_node->var_node != NULL)
		{
			RemoveVariable(seg_node->var_node->parent_var_list, seg_node->var_node);
		}
		
		if(seg_list->first == seg_node)
		{
			seg_list->first = seg_node->next;
			if(seg_list->first != NULL)
			{
				shm_info->first_seg_num = seg_list->first->seg_num;
			}
			else
			{
				shm_info->first_seg_num = -1;
			}
		}
		
		if(seg_list->last == seg_node)
		{
			seg_list->last = seg_node->prev;
		}
		
		/* reset local pointers */
		if(seg_node->prev != NULL)
		{
			if(seg_node->prev->data_seg.is_mapped)
			{
				seg_node->prev->data_seg.ptr->next_seg_num = seg_node->next_seg_num;
			}
			seg_node->prev->next = seg_node->next;
		}
		
		if(seg_node->next != NULL)
		{
			if(seg_node->next->data_seg.is_mapped)
			{
				seg_node->next->data_seg.ptr->prev_seg_num = seg_node->prev_seg_num;
			}
			seg_node->next->prev = seg_node->prev;
		}
		
		/* decrement the number of segments now in case we crash when doing the unmapping */
		seg_list->num_segs -= 1;
		
		if(seg_node->data_seg.is_mapped)
		{
			seg_node->data_seg.ptr->procs_tracking -= 1;
			
			/* check if this process will unlink the shared memory (for linux) */
			will_unlink = (bool_t)(seg_node->data_seg.ptr->procs_tracking == 0);

#ifdef MSH_WIN
			if(UnmapViewOfFile(seg_node->data_seg.ptr) == 0)
			{
				readErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
			}
#else
			if(munmap(seg_node->data_seg.ptr, seg_node->data_seg.seg_sz) != 0)
			{
				readMunmapError(errno);
			}
#endif
			
			seg_node->data_seg.is_mapped = FALSE;
		}
		
		if(seg_node->data_seg.is_init)
		{
#ifdef MSH_WIN
			if(CloseHandle(seg_node->data_seg.handle) == 0)
			{
				readErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
			}
#else
			if(will_unlink)
			{
				if(shm_unlink(seg_node->data_seg.name) != 0)
				{
					readShmUnlinkError(errno);
				}
			}
#endif
			seg_node->data_seg.is_init = FALSE;
		}
		
		mxFree(seg_node);
	}
	
}


VariableNode_t* CreateVariable(SegmentNode_t* seg_node)
{
	VariableNode_t* new_var_node = mxCalloc(1, sizeof(VariableNode_t));
	mexMakeMemoryPersistent(new_var_node);
	
	new_var_node->seg_node = seg_node;
	shmFetch((byte_t*)seg_node->data_seg.ptr, &new_var_node->var);
	new_var_node->crosslink = &((mxArrayStruct*)new_var_node->var)->CrossLink;
	seg_node->data_seg.ptr->is_used = TRUE;
	
	seg_node->var_node = new_var_node;
	
	return new_var_node;
}


void AddVariable(VariableList_t* var_list, VariableNode_t* var_node)
{
	if(var_list != NULL && var_node != NULL)
	{
		
		var_node->parent_var_list = var_list;
		
		var_node->next = NULL;
		var_node->prev = var_list->last;
		
		if(var_list->num_vars != 0)
		{
			/* set pointer in the last node to this new node */
			var_list->last->next = var_node;
		}
		else
		{
			/* set the front to this node */
			var_list->first = var_node;
		}
		
		/* append to the end of the list */
		var_list->last = var_node;
		
		/* increment number of variables */
		var_list->num_vars += 1;
		
	}
	
}


void RemoveVariable(VariableList_t* var_list, VariableNode_t* var_node)
{
	
	if(var_list!= NULL && var_node != NULL)
	{
		
		if(!mxIsEmpty(var_node->var))
		{
			/* NULL all of the Matlab pointers */
			shmDetach(var_node->var);
		}
		mxDestroyArray(var_node->var);
		
		
		/* reset references in prev and next var node */
		if(var_node->prev != NULL)
		{
			var_node->prev->next = var_node->next;
		}
		
		if(var_node->next != NULL)
		{
			var_node->next->prev = var_node->prev;
		}
		
		if(var_list->first == var_node)
		{
			var_list->first = var_node->next;
		}
		
		if(var_list->last == var_node)
		{
			var_list->last = var_node->prev;
		}
		
		/* decrement number of variables in the list */
		var_list->num_vars -= 1;
		
		/* decrement number of processes tracking this variable */
		var_node->seg_node->data_seg.ptr->procs_using -= 1;
		
		if(var_node->seg_node->data_seg.ptr->procs_using == 0)
		{
			
			if(var_list == &g_var_list)
			{
				mshUpdateSegments();
			}
			
			RemoveSegment(var_node->seg_node->parent_seg_list, var_node->seg_node);
			
			/* this should only fire once when the number of procs using hits 0*/
			shm_info->num_shared_vars -= 1;
		}
		
		var_node->seg_node->var_node = NULL;
		
		mxFree(var_node);
		
	}
}


void CleanVariableList(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node,* next_var_node;
	if(var_list != NULL)
	{
		acquireProcLock();
		curr_var_node = var_list->first;
		while(curr_var_node != NULL)
		{
			next_var_node = curr_var_node->next;
			RemoveVariable(var_list, curr_var_node);
			curr_var_node = next_var_node;
		}
		releaseProcLock();
	}
}


void CleanSegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node,* next_seg_node;
	if(seg_list != NULL)
	{
		acquireProcLock();
		curr_seg_node = seg_list->first;
		while(curr_seg_node != NULL)
		{
			next_seg_node = curr_seg_node->next;
			RemoveSegment(seg_list, curr_seg_node);
			curr_seg_node = next_seg_node;
		}
		releaseProcLock();
	}
}


void NullFunction(void)
{
	/* does nothing (so we can reset the mexAtExit function, since NULL is undocumented) */
}