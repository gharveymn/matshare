#include "headers/mshinit.h"
#include "headers/mlerrorutils.h"
#include "headers/mshutils.h"
#include "headers/mshtypes.h"
#include "headers/mshsegments.h"

#ifdef MSH_UNIX
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#endif

static void msh_InitializeSharedInfo(void);
static void msh_InitializeConfiguration(void);

void msh_InitializeMatshare(void)
{
	
	if(g_local_info.is_initialized)
	{
		return;
	}
	
	meu_SetErrorCallback(msh_OnError);
	meu_SetWarningCallback(NULL);
	meu_SetLibraryName(g_msh_library_name);
	meu_SetErrorHelpMessage(g_msh_error_help_message);
	meu_SetWarningHelpMessage(g_msh_warning_help_message);
	
	
	/* lock the file */
	if(!g_local_info.is_mex_locked)
	{
		if(mexIsLocked())
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_INTERNAL | MEU_SEVERITY_CORRUPTION, 0, "MexLockedError", "Matshare tried to lock its file when it was already locked.");
		}
		mexAtExit(msh_OnExit);
		mexLock();
		g_local_info.is_mex_locked = TRUE;
	}
	
	if(g_local_info.this_pid == 0)
	{
		g_local_info.this_pid = msh_GetPid();
	}
	
#ifdef MSH_WIN
	if(g_local_info.process_lock == MSH_INVALID_HANDLE)
	{
		if((g_local_info.process_lock = CreateMutex(NULL, FALSE, MSH_LOCK_NAME)) == NULL)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "CreateMutexError", "Failed to create the mutex.");
		}
	}
#endif
	
	msh_InitializeSharedInfo();
	
	if(g_local_seg_list.seg_table.table == NULL)
	{
		msh_InitializeTable(&g_local_seg_list.seg_table);
	}
	
	g_local_info.is_initialized = TRUE;
	
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
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "CreateSharedInfoError", "Could not create or open the shared info segment.");
		}
#else
		g_local_info.shared_info_wrapper.handle = shm_open(MSH_SHARED_INFO_SEGMENT_NAME, O_RDWR | O_CREAT, MSH_DEFAULT_PERMISSIONS);
		if(g_local_info.shared_info_wrapper.handle == MSH_INVALID_HANDLE)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "CreateError", "There was an error creating the shared info segment.");
		}
		
		if(ftruncate(g_local_info.shared_info_wrapper.handle, sizeof(SharedInfo_t)) != 0)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "TruncateError", "There was an error truncating the shared info segment.");
		}
#endif
	}
	
	if(g_local_info.shared_info_wrapper.ptr == NULL)
	{
		
		g_local_info.shared_info_wrapper.ptr = msh_MapMemory(g_local_info.shared_info_wrapper.handle, sizeof(SharedInfo_t));

#ifdef MSH_WIN
		if(msh_AtomicIncrement(&g_shared_info->num_procs) == 1)
		{
			g_shared_info->last_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->first_seg_num = MSH_INVALID_SEG_NUM;
			msh_InitializeConfiguration();
			g_shared_info->is_initialized = TRUE;
		}
#else
		ret_num_procs = msh_IncrementCounter(&g_shared_info->num_procs);
		
		/* flag is set if the shared info segment is being unlinked */
		if(msh_GetCounterFlag(&g_shared_info->num_procs))
		{

#ifdef MSH_DEBUG_PERF
			msh_GetTick(&busy_wait_time.old);
#endif
			
			/* busy wait for the unlinking operation to complete */
			while(!msh_GetCounterPost(&g_shared_info->num_procs));

#ifdef MSH_DEBUG_PERF
			msh_GetTick(&busy_wait_time.new);
			msh_AtomicAddSizeWithMax(&g_shared_info->debug_perf.busy_wait_time, msh_GetTickDifference(&busy_wait_time), SIZE_MAX);
#endif
			
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
			g_shared_info->last_seg_num = MSH_INVALID_SEG_NUM;
			g_shared_info->first_seg_num = MSH_INVALID_SEG_NUM;
			msh_InitializeConfiguration();
			g_shared_info->is_initialized = TRUE;
		}
#endif

#ifdef MSH_DEBUG_PERF
		msh_GetTick(&busy_wait_time.old);
#endif
		
		/* wait until the shared memory is initialized to move on */
		while(!g_shared_info->is_initialized);

#ifdef MSH_DEBUG_PERF
		msh_GetTick(&busy_wait_time.new);
		msh_AtomicAddSizeWithMax(&g_shared_info->debug_perf.busy_wait_time, msh_GetTickDifference(&busy_wait_time), SIZE_MAX);
#endif
		
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
		mxFree(config_path);
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "CreateFileError", "Error opening the config file.");
	}
	else
	{
		if(GetLastError() == ERROR_ALREADY_EXISTS)
		{
			if(ReadFile(config_handle, (void*)&g_shared_info->user_defined, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
			{
				mxFree(config_path);
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, GetLastError(), "ReadFileError", "Error reading from the config file.");
			}
		}
		else
		{
			/* this is a new file */
			msh_SetDefaultConfiguration();
			
			if(WriteFile(config_handle, (void*)&g_shared_info->user_defined, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
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
	if((config_handle = open(config_path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR)) == -1)
	{
		if(errno != EEXIST)
		{
			mxFree(config_path);
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "CreateFileError", "Error creating the config file.");
		}
		else
		{
			/* the config file is already created, open it instead */
			if((config_handle = open(config_path, O_RDONLY | O_CLOEXEC, S_IRUSR | S_IWUSR)) == -1)
			{
				mxFree(config_path);
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "OpenFileError", "Error opening the config file.");
			}
			else
			{
				if(read(config_handle, (void*)&g_shared_info->user_defined, sizeof(UserConfig_t)) == -1)
				{
					mxFree(config_path);
					meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "ReadFileError", "Error reading from the config file.");
				}
			}
		}
	}
	else
	{
		/* this is a new file */
		msh_SetDefaultConfiguration();
		
		if(write(config_handle, (void*)&g_shared_info->user_defined, sizeof(UserConfig_t)) == -1)
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
	
#endif

	mxFree(config_path);
	
}
