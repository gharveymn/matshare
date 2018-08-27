/** mshutils.c
 * Defines miscellaneous utility functions.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "mex.h"

#include <ctype.h>

#include "mshtypes.h"
#include "mshexterntypes.h"
#include "mshutils.h"
#include "mshvariables.h"
#include "mshlockfree.h"
#include "mlerrorutils.h"

#ifdef MSH_UNIX
#  include <string.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif

void msh_AcquireProcessLock(FileLock_T file_lock)
{
#ifdef MSH_WIN
	DWORD status;
#endif
	
	if(g_local_info.lock_level == 0)
	{

#ifdef MSH_WIN
		status = WaitForSingleObject(file_lock, INFINITE);
		if(status == WAIT_ABANDONED)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, "ProcessLockAbandonedError", "Another process has failed. Cannot safely continue.");
		}
		else if(status == WAIT_FAILED)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ProcessLockError", "Failed to lock acquire the process lock.");
		}
#else
		if(lockf(file_lock.lock_handle, F_LOCK, file_lock.lock_size) != 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ProcessLockError", "Failed to acquire the process lock.");
		}
#endif
	
	}
	
	g_local_info.lock_level += 1;
	
}


void msh_ReleaseProcessLock(FileLock_T file_lock)
{
	
	if(g_local_info.lock_level > 0)
	{
		if(g_local_info.lock_level == 1)
		{
#ifdef MSH_WIN
			if(ReleaseMutex(file_lock) == 0)
			{
				/* prevent recursion in error callback */
				meu_SetErrorCallback(NULL);
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, "ProcessUnlockError", "Failed to release the process lock.");
			}
#else
			if(lockf(file_lock.lock_handle, F_ULOCK, file_lock.lock_size) != 0)
			{
				/* prevent recursion in error callback */
				meu_SetErrorCallback(NULL);
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, "ProcessUnlockError", "Failed to release the process lock.");
			}
#endif
		}
		
		g_local_info.lock_level -= 1;
		
	}
	
}


void msh_WriteSegmentName(char* name_buffer, segmentnumber_T seg_num)
{
	if(seg_num == MSH_INVALID_SEG_NUM)
	{
		meu_PrintMexError(MEU_FL,
		                  MEU_SEVERITY_INTERNAL | MEU_SEVERITY_FATAL,
		                  "InternalLogicError",
		                  "There was an internal logic error where matshare lost track of its internal shared memory list. Please"
		                  "report this if you can.");
	}
	sprintf(name_buffer, MSH_SEGMENT_NAME_FORMAT, (unsigned long)seg_num);
}


void msh_WriteConfiguration(void)
{

#ifdef MSH_WIN
	DWORD bytes_wr;
#endif
	
	char_T* config_path;
	
	
	handle_T config_handle;
	UserConfig_T local_config = g_user_config, saved_config;
	
	config_path = msh_GetConfigurationPath();

#ifdef MSH_WIN
	
	if((config_handle = CreateFile(config_path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL)) == INVALID_HANDLE_VALUE)
	{
		mxFree(config_path);
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "OpenFileError", "Error opening the config file.");
	}
	else
	{
		if(ReadFile(config_handle, &saved_config, sizeof(UserConfig_T), &bytes_wr, NULL) == 0)
		{
			mxFree(config_path);
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_T)) != 0)
		{
			if(WriteFile(config_handle, &local_config, sizeof(UserConfig_T), &bytes_wr, NULL) == 0)
			{
				mxFree(config_path);
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "WriteFileError", "Error writing to the config file.");
			}
		}
		
		if(CloseHandle(config_handle) == 0)
		{
			mxFree(config_path);
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the config file handle.");
		}
	}
#else
	if((config_handle = open(config_path, O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR)) == -1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "OpenFileError", "Error opening the config file.");
	}
	else
	{
		if(read(config_handle, &saved_config, sizeof(UserConfig_T)) == -1)
		{
			mxFree(config_path);
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_T)) != 0)
		{
			if(write(config_handle, &local_config, sizeof(UserConfig_T)) == -1)
			{
				mxFree(config_path);
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "WriteFileError", "Error writing to the config file.");
			}
		}
		
		if(close(config_handle) == -1)
		{
			mxFree(config_path);
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the config file handle.");
		}
	}
#endif
	
	mxFree(config_path);
}


char_T* msh_GetConfigurationPath(void)
{
	char_T* user_config_folder;
	char_T* config_path;

#ifdef MSH_WIN
	
	/* use NULL check to avoid reliance on _WIN32_WINNT */
	if((user_config_folder = getenv("LOCALAPPDATA")) == NULL)
	{
		if((user_config_folder = getenv("APPDATA")) == NULL)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ConfigPathError", "Could not find a suitable configuration path. Please make sure either %LOCALAPPDATA% or %APPDATA% is defined.");
		}
	}
	
	config_path = mxCalloc(strlen(user_config_folder) + 2 + strlen(MSH_CONFIG_FOLDER_NAME) + 2 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_T));
	sprintf(config_path, "%s\\%s", user_config_folder, MSH_CONFIG_FOLDER_NAME);
	if(CreateDirectory(config_path, NULL) == 0)
	{
		if(GetLastError() != ERROR_ALREADY_EXISTS)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateDirectoryError", "There was an error creating the directory for the matshare config file at location \"%s\".", config_path);
		}
	}
	
	sprintf(config_path, "%s\\%s\\%s", user_config_folder, MSH_CONFIG_FOLDER_NAME, MSH_CONFIG_FILE_NAME);

#else
	if((user_config_folder = getenv("HOME")) == NULL)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ConfigPathError", "Could not find a suitable configuration path. Please make sure $HOME is defined.");
	}
	
	config_path = mxCalloc(strlen(user_config_folder) + 1 + strlen(HOME_CONFIG_FOLDER) + 1 + strlen(MSH_CONFIG_FOLDER_NAME) + 1 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_T));
	sprintf(config_path, "%s/%s", user_config_folder, HOME_CONFIG_FOLDER);
	if(mkdir(config_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
	{
		if(errno != EEXIST)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateDirectoryError", "There was an error creating the user config directory at location \"%s\".", config_path);
		}
	}
	
	sprintf(config_path, "%s/%s/%s", user_config_folder, HOME_CONFIG_FOLDER, MSH_CONFIG_FOLDER_NAME);
	if(mkdir(config_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
	{
		if(errno != EEXIST)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateDirectoryError", "There was an error creating the directory for the matshare config file at location \"%s\".",
						   config_path);
		}
	}
	
	sprintf(config_path, "%s/%s/%s/%s", user_config_folder, HOME_CONFIG_FOLDER, MSH_CONFIG_FOLDER_NAME, MSH_CONFIG_FILE_NAME);

#endif
	
	return config_path;
	
}


pid_T msh_GetPid(void)
{
#ifdef MSH_WIN
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}


size_t msh_PadToAlignData(size_t curr_sz)
{
	return curr_sz + ((MSH_ALIGNMENT-1) - ((curr_sz - 1) & (MSH_ALIGNMENT-1)));
}


void msh_CheckVarname(const mxArray* varname)
{
	size_t i, num_str_elems;
	mxChar* wide_str;
	
	if(!mxIsChar(varname))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNameError", "All variable IDs must be of type 'char'.");
	}
	
	if(mxIsEmpty(varname))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNameError", "Variable names must have a length more than zero.");
	}
	
	if((num_str_elems = mxGetNumberOfElements(varname)) > MSH_NAME_LEN_MAX-1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNameError", "Variable names must have length of less than %d characters.", MSH_NAME_LEN_MAX);
	}
	
	wide_str = mxGetChars(varname);
	if(wide_str[0] != '-' && isalpha(wide_str[0]))
	{
		for(i = 1; i < num_str_elems; i++)
		{
			if(!(isalnum(wide_str[i]) || wide_str[i] == '_'))
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNameError", "Variable names must consist only of ANSI alphabetic characters.");
			}
		}
	}
	else
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNameError", "Variable names must start with an ANSI alphabetic character.");
	}
}


/* MurmurHash3, by Austin Appleby (public domain---thanks, Austin!) */
uint32_T msh_MurmurHash3(const uint8_T* key, size_t len, int seed)
{
	uint32_T h = (uint32_T)seed;
	if(len > 3)
	{
		const uint32_T* key_x4 = (const uint32_T*)key;
		size_t i = len >> 2u;
		do
		{
			uint32_T k = *key_x4++;
			k *= 0xcc9e2d51;
			k = (k << 15u) | (k >> 17u);
			k *= 0x1b873593;
			h ^= k;
			h = (h << 13u) | (h >> 19u);
			h = (h*5) + 0xe6546b64;
		} while(--i);
		key = (const uint8_T*)key_x4;
	}
	if(len & 3u)
	{
		size_t i = len & 3u;
		uint32_T k = 0;
		key = &key[i - 1];
		do
		{
			k <<= 8;
			k |= *key--;
		} while(--i);
		k *= 0xcc9e2d51;
		k = (k << 15u) | (k >> 17u);
		k *= 0x1b873593;
		h ^= k;
	}
	h ^= len;
	h ^= h >> 16u;
	h *= 0x85ebca6b;
	h ^= h >> 13u;
	h *= 0xc2b2ae35;
	h ^= h >> 16u;
	return h;
}
