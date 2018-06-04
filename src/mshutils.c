#include <string.h>

#include "headers/mshutils.h"
#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/matlabutils.h"
#include "headers/mshtypes.h"

#ifdef MSH_UNIX
#  include <unistd.h>
#endif

static LockFreeCounter_t old_counter, new_counter;

void msh_OnExit(void)
{
	
	g_local_info.is_initialized = FALSE;
	
	msh_DetachSegmentList(&g_local_seg_list);
	msh_DestroyTable(&g_local_seg_list.seg_table);
	
	if(g_local_info.shared_info_wrapper.ptr != NULL)
	{
	
#ifdef MSH_WIN
		if(msh_AtomicDecrement(&g_shared_info->num_procs) == 0)
		{
			msh_WriteConfiguration();
		}
#else
		
		/* this will set the unlink flag to TRUE if it hits zero atomically, and only return true if this process did the operation */
		if(msh_DecrementCounter(&g_shared_info->num_procs, TRUE))
		{
			msh_WriteConfiguration();
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

	/* set the process lock at a level where it can be released if needed */
	while(g_local_info.lock_level > 0)
	{
		msh_ReleaseProcessLock();
	}

}


void msh_AcquireProcessLock(void)
{
#ifdef MSH_WIN
	DWORD status;
#endif
	
	/* blocks until lock flag operation is finished */
	while(!msh_GetCounterPost(&g_shared_info->user_defined.lock_counter));
	
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
}


void msh_ReleaseProcessLock(void)
{
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


void msh_WriteConfiguration(void)
{
	char_t* config_path;
	
	handle_t config_handle;
	UserConfig_t local_config = g_shared_info->user_defined, saved_config;
	local_config.lock_counter.values.count = 0;				/* reset the lock_counter so that it counter values don't roll over */
	local_config.lock_counter.values.post = TRUE;
	
	config_path = msh_GetConfigurationPath();

#ifdef MSH_WIN
	DWORD bytes_wr;
	
	if((config_handle = CreateFile(config_path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL)) == INVALID_HANDLE_VALUE)
	{
		mxFree(config_path);
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "OpenFileError", "Error opening the config file.");
	}
	else
	{
		if(ReadFile(config_handle, &saved_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
		{
			mxFree(config_path);
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_t)) != 0)
		{
			if(WriteFile(config_handle, &local_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
			{
				mxFree(config_path);
				ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "WriteFileError", "Error writing to the config file.");
			}
		}
		
		if(CloseHandle(config_handle) == 0)
		{
			mxFree(config_path);
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CloseHandleError", "Error closing the config file handle.");
		}
	}
#else
	if((config_handle = open(config_path, O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR)) == -1)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "OpenFileError", "Error opening the config file.");
	}
	else
	{
		if(read(config_handle, &saved_config, sizeof(UserConfig_t)) == -1)
		{
			mxFree(config_path);
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_t)) != 0)
		{
			if(write(config_handle, &local_config, sizeof(UserConfig_t)) == -1)
			{
				mxFree(config_path);
				ReadMexErrorWithCode(__FILE__, __LINE__, errno, "WriteFileError", "Error writing to the config file.");
			}
		}
		
		if(close(config_handle) == -1)
		{
			mxFree(config_path);
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CloseHandleError", "Error closing the config file handle.");
		}
	}
#endif
	
	mxFree(config_path);
}


char_t* msh_GetConfigurationPath(void)
{
	char_t* user_config_folder;
	char_t* config_path;
	
#ifdef MSH_WIN
	user_config_folder = getenv("LOCALAPPDATA");
	config_path = mxCalloc(strlen(user_config_folder) + 2 + strlen(MSH_CONFIG_FOLDER_NAME) + 2 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_t));
	sprintf(config_path, "%s\\%s", user_config_folder, MSH_CONFIG_FOLDER_NAME);
	if(CreateDirectory(config_path, NULL) == 0)
	{
		if(GetLastError() != ERROR_ALREADY_EXISTS)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CreateDirectoryError", "There was an error creating the directory for the matshare config file.");
		}
	}
	
	sprintf(config_path, "%s\\%s\\%s", user_config_folder, MSH_CONFIG_FOLDER_NAME, MSH_CONFIG_FILE_NAME);
	
#else
	user_config_folder = getenv("HOME");
	config_path = mxCalloc(strlen(user_config_folder) + 1 + strlen(HOME_CONFIG_FOLDER) + 1 + strlen(MSH_CONFIG_FOLDER_NAME) + 1 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_t));
	sprintf(config_path, "%s/%s", user_config_folder, HOME_CONFIG_FOLDER);
	if(mkdir(config_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0)
	{
		if(errno != EEXIST)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CreateDirectoryError", "There was an error creating the user config directory.");
		}
	}
	
	sprintf(config_path, "%s/%s/%s", user_config_folder, HOME_CONFIG_FOLDER, MSH_CONFIG_FOLDER_NAME);
	if(mkdir(config_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0)
	{
		if(errno != EEXIST)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CreateDirectoryError", "There was an error creating the directory for the matshare config file.");
		}
	}
	
	sprintf(config_path, "%s/%s/%s/%s", user_config_folder, HOME_CONFIG_FOLDER, MSH_CONFIG_FOLDER_NAME, MSH_CONFIG_FILE_NAME);
	
#endif
	
	return config_path;
	
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
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.flag = val;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
}


void msh_SetCounterPost(LockFreeCounter_t* counter, unsigned long val)
{
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
	msh_SetCounterPost(counter, FALSE);
	old_counter.values.count = 0;
	old_counter.values.post = FALSE;
	new_counter.values.count = 0;
	new_counter.values.flag = val;
	new_counter.values.post = TRUE;
	do
	{
		old_counter.values.flag = counter->values.flag;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	/* this also sets post to TRUE */
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
