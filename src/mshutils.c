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
const uint8_t MXMALLOC_SIGNATURE[MXMALLOC_SIG_LEN] = {16, 0, 0, 0, 0, 0, 0, 0, 206, 250, 237, 254, 32, 0, 32, 0};


size_t memCpyMex(byte_t* dest, byte_t* orig, byte_t** data_ptr, size_t cpy_sz)
{
	size_t offset = padToAlign(MXMALLOC_SIG_LEN);
	
	uint8_t mxmalloc_sig[MXMALLOC_SIG_LEN];
	makeMxMallocSignature(mxmalloc_sig, cpy_sz);
	
	*data_ptr = dest + offset;
	memcpy(*data_ptr - MXMALLOC_SIG_LEN, mxmalloc_sig, MXMALLOC_SIG_LEN);
	memcpy(*data_ptr, orig, cpy_sz);
	return offset + padToAlign(cpy_sz);
	
}


/* ------------------------------------------------------------------------- */
void freeTmp(data_t* dat)
{
	
	/* recurse to the bottom */
	if(dat->child_dat)
	{
		freeTmp(dat->child_dat);
	}
	
	/* free on the way back up */
	mxFree(dat->child_hdr);
	dat->child_hdr = (header_t*)NULL;
	dat->child_dat = (data_t*)NULL;
	dat->field_str = (char_t*)NULL;
}


/* field names of a structure									     */
size_t fieldNamesSize(const mxArray* mxStruct)
{
	const char_t* field_name_ptr;
	int num_fields;
	int i, j;               /* counters */
	size_t offset = 0;
	
	/* How many fields? */
	num_fields = mxGetNumberOfFields(mxStruct);
	
	/* Go through them */
	for(i = 0; i < num_fields; i++)
	{
		/* This field */
		field_name_ptr = mxGetFieldNameByNumber(mxStruct, i);
		j = 0;
		while(field_name_ptr[j])
		{
			j++;
		}
		j++;          /* for the null termination */
		
		if(i == (num_fields - 1))
		{
			j++;
		}     /* add this at the end for an end of string sequence since we use 0 elsewhere (TERM_CHAR). */
		
		
		/* Pad it out to 8 bytes */
		j = (int) padToAlign((size_t) j);
		
		/* keep the running tally */
		offset += j;
		
	}
	
	/* Add an extra alignment size to store the length of the string (in Bytes) */
	offset += ALIGN_SIZE;
	
	return offset;
	
}


/* returns the number of bytes used in pList					   */
size_t copyFieldNames(const mxArray* mxStruct, char_t* pList, const char_t** field_names)
{
	const char_t* field_name_ptr;    /* name of the field to add to the list */
	int num_fields;                    /* the number of fields */
	int i, j;                         /* counters */
	size_t offset = 0;                    /* number of bytes copied */
	
	/* How many fields? */
	num_fields = mxGetNumberOfFields(mxStruct);
	
	/* Go through them */
	for(i = 0; i < num_fields; i++)
	{
		/* This field */
		field_name_ptr = mxGetFieldNameByNumber(mxStruct, i);
		field_names[i] = field_name_ptr;
		j = 0;
		while(field_name_ptr[j])
		{
			pList[offset++] = field_name_ptr[j++];
		}
		pList[offset++] = 0; /* add the null termination */
		
		/* if this is last entry indicate the end of the string */
		if(i == (num_fields - 1))
		{
			pList[offset++] = TERM_CHAR;
			pList[offset++] = 0;  /* another null termination just to be safe */
		}
		
		/* Pad it out to 8 bytes */
		while(offset%ALIGN_SIZE)
		{
			pList[offset++] = 0;
		}
	}
	
	/* Add an extra alignment size to store the length of the string (in Bytes) */
	*(unsigned int*)&pList[offset] = (unsigned int)(offset + ALIGN_SIZE);
	offset += ALIGN_SIZE;
	
	return offset;
	
}


/*returns 0 if successful */
int pointCharArrayAtString(char_t** pCharArray, char_t* pString, int nFields)
{
	int i = 0;                             /* the index in pString we are up to */
	int field_num = 0;                   /* the field we are up to */
	
	/* and recover them */
	while(field_num < nFields)
	{
		/* scan past null termination chars */
		while(!pString[i])
		{
			i++;
		}
		
		/* Check the address is aligned (assume every forth bytes is aligned) */
		if(i%ALIGN_SIZE)
		{
			return -1;
		}
		
		/* grab the address */
		pCharArray[field_num] = &pString[i];
		field_num++;
		
		/* continue until a null termination char_t */
		while(pString[i])
		{
			i++;
		}
		
	}
	return 0;
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
	
	if(g_info->shm_data_reg.is_mapped)
	{
		UnmapViewOfFile(shm_data_ptr);
		g_info->shm_data_reg.is_mapped = FALSE;
	}
	
	if(g_info->shm_data_reg.is_init)
	{
		CloseHandle(g_info->shm_data_reg.handle);
		g_info->shm_data_reg.is_init = FALSE;
	}
	
	if(g_info->shm_update_reg.is_mapped)
	{
		
		acquireProcLock();
		shm_update_info->num_procs -= 1;
		UnmapViewOfFile(shm_update_info);
		releaseProcLock();
		
		g_info->shm_update_reg.is_mapped = FALSE;
	}
	
	if(g_info->shm_update_reg.is_init)
	{
		CloseHandle(g_info->shm_update_reg.handle);
		g_info->shm_update_reg.is_init = FALSE;
	}
	
	if(g_info->flags.is_proc_lock_init)
	{
		if(g_info->flags.is_proc_locked)
		{
			releaseProcLock();
		}
		CloseHandle(g_info->proc_lock);
		g_info->flags.is_proc_lock_init = FALSE;
	}
	
#else
	
	if(g_info->shm_data_reg.is_mapped)
	{
		if(munmap(shm_data_ptr, g_info->shm_data_reg.seg_sz) != 0)
		{
			readMunmapError(errno);
		}
		g_info->shm_data_reg.is_mapped = FALSE;
	}
	
	if(g_info->shm_data_reg.is_init)
	{
		if(shm_unlink(g_info->shm_data_reg.name) != 0)
		{
			readShmUnlinkError(errno);
		}
		g_info->shm_data_reg.is_init = FALSE;
	}
	
	if(g_info->shm_update_reg.is_mapped)
	{
		shm_update_info->num_procs -= 1;
		if(munmap(shm_update_info, g_info->shm_update_reg.seg_sz) != 0)
		{
			readMunmapError(errno);
		}
		g_info->shm_update_reg.is_mapped = FALSE;
	}
	
	if(g_info->shm_update_reg.is_init)
	{
		if(shm_unlink(g_info->shm_update_reg.name) != 0)
		{
			readShmUnlinkError(errno);
		}
		g_info->shm_update_reg.is_init = FALSE;
	}
	
	if(g_info->flags.is_proc_lock_init)
	{
		if(g_info->flags.is_proc_locked)
		{
			releaseProcLock();
		}
		
		
		if(shm_unlink(MSH_LOCK_NAME) != 0)
		{
			readShmUnlinkError(errno);
		}
		
//		if(sem_close(g_info->proc_lock) != 0)
//		{
//			readErrorMex("SemCloseInvalidError", "The sem argument is not a valid semaphore descriptor.");
//		}
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
	
	memcpy(sig, MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
	size_t multi = 1 << 4;
	
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	sig[0] = (uint8_t)((((seg_size + 0x0F)/multi) & (multi - 1))*multi);
	
	/* note: this only does bits 1 to 3 because of 64 bit precision limit (maybe implement bit 4 in the future?)*/
	for(int i = 1; i < 4; i++)
	{
		multi = (size_t)1 << (1 << (2 + i));
		sig[i] = (uint8_t)(((seg_size + 0x0F)/multi) & (multi - 1));
	}
	
}


void acquireProcLock(void)
{
	/* only request a lock if there is more than one process */
	if(shm_update_info->num_procs > 1 && g_info->flags.is_mem_safe)
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
		
//		if(sem_wait(g_info->proc_lock) != 0)
//		{
//			switch(errno)
//			{
//				case EINVAL:
//					readErrorMex("SemWaitInvalid", "The sem argument does not refer to a valid semaphore.");
//				case ENOSYS:
//					readErrorMex("SemWaitNotSupportedError", "The functions sem_wait() and sem_trywait() are not supported by this implementation.");
//				case EDEADLK:
//					readErrorMex("SemWaitDeadlockError", "A deadlock condition was detected.");
//				case EINTR:
//					readErrorMex("SemWaitInterruptError", "A signal interrupted this function.");
//				default:
//					readErrorMex("SemWaitUnknownError", "An unknown error occurred (Error number: %i)", errno);
//			}
//		}
#endif
		g_info->flags.is_proc_locked = TRUE;
	}
}


void releaseProcLock(void)
{
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
		
		
//		if(sem_post(g_info->proc_lock) != 0)
//		{
//			switch(errno)
//			{
//				case EINVAL:
//					readErrorMex("SemPostInvalid", "The sem argument does not refer to a valid semaphore.");
//				case ENOSYS:
//					readErrorMex("SemPostError", "The function sem_post() is not supported by this implementation.");
//				default:
//					readErrorMex("SemPostUnknownError", "An unknown error occurred (Error number: %i)", errno);
//			}
//		}
#endif
		g_info->flags.is_proc_locked = FALSE;
	}
}


msh_directive_t parseDirective(const mxArray* in)
{
	
	if(mxIsNumeric(in))
	{
		return (msh_directive_t)*((uint8_t*)(mxGetData(in)));
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


void updateAll(void)
{
	shm_update_info->upd_pid = g_info->this_pid;
	shm_update_info->rev_num = g_info->cur_seg_info.rev_num;
	shm_update_info->seg_num = g_info->cur_seg_info.seg_num;
	shm_update_info->seg_sz = g_info->shm_data_reg.seg_sz;
#ifdef MSH_WIN
	/* not sure if this is required on windows, but it doesn't hurt */
	FlushViewOfFile(shm_update_info, g_info->shm_update_reg.seg_sz);
	FlushViewOfFile(shm_data_ptr, g_info->shm_data_reg.seg_sz);
#else
	/* yes, this is required to ensure the changes are written (mmap creates a virtual address space)
	 * no, I don't know how this is possible without doubling the actual amount of RAM needed */
	if(msync(shm_update_info, g_info->shm_update_reg.seg_sz, MS_SYNC|MS_INVALIDATE) != 0)
	{
		releaseProcLock();
		readMsyncError(errno);
	}
	if(msync(shm_data_ptr, g_info->shm_data_reg.seg_sz, MS_SYNC|MS_INVALIDATE) != 0)
	{
		releaseProcLock();
		readMsyncError(errno);
	}
#endif
}


/* checks if everything is in working order before doing the operation */
bool_t precheck(void)
{
	return g_info->flags.is_proc_lock_init
			& g_info->shm_update_reg.is_init
			& g_info->shm_update_reg.is_mapped
			& g_info->shm_data_reg.is_init
			& g_info->shm_data_reg.is_mapped
			& g_info->flags.is_glob_shm_var_init;
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
	// does nothing
}