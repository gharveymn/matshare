#include "headers/init.h"
#include "headers/msh_types.h"


static bool_t is_glob_init;
static bool_t is_proc_init;

void init(bool_t is_mem_safe)
{
	
	header_t hdr;
	
	//assume not
	is_glob_init = FALSE;
	is_proc_init = FALSE;
	
	/* find out if this has been initialized yet in this process */
	procStartup();
	glob_info->flags.is_mem_safe = is_mem_safe;

	if(!glob_info->flags.is_proc_lock_init)
	{
		initProcLock();
	}
	
	if(!glob_info->shm_update_reg.is_init)
	{
		initUpdateSegment();
	}
	
	if(!glob_info->shm_update_reg.is_mapped)
	{
		mapUpdateSegment();
	}
	
	/* only actually locks if this is memory safe */
	acquireProcLock();
	globStartup(&hdr);
	
	if(!glob_info->shm_data_reg.is_init)
	{
		initDataSegment();
	}
	
	if(!glob_info->shm_data_reg.is_mapped)
	{
		mapDataSegment();
	}
	
	if(is_glob_init)
	{
		memcpy(shm_data_ptr, &hdr, hdr.shmsiz);
	}
	
	glob_info->cur_seg_info.rev_num = shm_update_info->rev_num;
	glob_info->cur_seg_info.seg_num = shm_update_info->seg_num;
	glob_info->cur_seg_info.seg_sz = shm_update_info->seg_sz;
	
	glob_info->flags.is_freed = FALSE;
	
	if(is_proc_init)
	{
		if(!glob_info->flags.is_glob_shm_var_init)
		{
			
			if(is_glob_init)
			{
				glob_shm_var = mxCreateDoubleMatrix(0, 0, mxREAL);
			}
			else
			{
				/* fetch the data rather than create a dummy variable */
				shallowfetch(shm_data_ptr, &glob_shm_var);
			}
			mexMakeArrayPersistent(glob_shm_var);
			glob_info->flags.is_glob_shm_var_init = TRUE;
		}
		else
		{
			readMXError("ShmVarInitError", "The globally shared variable was unexpectedly already initialized before startup.");
		}
	}
	else
	{
		if(!glob_info->flags.is_glob_shm_var_init)
		{
			readMXError("ShmVarInitError", "The globally shared variable was unexpectedly not initialized after startup.");
		}
	}
	
	mexAtExit(onExit);
	releaseProcLock();
	
}


void procStartup(void)
{
	mex_info* temp_info = mxCalloc(1, sizeof(mex_info));

	/* if MatShare fails here then we have a critical error */
	
	temp_info->startup_flag.reg_sz = MSH_MAX_NAME_LEN;
	
#ifdef MSH_WIN
	
	temp_info->this_pid = GetProcessId(GetCurrentProcess());
	
	sprintf(temp_info->startup_flag.name, MATSHARE_STARTUP_FLAG_PRENAME, temp_info->this_pid);
	temp_info->startup_flag.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MSH_STARTUP_SIG), temp_info->startup_flag.name);
	DWORD err = GetLastError();
	if(temp_info->startup_flag.handle == NULL)
	{
		mxFree(temp_info);
		readMXError("MshStartupFlagError", "Could not create or open the update memory segment (Error number: %u).", err);
	}
	temp_info->startup_flag.is_init = TRUE;

	temp_info->startup_flag.ptr = MapViewOfFile(temp_info->startup_flag.handle, FILE_MAP_ALL_ACCESS, 0, 0, temp_info->startup_flag.reg_sz);
	if(temp_info->startup_flag.ptr == NULL)
	{
		readMXError("MapStartupFlagError", "Could not map the startup memory segment (Error number %u)", GetLastError());
	}
	temp_info->startup_flag.is_mapped = TRUE;
	
	if(err == ERROR_ALREADY_EXISTS)
	{
		/* this may only exist uncleared in the namespace so we have to check the file contents */
		
		if(memcmp(temp_info->startup_flag.ptr, MSH_STARTUP_SIG, sizeof(MSH_STARTUP_SIG)) == 0)
		{
			/* then this has already been started */
			is_proc_init = FALSE;
		}
		else
		{
			/* the file still existed in the namespace, but was not cleared when closed */
			is_proc_init = TRUE;
		}
	
	}
	else
	{
		/* we definitely will initialize in this case*/
		is_proc_init = TRUE;
	}

#else
	
	/* Try to open an already created segment so we can get the proc init signal */
	temp_info->this_pid = getpid();
	
	sprintf(temp_info->startup_flag.name, MATSHARE_STARTUP_FLAG_PRENAME, (unsigned long)temp_info->this_pid);
	temp_info->startup_flag.handle = shm_open(temp_info->startup_flag.name, O_RDWR|O_CREAT|O_EXCL, S_IRWXU);
	if(temp_info->startup_flag.handle == -1)
	{
		
		/* already exists */
		
		is_proc_init = FALSE;
		if(errno != EEXIST)
		{
			readShmError(errno);
		}
		
		temp_info->startup_flag.handle = shm_open(temp_info->startup_flag.name, O_RDWR, S_IRWXU);
		if(temp_info->startup_flag.handle == -1)
		{
			readShmError(errno);
		}
		
	}
	else
	{
		
		temp_info->startup_flag.is_init = TRUE;
		if(ftruncate(temp_info->startup_flag.handle, temp_info->startup_flag.reg_sz) != 0)
		{
			readFtruncateError(errno);
		}
		temp_info->startup_flag.is_init = TRUE;
		
		is_proc_init = TRUE;
		
	}
	
	temp_info->startup_flag.ptr = mmap(NULL, temp_info->startup_flag.reg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, temp_info->startup_flag.handle, 0);
	if(temp_info->startup_flag.ptr == MAP_FAILED)
	{
		readMmapError(errno);
	}
	temp_info->startup_flag.is_mapped = TRUE;
	
	if(is_proc_init == FALSE)
	{
		/* Check if this is really the first one or the file was hanging around in the namespace */
		if(memcmp(temp_info->startup_flag.ptr, MSH_STARTUP_SIG, sizeof(MSH_STARTUP_SIG)) == 0)
		{
			/* then this has already been started */
			is_proc_init = FALSE;
		}
		else
		{
			/* the file still existed in the namespace, but was not cleared when closed */
			is_proc_init = TRUE;
		}
	}
	
#endif
	
	if(is_proc_init)
	{
		mexLock();
		
		glob_info = temp_info;
		mexMakeMemoryPersistent(glob_info);
		
		glob_info->flags.is_proc_lock_init = FALSE;
		
		glob_info->shm_update_reg.is_init = FALSE;
		glob_info->shm_update_reg.is_mapped = FALSE;
		
		glob_info->shm_data_reg.is_init = FALSE;
		glob_info->shm_data_reg.is_mapped = FALSE;
		
		/*copy over the startup signature */
		strcpy(glob_info->startup_flag.ptr, MSH_STARTUP_SIG);
#ifdef MSH_UNIX
		msync(glob_info->startup_flag.ptr, glob_info->startup_flag.reg_sz, MS_SYNC|MS_INVALIDATE);
#endif
		
		glob_info->flags.is_glob_shm_var_init = FALSE;
		
		glob_info->flags.is_freed = FALSE;
		glob_info->flags.is_proc_locked = FALSE;
		
		glob_info->num_lcl_objs_using = 1;
	}
	else
	{

#ifdef MSH_WIN
		UnmapViewOfFile(temp_info->startup_flag.ptr);
#else
		munmap(temp_info->startup_flag.ptr, temp_info->startup_flag.reg_sz);
		shm_unlink(temp_info->startup_flag.name);
#endif
		
		mxFree(temp_info);
		glob_info->num_lcl_objs_using += 1;
	}
	
}


void initProcLock(void)
{

#ifdef MSH_WIN
	
	glob_info->lock_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	glob_info->lock_sec.lpSecurityDescriptor = NULL;
	glob_info->lock_sec.bInheritHandle = TRUE;
	
	HANDLE temp_handle = CreateMutex(&glob_info->lock_sec, FALSE, MSH_LOCK_NAME);
	DWORD err = GetLastError();
	if(temp_handle == NULL)
	{
		readMXError("Internal:InitMutexError", "Failed to create the mutex (Error number: %u).");
	}
	else if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->proc_lock, 0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	else
	{
		glob_info->proc_lock = temp_handle;
	}

#else
	
	glob_info->proc_lock = sem_open(MSH_LOCK_NAME, O_RDWR | O_CREAT, S_IRWXU, 1);
	if(glob_info->proc_lock == SEM_FAILED)
	{
		switch(errno)
		{
			case EACCES:
				readMXError("SemOpenAccessError", "The named semaphore exists and the permissions specified by oflag are denied, or the named semaphore does not exist and permission to create the named semaphore is denied.");
			case EINTR:
				readMXError("SemOpenInterruptError", "The sem_open() operation was interrupted by a signal.");
			case EINVAL:
				readMXError("SemOpenInvalidError", "The sem_open() operation is not supported for the given name.");
			case EMFILE:
				readMXError("SemOpenTooManyFilesError", "Too many semaphore descriptors or file descriptors are currently in use by this process.");
			case ENFILE:
				readMXError("SemOpenTooManySemsError", "Too many semaphores are currently open in the system.");
			case ENOSPC:
				readMXError("SemOpenNoSpaceError", "There is insufficient space for the creation of the new named semaphore.");
			case ENOSYS:
				readMXError("SemOpenNotSupportedError", "The function sem_open() is not supported by this implementation.");
			default:
				readMXError("SemOpenUnknownError", "An unknown error occurred.");
			
		}
	}

#endif
	
	glob_info->flags.is_proc_lock_init = TRUE;

}


void initUpdateSegment(void)
{

	glob_info->shm_update_reg.reg_sz = sizeof(shm_segment_info);
	
#ifdef MSH_WIN
	
	HANDLE temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)glob_info->shm_update_reg.reg_sz, MSH_UPDATE_SEGMENT_NAME);
	DWORD err = GetLastError();
	if(temp_handle == NULL)
	{
		releaseProcLock();
		readMXError("CreateUpdateSegError", "Could not create or open the update memory segment (Error number: %u).");
	}
	glob_info->shm_update_reg.is_init = TRUE;
	
	if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->shm_update_reg.handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
		is_glob_init = FALSE;
	}
	else
	{
		/* will initialize */
		glob_info->shm_update_reg.handle = temp_handle;
		is_glob_init = TRUE;
	}

#else
	
	/* Try to open an already created segment so we can get the global init signal */
	strcpy(glob_info->shm_update_reg.name, MSH_UPDATE_SEGMENT_NAME);
	glob_info->shm_update_reg.handle = shm_open(glob_info->shm_update_reg.name, O_RDWR, S_IRWXU);
	if(glob_info->shm_update_reg.handle == -1)
	{
		is_glob_init = TRUE;
		if(errno == ENOENT)
		{
			glob_info->shm_update_reg.handle = shm_open(glob_info->shm_update_reg.name, O_RDWR | O_CREAT, S_IRWXU);
			if(glob_info->shm_update_reg.handle == -1)
			{
				readShmError(errno);
			}
		}
		else
		{
			readShmError(errno);
		}
	}
	else
	{
		is_glob_init = FALSE;
	}
	glob_info->shm_update_reg.is_init = TRUE;
	
	if(ftruncate(glob_info->shm_update_reg.handle, glob_info->shm_update_reg.reg_sz) != 0)
	{
		readFtruncateError(errno);
	}
	

#endif

}

void mapUpdateSegment(void)
{
#ifdef MSH_WIN
	
	glob_info->shm_update_reg.ptr = MapViewOfFile(glob_info->shm_update_reg.handle, FILE_MAP_ALL_ACCESS, 0, 0, glob_info->shm_update_reg.reg_sz);
	if(shm_update_info == NULL)
	{
		releaseProcLock();
		readMXError("MapUpdateSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}

#else
	
	glob_info->shm_update_reg.ptr = mmap(NULL, glob_info->shm_update_reg.reg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, glob_info->shm_update_reg.handle, 0);
	if(shm_update_info == MAP_FAILED)
	{
		readMmapError(errno);
	}

#endif
	
	glob_info->shm_update_reg.is_mapped = TRUE;
	
}


void globStartup(header_t* hdr)
{
	if(is_glob_init)
	{
		
		/* this is the first region created */
		/* this info shouldn't ever actually be used */
		/* but make sure the memory segment is consistent */
		hdr->isNumeric = 1;
		hdr->isSparse = 0;
		hdr->isEmpty = 1;
		hdr->complexity = mxREAL;
		hdr->classid = mxDOUBLE_CLASS;
		hdr->nDims = 2;
		hdr->elemsiz = sizeof(mxDouble);
		hdr->nzmax = 0;      /* update this later on sparse*/
		hdr->nFields = 0;                                 /* update this later */
		hdr->shmsiz = pad_to_align(sizeof(header_t));     /* update this later */
		hdr->strBytes = 0;
		hdr->par_hdr_off = 0;
		
		shm_update_info->num_procs = 1;
		shm_update_info->seg_num = 0;
		shm_update_info->rev_num = 0;
		shm_update_info->lead_seg_num = 0;
		shm_update_info->upd_pid = glob_info->this_pid;
		shm_update_info->seg_sz = hdr->shmsiz;
	}
	else
	{
		if(is_proc_init)
		{
			shm_update_info->num_procs += 1;
		}
	}
}


void initDataSegment(void)
{
	
	glob_info->shm_data_reg.reg_sz = shm_update_info->seg_sz;
	sprintf(glob_info->shm_data_reg.name, MSH_SEGMENT_NAME, (unsigned long long)shm_update_info->seg_num);

#ifdef MSH_WIN
	
	uint32_t lo_sz = (uint32_t)(glob_info->shm_data_reg.reg_sz & 0xFFFFFFFFL);
	uint32_t hi_sz = (uint32_t)((glob_info->shm_data_reg.reg_sz >> 32) & 0xFFFFFFFFL);
	HANDLE temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, glob_info->shm_data_reg.name);
	DWORD err = GetLastError();
	if(temp_handle == NULL)
	{
		releaseProcLock();
		readMXError("CreateFileError", "Error creating the file mapping (Error Number %u)", err);
	}
	glob_info->shm_data_reg.is_init = TRUE;
	
	if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->shm_data_reg.handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	else
	{
		glob_info->shm_data_reg.handle = temp_handle;
	}
	
#else
	
	glob_info->shm_data_reg.handle = shm_open(glob_info->shm_data_reg.name, O_RDWR | O_CREAT, S_IRWXU);
	if(glob_info->shm_data_reg.handle == -1)
	{
		readShmError(errno);
	}
	glob_info->shm_data_reg.is_init = TRUE;
	
	if(ftruncate(glob_info->shm_data_reg.handle, shm_update_info->seg_sz) != 0)
	{
		readFtruncateError(errno);
	}
	
#endif

}


void mapDataSegment(void)
{
#ifdef MSH_WIN
	
	glob_info->shm_data_reg.ptr = MapViewOfFile(glob_info->shm_data_reg.handle, FILE_MAP_ALL_ACCESS, 0, 0, glob_info->shm_data_reg.reg_sz);
	if(shm_data_ptr == NULL)
	{
		releaseProcLock();
		readMXError("MapDataSegError", "Could not map the update memory segment (Error number %u)", GetLastError());
	}
	
#else
	
	glob_info->shm_data_reg.ptr = mmap(NULL, glob_info->shm_data_reg.reg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, glob_info->shm_data_reg.handle, 0);
	if(shm_data_ptr == MAP_FAILED)
	{
		readMmapError(errno);
	}
	
#endif
	
	glob_info->shm_data_reg.is_mapped = TRUE;
	
}


void makeDummyVar(mxArray** out)
{
	glob_shm_var = mxCreateDoubleMatrix(0, 0, mxREAL);
	mexMakeArrayPersistent(glob_shm_var);
	*out = mxCreateSharedDataCopy(glob_shm_var);
	glob_info->flags.is_glob_shm_var_init = TRUE;
}