#include "headers/mshtypes.h"
#include "headers/mshutils.h"
#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/mlerrorutils.h"

#ifdef MSH_UNIX
#  include <string.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
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
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "UnlinkError", "There was an error unlinking the shared info segment. This is a critical error, please restart.");
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
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "CloseHandleError", "Error closing the process lock handle.");
		}
		g_local_info.process_lock = MSH_INVALID_HANDLE;
	}
#endif
	
	if(g_local_info.is_mex_locked)
	{
		if(!mexIsLocked())
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_CORRUPTION | MEU_SEVERITY_INTERNAL, 0, "MexUnlockedError", "Matshare tried to unlock its file when it was already unlocked.");
		}
		mexUnlock();
		g_local_info.is_mex_locked = FALSE;
	}
	mexAtExit(msh_NullFunction);
	
}


void msh_OnError(void)
{
	meu_SetErrorCallback(NULL);

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

#ifdef MSH_DEBUG_PERF
	msh_GetTick(&busy_wait_time.old);
#endif
	
	/* blocks until lock flag operation is finished */
	while(!msh_GetCounterPost(&g_shared_info->user_defined.lock_counter));

#ifdef MSH_DEBUG_PERF
	msh_GetTick(&busy_wait_time.new);
	msh_AtomicAddSizeWithMax(&g_shared_info->debug_perf.busy_wait_time, msh_GetTickDifference(&busy_wait_time), SIZE_MAX);
#endif
	
	msh_IncrementCounter(&g_shared_info->user_defined.lock_counter);
	
	if(msh_GetCounterFlag(&g_shared_info->user_defined.lock_counter))
	{
		if(g_local_info.lock_level == 0)
		{

#ifdef MSH_DEBUG_PERF
			msh_GetTick(&lock_time.old);
#endif

#ifdef MSH_WIN
			status = WaitForSingleObject(g_process_lock, INFINITE);
			if(status == WAIT_ABANDONED)
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, 0, "ProcessLockAbandonedError",  "Another process has failed. Cannot safely continue.");
			}
			else if(status == WAIT_FAILED)
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "ProcessLockError",  "Failed to lock acquire the process lock.");
			}
#else
			
			if(lockf(g_process_lock, F_LOCK, sizeof(SharedInfo_t)) != 0)
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "ProcessLockError", "Failed to acquire the process lock.");
			}
#endif

#ifdef MSH_DEBUG_PERF
			msh_GetTick(&lock_time.new);
			msh_AtomicAddSizeWithMax(&g_shared_info->debug_perf.lock_time, msh_GetTickDifference(&lock_time), SIZE_MAX);
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
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "ProcessUnlockError", "Failed to release the process lock.");
			}
#else
			if(lockf(g_process_lock, F_ULOCK, sizeof(SharedInfo_t)) != 0)
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "ProcessUnlockError", "Failed to release the process lock.");
			}
#endif
		}
		
		g_local_info.lock_level -= 1;
		
	}

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

#ifdef MSH_WIN
	DWORD bytes_wr;
#endif
	
	char_t* config_path;

	
	handle_t config_handle;
	UserConfig_t local_config = g_shared_info->user_defined, saved_config;
	local_config.lock_counter.values.count = 0;				/* reset the lock_counter so that it counter values don't roll over */
	local_config.lock_counter.values.post = TRUE;
	
	config_path = msh_GetConfigurationPath();

#ifdef MSH_WIN
	
	if((config_handle = CreateFile(config_path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL)) == INVALID_HANDLE_VALUE)
	{
		mxFree(config_path);
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "OpenFileError", "Error opening the config file.");
	}
	else
	{
		if(ReadFile(config_handle, &saved_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
		{
			mxFree(config_path);
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_t)) != 0)
		{
			if(WriteFile(config_handle, &local_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
			{
				mxFree(config_path);
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "WriteFileError", "Error writing to the config file.");
			}
		}
		
		if(CloseHandle(config_handle) == 0)
		{
			mxFree(config_path);
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "CloseHandleError", "Error closing the config file handle.");
		}
	}
#else
	if((config_handle = open(config_path, O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR)) == -1)
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "OpenFileError", "Error opening the config file.");
	}
	else
	{
		if(read(config_handle, &saved_config, sizeof(UserConfig_t)) == -1)
		{
			mxFree(config_path);
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_t)) != 0)
		{
			if(write(config_handle, &local_config, sizeof(UserConfig_t)) == -1)
			{
				mxFree(config_path);
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "WriteFileError", "Error writing to the config file.");
			}
		}
		
		if(close(config_handle) == -1)
		{
			mxFree(config_path);
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "CloseHandleError", "Error closing the config file handle.");
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
	
	/* use NULL check to avoid reliance on _WIN32_WINNT */
	if((user_config_folder = getenv("LOCALAPPDATA")) == NULL)
	{
		if((user_config_folder = getenv("APPDATA")) == NULL)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "ConfigPathError", "Could not find a suitable configuration path. Please make sure either %LOCALAPPDATA% or %APPDATA% is defined.");
		}
	}
	config_path = mxCalloc(strlen(user_config_folder) + 2 + strlen(MSH_CONFIG_FOLDER_NAME) + 2 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_t));
	sprintf(config_path, "%s\\%s", user_config_folder, MSH_CONFIG_FOLDER_NAME);
	if(CreateDirectory(config_path, NULL) == 0)
	{
		if(GetLastError() != ERROR_ALREADY_EXISTS)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "CreateDirectoryError", "There was an error creating the directory for the matshare config file at location \"%s\".", config_path);
		}
	}
	
	sprintf(config_path, "%s\\%s\\%s", user_config_folder, MSH_CONFIG_FOLDER_NAME, MSH_CONFIG_FILE_NAME);
	
#else
	user_config_folder = getenv("HOME");
	config_path = mxCalloc(strlen(user_config_folder) + 1 + strlen(HOME_CONFIG_FOLDER) + 1 + strlen(MSH_CONFIG_FOLDER_NAME) + 1 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_t));
	sprintf(config_path, "%s/%s", user_config_folder, HOME_CONFIG_FOLDER);
	if(mkdir(config_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
	{
		if(errno != EEXIST)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "CreateDirectoryError", "There was an error creating the user config directory at location \"%s\".", config_path);
		}
	}
	
	sprintf(config_path, "%s/%s/%s", user_config_folder, HOME_CONFIG_FOLDER, MSH_CONFIG_FOLDER_NAME);
	if(mkdir(config_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
	{
		if(errno != EEXIST)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "CreateDirectoryError", "There was an error creating the directory for the matshare config file at location \"%s\".",
						   config_path);
		}
	}
	
	sprintf(config_path, "%s/%s/%s/%s", user_config_folder, HOME_CONFIG_FOLDER, MSH_CONFIG_FOLDER_NAME, MSH_CONFIG_FILE_NAME);
	
#endif
	
	return config_path;
	
}


void msh_SetDefaultConfiguration(void)
{
	g_shared_info->user_defined.sharetype = MSH_SHARETYPE;
	msh_SetCounterFlag(&g_shared_info->user_defined.lock_counter, MSH_THREAD_SAFETY);
	msh_SetCounterPost(&g_shared_info->user_defined.lock_counter, TRUE);       		  /** counter is in post state **/
	g_shared_info->user_defined.max_shared_segments = MSH_MAX_SHARED_SEGMENTS;
	g_shared_info->user_defined.max_shared_size = MSH_MAX_SHARED_SIZE;
	g_shared_info->user_defined.will_gc = TRUE;
#ifdef MSH_UNIX
	g_shared_info->user_defined.security = MSH_DEFAULT_PERMISSIONS;
#endif
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
LockFreeCounter_t msh_IncrementCounter(volatile LockFreeCounter_t* counter)
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
bool_t msh_DecrementCounter(volatile LockFreeCounter_t* counter, bool_t set_flag)
{
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.count -= 1;
		if(set_flag && old_counter.values.flag == FALSE && new_counter.values.count == 0)
		{
			new_counter.values.flag = TRUE;
		}
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
	return (old_counter.values.flag != new_counter.values.flag);
}


void msh_SetCounterFlag(volatile LockFreeCounter_t* counter, unsigned long val)
{
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.flag = val;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
}


void msh_SetCounterPost(volatile LockFreeCounter_t* counter, unsigned long val)
{
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.post = val;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
}

unsigned long msh_GetCounterCount(volatile LockFreeCounter_t* counter)
{
	return counter->values.count;
}

unsigned long msh_GetCounterFlag(volatile LockFreeCounter_t* counter)
{
	return counter->values.flag;
}

unsigned long msh_GetCounterPost(volatile LockFreeCounter_t* counter)
{
	return counter->values.post;
}

void msh_WaitSetCounter(volatile LockFreeCounter_t* counter, unsigned long val)
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


#ifdef MSH_DEBUG_PERF
void msh_GetTick(msh_tick_t* tick_pointer)
{
#  ifdef MSH_WIN
	QueryPerformanceCounter(tick_pointer);
#  else
	*tick_pointer = clock();
#  endif
}

size_t msh_GetTickDifference(TickTracker_t* tracker)
{
#  ifdef MSH_WIN
#    if MSH_BITNESS==64
	return (size_t)(tracker->new.QuadPart - tracker->old.QuadPart);
#    elif MSH_BITNESS==32
	return (size_t)(tracker->new.LowPart - tracker->old.LowPart);
#    endif
#  else
	return (size_t)(tracker->new - tracker->old);
#  endif
}

#endif


/**
 * Atomically adds the size_t value to the value pointed to. Fails if this will result in a
 * value higher than the maximum specified.
 *
 * @param value_pointer
 * @param add_value
 * @param max_value
 * @return
 */
bool_t msh_AtomicAddSizeWithMax(volatile size_t* value_pointer, size_t add_value, size_t max_value)
{
	size_t old_value, new_value;
#ifdef MSH_WIN
#  if MSH_BITNESS==64
	do
	{
		old_value = *value_pointer;
		new_value = old_value + add_value;
		if(new_value > max_value)
		{
			return FALSE;
		}
	} while(InterlockedCompareExchange64((volatile long long*)value_pointer, new_value, old_value) != old_value);
#  elif MSH_BITNESS==32
	do
	{
		/* workaround because lcc defines size_t as signed (???) */
		old_value = *value_pointer;
		new_value = old_value + add_value;
		if(new_value > max_value)
		{
			return FALSE;
		}
	} while(InterlockedCompareExchange((volatile long*)value_pointer, new_value, old_value) != old_value);
#  endif
#else
	do
	{
		old_value = *value_pointer;
		new_value = old_value + add_value;
		if(new_value > max_value)
		{
			return FALSE;
		}
	} while(__sync_val_compare_and_swap(value_pointer, old_value, new_value) != old_value);
#endif
	return TRUE;
}

size_t msh_AtomicSubtractSize(volatile size_t* value_pointer, size_t subtract_value)
{
#ifdef MSH_WIN
	/* using compare swaps because windows doesn't have unsigned interlocked functions */
	size_t old_value, new_value;
#  if MSH_BITNESS==64
	do
	{
		old_value = *value_pointer;
		new_value = old_value - subtract_value;
	} while(InterlockedCompareExchange64((volatile long long*)value_pointer, new_value, old_value) != old_value);
#  elif MSH_BITNESS==32
	do
	{
		old_value = *value_pointer;
		new_value = old_value - subtract_value;
	} while(InterlockedCompareExchange((volatile long*)value_pointer, new_value, old_value) != old_value);
#  endif
	return new_value;
#else
	return __sync_sub_and_fetch(value_pointer, subtract_value);
#endif
}

/*
long msh_AtomicAddLong(volatile long* value_pointer, long add_value)
{
#ifdef MSH_WIN
	return InterlockedAdd(value_pointer, add_value);
#else
	return __sync_add_and_fetch(value_pointer, add_value);
#endif
}
*/


long msh_AtomicIncrement(volatile long* value_pointer)
{
#ifdef MSH_WIN
	return InterlockedIncrement(value_pointer);
#else
	return __sync_add_and_fetch(value_pointer, 1);
#endif
}

long msh_AtomicDecrement(volatile long* value_pointer)
{
#ifdef MSH_WIN
	return InterlockedDecrement(value_pointer);
#else
	return __sync_sub_and_fetch(value_pointer, 1);
#endif
}

long msh_AtomicCompareSwap(volatile long* value_pointer, long compare_value, long swap_value)
{
#ifdef MSH_WIN
	return InterlockedCompareExchange(value_pointer, swap_value, compare_value);
#else
	return __sync_val_compare_and_swap(value_pointer, compare_value, swap_value);
#endif
}
