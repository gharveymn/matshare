/** mshinit.c
 * Defines initialization functions.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#include "mex.h"

#include <string.h>

#include "mshinit.h"
#include "mlerrorutils.h"
#include "mshutils.h"
#include "mshtypes.h"
#include "mshsegments.h"
#include "mshtable.h"
#include "mshlockfree.h"

#ifdef MSH_UNIX
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#endif


/**
 * Initializes the shared info segment.
 */
static void msh_InitializeSharedInfo(void);


/**
 * Initializes the configuration parameters.
 */
static void msh_InitializeConfiguration(void);


void msh_InitializeMatshare(void)
{
	
	if(g_local_info.is_initialized)
	{
		return;
	}
	
	/* init == FALSE, deinit == FALSE */
	g_local_info.is_deinitialized = FALSE;
	
	mexAtExit(msh_OnExit);
	
	meu_SetErrorCallback(msh_OnError);
	meu_SetWarningCallback(NULL);
	meu_SetLibraryName(g_msh_library_name);
	meu_SetErrorHelpMessage(g_msh_error_help_message);
	meu_SetWarningHelpMessage(g_msh_warning_help_message);
	
	if(g_local_info.this_pid == 0)
	{
		g_local_info.this_pid = msh_GetPid();
	}
	
	msh_InitializeSharedInfo();

#ifdef MSH_WIN
	if(g_local_info.process_lock == MSH_INVALID_HANDLE)
	{
		if((g_local_info.process_lock = CreateMutex(NULL, FALSE, MSH_LOCK_NAME)) == NULL)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateMutexError", "Failed to create the mutex.");
		}
	}
#else
	if(g_local_info.process_lock.lock_handle == MSH_INVALID_HANDLE)
	{
		g_local_info.process_lock.lock_handle = g_local_info.shared_info_wrapper.handle;
		g_local_info.process_lock.lock_size = sizeof(SharedInfo_t);
	}
#endif
	
	if(g_local_seg_list.seg_table->table == NULL)
	{
		msh_InitializeTable(g_local_seg_list.seg_table);
	}
	
	if(g_local_seg_list.name_table->table == NULL)
	{
		msh_InitializeTable(g_local_seg_list.name_table);
	}
	
	g_local_info.is_initialized = TRUE;
	
	/* init == TRUE, deinit == FALSE */
	
}


static void msh_InitializeSharedInfo(void)
{

#ifdef MSH_UNIX
	LockFreeCounter_t ret_num_procs;
#endif
	
	if(g_local_info.shared_info_wrapper.handle == MSH_INVALID_HANDLE)
	{
		/* note: on both windows and linux, this is guaranteed to be zeroed after creating */
#ifdef MSH_WIN
		g_local_info.shared_info_wrapper.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)sizeof(SharedInfo_t), MSH_SHARED_INFO_SEGMENT_NAME);
		if(g_local_info.shared_info_wrapper.handle == NULL)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateSharedInfoError", "Could not create or open the shared info segment.");
		}
#else
		g_local_info.shared_info_wrapper.handle = shm_open(MSH_SHARED_INFO_SEGMENT_NAME, O_RDWR | O_CREAT, MSH_DEFAULT_SECURITY);
		if(g_local_info.shared_info_wrapper.handle == MSH_INVALID_HANDLE)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateError", "There was an error creating the shared info segment.");
		}
		
		if(ftruncate(g_local_info.shared_info_wrapper.handle, sizeof(SharedInfo_t)) != 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "TruncateError", "There was an error truncating the shared info segment.");
		}
#endif
	}
	
	if(g_local_info.shared_info_wrapper.ptr == NULL)
	{
		
		g_local_info.shared_info_wrapper.ptr = msh_MapMemory(g_local_info.shared_info_wrapper.handle, sizeof(SharedInfo_t));

#ifdef MSH_WIN
		if(msh_AtomicIncrement(&g_shared_info->num_procs) == 1)
		{
			/* this is the global initializer */
			g_shared_info->rev_num = MSH_INITIAL_STATE;
			g_shared_info->total_shared_size = 0;
			g_shared_info->last_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->first_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->num_shared_segments = 0;
			g_shared_info->has_fatal_error = 0;
			g_shared_info->update_pid = 0;
			/* the lockfree mechanism relies on is_initialized being initialized to 0 */
			
			msh_InitializeConfiguration();
			
			g_shared_info->is_initialized = TRUE;
		}
#else
		ret_num_procs = msh_IncrementCounter(&g_shared_info->num_procs);
		
		/* flag is set if the shared info segment is being unlinked */
		if(msh_GetCounterFlag(&g_shared_info->num_procs))
		{
			
			/* busy wait for the unlinking operation to complete */
			while(!msh_GetCounterPost(&g_shared_info->num_procs));
			
			/* close whatever we just opened and get the new shared memory */
			msh_UnmapMemory((void*)g_local_info.shared_info_wrapper.ptr, sizeof(SharedInfo_t));
			g_local_info.shared_info_wrapper.ptr = NULL;
			
			msh_CloseSharedMemory(g_local_info.shared_info_wrapper.handle);
			g_local_info.shared_info_wrapper.handle = MSH_INVALID_HANDLE;
			
			msh_InitializeSharedInfo();
			
			return;
		}
		
		if(ret_num_procs.values.count == 1)
		{
			/* this is the global initializer */
			g_shared_info->rev_num = MSH_INITIAL_STATE;
			g_shared_info->total_shared_size = 0;
			g_shared_info->last_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->first_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->num_shared_segments = 0;
			g_shared_info->has_fatal_error = 0;
			g_shared_info->update_pid = g_local_info.this_pid;
			/* the lockfree mechanism relies on is_initialized being initialized to 0 */
			
			msh_InitializeConfiguration();
			
			g_shared_info->is_initialized = TRUE;
		}
#endif
		
		/* busy wait until the shared memory is initialized to move on */
		while(!g_shared_info->is_initialized);
		
		msh_LockMemory((void*)g_local_info.shared_info_wrapper.ptr, sizeof(SharedInfo_t));
		
	}
}


static void msh_InitializeConfiguration(void)
{
#ifdef MSH_WIN
	DWORD bytes_wr;
#endif
	
	handle_t config_handle;
	char_t* config_path;
	
	config_path = msh_GetConfigurationPath();

#ifdef MSH_WIN
	
	if((config_handle = CreateFile(config_path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL)) == INVALID_HANDLE_VALUE)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateFileError", "Error opening the config file.");
	}
	else
	{
		if(GetLastError() == ERROR_ALREADY_EXISTS)
		{
			if(ReadFile(config_handle, (void*)&g_user_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ReadFileError", "Error reading from the config file.");
			}
		}
		else
		{
			/* this is a new file */
			msh_SetDefaultConfiguration();
			
			if(WriteFile(config_handle, (void*)&g_user_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "WriteFileError", "Error writing to the config file.");
			}
		}
		
		if(CloseHandle(config_handle) == 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the config file handle.");
		}
	}
#else
	if((config_handle = open(config_path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR)) == -1)
	{
		if(errno != EEXIST)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateFileError", "Error creating the config file.");
		}
		else
		{
			/* the config file is already created, open it instead */
			if((config_handle = open(config_path, O_RDONLY | O_CLOEXEC, S_IRUSR | S_IWUSR)) == -1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "OpenFileError", "Error opening the config file.");
			}
			else
			{
				if(read(config_handle, (void*)&g_user_config, sizeof(UserConfig_t)) == -1)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ReadFileError", "Error reading from the config file.");
				}
			}
		}
	}
	else
	{
		/* this is a new file */
		msh_SetDefaultConfiguration();
		
		if(write(config_handle, (void*)&g_user_config, sizeof(UserConfig_t)) == -1)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "WriteFileError", "Error writing to the config file.");
		}
	}
	
	if(close(config_handle) == -1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the config file handle.");
	}

#endif
	
	mxFree(config_path);
	
}


void msh_OnExit(void)
{
	
	if(g_local_info.is_deinitialized)
	{
		return;
	}
	
	g_local_info.is_initialized = FALSE;
	
	/* init == FALSE, deinit == FALSE */
	
	msh_DetachSegmentList(&g_local_seg_list);
	msh_DestroyTable(g_local_seg_list.seg_table);
	msh_DestroyTable(g_local_seg_list.name_table);
	
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
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "UnlinkError", "There was an error unlinking the shared info segment. This is a critical error, please restart.");
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
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the process lock handle.");
		}
		g_local_info.process_lock = MSH_INVALID_HANDLE;
	}
#else
	if(g_local_info.process_lock.lock_handle != MSH_INVALID_HANDLE)
	{
		g_local_info.process_lock.lock_handle = MSH_INVALID_HANDLE;
		g_local_info.process_lock.lock_size = 0;
	}
#endif
	
	/* make sure this is reset so there aren't any collisions with the shared state */
	g_local_info.rev_num  = MSH_INITIAL_STATE;
	
	g_local_info.is_deinitialized = TRUE;
	
	/* init == FALSE, deinit == TRUE */
	
}


void msh_SetDefaultConfiguration(void)
{
	msh_SetCounterPost(&g_user_config.lock_counter, FALSE);
	msh_SetCounterFlag(&g_user_config.lock_counter, MSH_DEFAULT_THREAD_SAFETY);
	msh_SetCounterPost(&g_user_config.lock_counter, TRUE);                        /** counter is in post state **/
	g_user_config.max_shared_segments = MSH_DEFAULT_MAX_SHARED_SEGMENTS;
	g_user_config.max_shared_size = MSH_DEFAULT_MAX_SHARED_SIZE;
	g_user_config.will_shared_gc = MSH_DEFAULT_SHARED_GC;
#ifdef MSH_UNIX
	g_user_config.security = MSH_DEFAULT_SECURITY;
#endif
	/* should be null terminated since shared memory is initialized */
	strncpy((char*)g_user_config.fetch_default, MSH_DEFAULT_FETCH_DEFAULT, sizeof(g_user_config.fetch_default)-1);
}


void msh_OnError(int error_severity)
{
	/* if the severity compromises the system set local and shared memory fatal flags */
	if(error_severity & MEU_SEVERITY_FATAL)
	{
		g_local_info.has_fatal_error = TRUE;
		if(g_shared_info != NULL)
		{
			g_shared_info->has_fatal_error = TRUE;
		}
	}
	
	/* set the process lock at a level where it can be released if needed */
	while(g_local_info.lock_level > 0)
	{
		msh_ReleaseProcessLock(g_process_lock);
	}
}
