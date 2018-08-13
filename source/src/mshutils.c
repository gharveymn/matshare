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

static int msh_CheckSparseIndex(const mxArray* dest_var, mwIndex dest_idx, const mxArray* comp_var, mwIndex comp_idx);

void msh_AcquireProcessLock(ProcessLock_t process_lock)
{
#ifdef MSH_WIN
	DWORD status;
#endif
	
	if(g_local_info.lock_level == 0)
	{

#ifdef MSH_WIN
		status = WaitForSingleObject(process_lock, INFINITE);
		if(status == WAIT_ABANDONED)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, "ProcessLockAbandonedError", "Another process has failed. Cannot safely continue.");
		}
		else if(status == WAIT_FAILED)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ProcessLockError", "Failed to lock acquire the process lock.");
		}
#else
		if(lockf(process_lock.lock_handle, F_LOCK, process_lock.lock_size) != 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ProcessLockError", "Failed to acquire the process lock.");
		}
#endif
	
	}
	
	g_local_info.lock_level += 1;
	
}


void msh_ReleaseProcessLock(ProcessLock_t process_lock)
{
	
	if(g_local_info.lock_level > 0)
	{
		if(g_local_info.lock_level == 1)
		{
#ifdef MSH_WIN
			if(ReleaseMutex(process_lock) == 0)
			{
				/* prevent recursion in error callback */
				meu_SetErrorCallback(NULL);
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, "ProcessUnlockError", "Failed to release the process lock.");
			}
#else
			if(lockf(process_lock.lock_handle, F_ULOCK, process_lock.lock_size) != 0)
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


void msh_WriteSegmentName(char* name_buffer, segmentnumber_t seg_num)
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
	
	char_t* config_path;
	
	
	handle_t config_handle;
	UserConfig_t local_config = g_user_config, saved_config;
	
	config_path = msh_GetConfigurationPath();

#ifdef MSH_WIN
	
	if((config_handle = CreateFile(config_path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL)) == INVALID_HANDLE_VALUE)
	{
		mxFree(config_path);
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "OpenFileError", "Error opening the config file.");
	}
	else
	{
		if(ReadFile(config_handle, &saved_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
		{
			mxFree(config_path);
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_t)) != 0)
		{
			if(WriteFile(config_handle, &local_config, sizeof(UserConfig_t), &bytes_wr, NULL) == 0)
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
		if(read(config_handle, &saved_config, sizeof(UserConfig_t)) == -1)
		{
			mxFree(config_path);
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ReadFileError", "Error reading from the config file.");
		}
		
		if(memcmp(&local_config, &saved_config, sizeof(UserConfig_t)) != 0)
		{
			if(write(config_handle, &local_config, sizeof(UserConfig_t)) == -1)
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
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ConfigPathError", "Could not find a suitable configuration path. Please make sure either %LOCALAPPDATA% or %APPDATA% is defined.");
		}
	}
	
	config_path = mxCalloc(strlen(user_config_folder) + 2 + strlen(MSH_CONFIG_FOLDER_NAME) + 2 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_t));
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
	
	config_path = mxCalloc(strlen(user_config_folder) + 1 + strlen(HOME_CONFIG_FOLDER) + 1 + strlen(MSH_CONFIG_FOLDER_NAME) + 1 + strlen(MSH_CONFIG_FILE_NAME) + 1, sizeof(char_t));
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


pid_t msh_GetPid(void)
{
#ifdef MSH_WIN
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}


int msh_CompareVariableSize(const mxArray* dest_var, const mxArray* comp_var, ParsedIndices_t* parsed_indices)
{
	/* for loops */
	size_t i, j, dest_idx, comp_idx, dest_num_elems, comp_num_elems;
	
	/* for structures */
	int field_num, comp_field_num, dest_num_fields, found_field_name;
	
	const char_t* curr_field_name;
	
	mxClassID dest_class_id = mxGetClassID(dest_var);
	
	if(mxGetElementSize(dest_var) != mxGetElementSize(comp_var))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "InternalSizeError", "Internal element sizes were incompatible.");
	}
	
	if(dest_class_id != mxGetClassID(comp_var))
	{
		return FALSE;
	}
	
	dest_num_elems = mxGetNumberOfElements(dest_var);
	comp_num_elems = mxGetNumberOfElements(comp_var);
	
	if(parsed_indices == NULL)
	{
		/* can't allow differing dimensions because matlab doesn't use shared pointers to dimensions in mxArrays */
		if(mxGetNumberOfDimensions(dest_var) != mxGetNumberOfDimensions(comp_var) ||
		   memcmp(mxGetDimensions(dest_var), mxGetDimensions(comp_var), mxGetNumberOfDimensions(dest_var)*sizeof(mwSize)) != 0)
		{
			return FALSE;
		}
	}
	else
	{
		
		if(parsed_indices->num_indices < parsed_indices->num_slices)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexParsingError", "The internal index parser failed. Sorry about that.");
		}
		
		if(mxIsSparse(dest_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "SubscriptedSparseError", "Cannot directly assign sparse matrices through subscripts.");
		}
		else
		{
			for(i = 0, comp_idx = 0; i < parsed_indices->num_indices; i += parsed_indices->num_slices)
			{
				for(j = 0; j < parsed_indices->num_slices; j++)
				{
					comp_idx += parsed_indices->slice_lens[j];
					if(dest_num_elems < parsed_indices->starting_indices[i+j] + parsed_indices->slice_lens[j] ||
					   (comp_num_elems != 1 && comp_num_elems < comp_idx))
					{
						return FALSE;
					}
				}
			}
		}
	}
	
	/* Structure case */
	if(dest_class_id == mxSTRUCT_CLASS)
	{
		
		dest_num_elems = mxGetNumberOfElements(dest_var);
		dest_num_fields = mxGetNumberOfFields(dest_var);
		
		if(dest_num_fields != mxGetNumberOfFields(comp_var))
		{
			return FALSE;
		}
		
		/* search for each field name */
		for(field_num = 0; field_num < dest_num_fields; field_num++)
		{
			curr_field_name = mxGetFieldNameByNumber(dest_var, field_num);
			for(comp_field_num = 0, found_field_name = FALSE; comp_field_num < dest_num_fields; comp_field_num++)
			{
				if(strcmp(curr_field_name, mxGetFieldNameByNumber(comp_var, comp_field_num)) == 0)
				{
					found_field_name = TRUE;
					break;
				}
			}
			if(!found_field_name)
			{
				return FALSE;
			}
		}
		
		/* Go through each element */
		if(parsed_indices == NULL)
		{
			for(field_num = 0; field_num < dest_num_fields; field_num++)     /* each field */
			{
				curr_field_name = mxGetFieldNameByNumber(dest_var, field_num);
				for(dest_idx = 0; dest_idx < dest_num_elems; dest_idx++)
				{
					if(!msh_CompareVariableSize(mxGetField(dest_var, dest_idx, curr_field_name), mxGetField(comp_var, dest_idx, curr_field_name), NULL))
					{
						return FALSE;
					}
				}
			}
		}
		else
		{
			for(field_num = 0, comp_idx = 0; field_num < dest_num_fields; field_num++)     /* each field */
			{
				curr_field_name = mxGetFieldNameByNumber(dest_var, field_num);
				for(i = 0; i < parsed_indices->num_indices; i += parsed_indices->num_slices)
				{
					for(j = 0; j < parsed_indices->num_slices; j++)
					{
						for(dest_idx = parsed_indices->starting_indices[i+j]; dest_idx < parsed_indices->starting_indices[i+j] + parsed_indices->slice_lens[j]; dest_idx++)
						{
							if(comp_num_elems == 1)
							{
								if(!msh_CompareVariableSize(mxGetField(dest_var, dest_idx, curr_field_name), mxGetField(comp_var, 0, curr_field_name), NULL))
								{
									return FALSE;
								}
							}
							else
							{
								if(!msh_CompareVariableSize(mxGetField(dest_var, dest_idx, curr_field_name), mxGetField(comp_var, comp_idx, curr_field_name), NULL))
								{
									return FALSE;
								}
								comp_idx++;
							}
						}
					}
				}
			}
		}
		
		
	}
	else if(dest_class_id == mxCELL_CLASS) /* Cell case */
	{
		dest_num_elems = mxGetNumberOfElements(dest_var);
		
		if(dest_num_elems != mxGetNumberOfElements(comp_var))
		{
			return FALSE;
		}
		
		if(parsed_indices == NULL)
		{
			for(dest_idx = 0; dest_idx < dest_num_elems; dest_idx++)
			{
				if(!msh_CompareVariableSize(mxGetCell(dest_var, dest_idx), mxGetCell(comp_var, dest_idx), NULL))
				{
					return FALSE;
				}
			}
		}
		else
		{
			for(i = 0, comp_idx = 0; i < parsed_indices->num_indices; i += parsed_indices->num_slices)
			{
				for(j = 0; j < parsed_indices->num_slices; j++)
				{
					for(dest_idx = parsed_indices->starting_indices[i+j]; dest_idx < parsed_indices->starting_indices[i+j] + parsed_indices->slice_lens[j]; dest_idx++)
					{
						if(comp_num_elems == 1)
						{
							if(!msh_CompareVariableSize(mxGetCell(dest_var, dest_idx), mxGetCell(comp_var, 0), NULL))
							{
								return FALSE;
							}
						}
						else
						{
							if(!msh_CompareVariableSize(mxGetCell(dest_var, dest_idx), mxGetCell(comp_var, comp_idx), NULL))
							{
								return FALSE;
							}
							comp_idx++;
						}
					}
				}
			}
		}
	}
	else if(mxIsNumeric(dest_var) || dest_class_id == mxLOGICAL_CLASS || dest_class_id == mxCHAR_CLASS)      /*base case*/
	{
		if(mxIsComplex(dest_var) != mxIsComplex(comp_var))
		{
			return FALSE;
		}
	}
	else
	{
		/* this may occur if the user passes a destination variable which is not in shared memory */
		meu_PrintMexError(MEU_FL,
		                  MEU_SEVERITY_USER,
		                  "InvalidTypeError",
		                  "Unexpected input type. All elements of the shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
	}
	
	return TRUE;
	
}


void msh_OverwriteVariable(const mxArray* dest_var, const mxArray* in_var, ParsedIndices_t* parsed_indices, int will_sync)
{
	byte_t* dest_data, * in_data, * dest_imag_data, * in_imag_data;
	
	/* for iterators */
	size_t i, j, dest_idx, comp_idx, in_num_elems, nzmax, elem_size, dest_offset, in_offset;
	
	int field_num, in_num_fields, is_complex;
	
	const char_t* curr_field_name;
	
	mxClassID shared_class_id = mxGetClassID(in_var);
	
	in_num_elems = mxGetNumberOfElements(in_var);
	
	/* Structure case */
	if(shared_class_id == mxSTRUCT_CLASS)
	{
		in_num_fields = mxGetNumberOfFields(in_var);
		
		/* go through each element */
		if(parsed_indices == NULL)
		{
			for(field_num = 0; field_num < in_num_fields; field_num++)     /* each field */
			{
				curr_field_name = mxGetFieldNameByNumber(dest_var, field_num);
				for(dest_idx = 0; dest_idx < in_num_elems; dest_idx++)
				{
					msh_OverwriteVariable(mxGetField(dest_var, dest_idx, curr_field_name), mxGetField(in_var, dest_idx, curr_field_name), NULL, will_sync);
				}
			}
		}
		else
		{
			for(field_num = 0; field_num < in_num_fields; field_num++)     /* each field */
			{
				curr_field_name = mxGetFieldNameByNumber(dest_var, field_num);
				for(i = 0, comp_idx = 0; i < parsed_indices->num_indices; i += parsed_indices->num_slices)
				{
					for(j = 0; j < parsed_indices->num_slices; j++)
					{
						for(dest_idx = parsed_indices->starting_indices[i+j]; dest_idx < parsed_indices->starting_indices[i+j] + parsed_indices->slice_lens[j]; dest_idx++)
						{
							/* this inner conditional should be optimized out by the compiler */
							if(in_num_elems == 1)
							{
								msh_OverwriteVariable(mxGetField(dest_var, dest_idx, curr_field_name), mxGetField(in_var, 0, curr_field_name), NULL, will_sync);
							}
							else
							{
								msh_OverwriteVariable(mxGetField(dest_var, dest_idx, curr_field_name), mxGetField(in_var, comp_idx, curr_field_name), NULL, will_sync);
							}
							comp_idx++;
						}
					}
				}
			}
		}
	}
	else if(shared_class_id == mxCELL_CLASS) /* Cell case */
	{
		
		/* go through each element */
		if(parsed_indices == NULL)
		{
			for(dest_idx = 0; dest_idx < in_num_elems; dest_idx++)
			{
				msh_OverwriteVariable(mxGetCell(dest_var, dest_idx), mxGetCell(in_var, dest_idx), NULL, will_sync);
			}
		}
		else
		{
			for(i = 0, comp_idx = 0; i < parsed_indices->num_indices; i += parsed_indices->num_slices)
			{
				for(j = 0; j < parsed_indices->num_slices; j++)
				{
					for(dest_idx = parsed_indices->starting_indices[i+j]; dest_idx < parsed_indices->starting_indices[i+j] + parsed_indices->slice_lens[j]; dest_idx++)
					{
						/* this inner conditional should be optimized out by the compiler */
						if(in_num_elems == 1)
						{
							msh_OverwriteVariable(mxGetCell(dest_var, dest_idx), mxGetCell(in_var, 0), NULL, will_sync);
						}
						else
						{
							msh_OverwriteVariable(mxGetCell(dest_var, dest_idx), mxGetCell(in_var, comp_idx), NULL, will_sync);
						}
						comp_idx++;
					}
				}
			}
		}
	}
	else     /*base case*/
	{
		
		is_complex = mxIsComplex(in_var);
		
		if(mxIsSparse(in_var))
		{
			
			nzmax = mxGetNzmax(in_var);
			
			if(will_sync) msh_AcquireProcessLock(g_process_lock);
			
			memcpy(mxGetIr(dest_var), mxGetIr(in_var), nzmax*sizeof(mwIndex));
			memcpy(mxGetJc(dest_var), mxGetJc(in_var), (mxGetN(in_var) + 1)*sizeof(mwIndex));
			memcpy(mxGetData(dest_var), mxGetData(in_var), nzmax*mxGetElementSize(in_var));
			if(is_complex) memcpy(mxGetImagData(dest_var), mxGetImagData(in_var), nzmax*mxGetElementSize(in_var));
			
			if(will_sync) msh_ReleaseProcessLock(g_process_lock);
			
		}
		else if(!mxIsEmpty(in_var))
		{
			
			elem_size = mxGetElementSize(dest_var);
			
			if(parsed_indices == NULL)
			{
				in_num_elems = mxGetNumberOfElements(in_var);
				
				if(will_sync) msh_AcquireProcessLock(g_process_lock);
				
				memcpy(mxGetData(dest_var), mxGetData(in_var), in_num_elems*elem_size);
				if(is_complex) memcpy(mxGetImagData(dest_var), mxGetImagData(in_var), in_num_elems*elem_size);
				
				if(will_sync) msh_ReleaseProcessLock(g_process_lock);
				
			}
			else
			{
				
				dest_data = mxGetData(dest_var);
				in_data = mxGetData(in_var);
				
				dest_imag_data = mxGetImagData(dest_var);
				in_imag_data = mxGetImagData(in_var);
				
				for(i = 0, in_offset = 0; i < parsed_indices->num_indices; i += parsed_indices->num_slices)
				{
					for(j = 0; j < parsed_indices->num_slices; j++)
					{
						dest_offset = parsed_indices->starting_indices[i+j]*elem_size/sizeof(byte_t);
						
						if(will_sync) msh_AcquireProcessLock(g_process_lock);
						
						/* optimized out */
						if(in_num_elems == 1)
						{
							for(dest_idx = 0; dest_idx < parsed_indices->slice_lens[j]; dest_idx++)
							{
								memcpy(dest_data + dest_offset + dest_idx*elem_size/sizeof(byte_t), in_data, elem_size);
								if(is_complex) memcpy(dest_imag_data + dest_offset + dest_idx*elem_size/sizeof(byte_t), in_imag_data, elem_size);
							}
						}
						else
						{
							memcpy(dest_data + dest_offset, in_data + in_offset, parsed_indices->slice_lens[j]*elem_size);
							if(is_complex) memcpy(dest_imag_data + dest_offset, in_imag_data + in_offset, parsed_indices->slice_lens[j]*elem_size);
							in_offset += parsed_indices->slice_lens[j]*elem_size/sizeof(byte_t);
						}
						
						if(will_sync) msh_ReleaseProcessLock(g_process_lock);
						
					}
				}
			}
		}
		
	}
}


static int msh_CheckSparseIndex(const mxArray* dest_var, mwIndex dest_idx, const mxArray* comp_var, mwIndex comp_idx)
{
	mwIndex ir_offset;
	
	mwIndex* dest_ir  = mxGetIr(dest_var);
	mwIndex* dest_jc  = mxGetJc(dest_var);
	mwSize   dest_m   = mxGetM (dest_var);
	mwSize   dest_n   = mxGetN (dest_var);
	
	mwIndex* comp_ir  = mxGetIr(comp_var);
	mwIndex* comp_jc  = mxGetJc(comp_var);
	mwSize   comp_m   = mxGetM (comp_var);
	mwSize   comp_n   = mxGetN (comp_var);
	
	/* idx = m*col + row */
	mwIndex dest_col = dest_idx/dest_m;
	mwIndex dest_row = dest_idx%dest_m;
	
	mwIndex comp_col = comp_idx/comp_m;
	mwIndex comp_row = comp_idx%comp_m;
	
	int dest_is_nz = FALSE;
	int comp_is_nz = FALSE;
	
	if(dest_idx >= dest_m * dest_n)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndex", "Invalid destination index.");
	}
	
	if(comp_idx >= comp_m * comp_n)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndex", "Invalid destination index.");
	}
	
	for(ir_offset = dest_jc[dest_col]; ir_offset < dest_jc[dest_col+1]; ir_offset++)
	{
		if(dest_ir[ir_offset] == dest_row)
		{
			dest_is_nz = TRUE;
		}
	}
	
	for(ir_offset = comp_jc[comp_col]; ir_offset < comp_jc[comp_col+1]; ir_offset++)
	{
		if(comp_ir[ir_offset] == comp_row)
		{
			comp_is_nz = TRUE;
		}
	}
	
	if(dest_is_nz == comp_is_nz)
	{
		return 0;
	}
	else if(dest_is_nz && !comp_is_nz)
	{
		return -1;
	}
	else /*if(!dest_is_nz && comp_is_nz)*/
	{
		return 1;
	}
	
	return 0;
	
}


size_t msh_PadToAlignData(size_t curr_sz)
{
	return curr_sz + (DATA_ALIGNMENT_SHIFT - ((curr_sz - 1) & DATA_ALIGNMENT_SHIFT));
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


/* MurmurHash3, by Austin Appleby (with a few modifications) */
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
