/** matshare_.c
 * Defines the MEX entry function and other top level functions.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "mex.h"

#include "headers/matshare_.h"
#include "headers/mshheader.h"
#include "headers/mshexterntypes.h"
#include "headers/mshtypes.h"
#include "headers/mlerrorutils.h"
#include "headers/mshutils.h"
#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/mshinit.h"

#include <ctype.h>

#ifdef MSH_UNIX
#  include <string.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif

/* undocumented function */
extern mxArray* mxCreateSharedDataCopy(mxArray *);

char_t* g_msh_library_name = "matshare";
char_t* g_msh_error_help_message = "";
char_t* g_msh_warning_help_message = "";

GlobalInfo_t g_local_info = {
							0,                      /* rev_num */
							0,                      /* lock_level */
							0,                      /* this_pid */
							{
								NULL,                    /* ptr */
								MSH_INVALID_HANDLE       /* handle */
							},     					/* shared_info_wrapper */
#ifdef MSH_WIN
							MSH_INVALID_HANDLE,     /* process_lock */
#else
							MSH_INVALID_HANDLE,
							0,
#endif
							0,                      /* is_mex_locked */
							0                       /* is_initialized */
						};

VariableList_t g_local_var_list = {0};
SegmentList_t g_local_seg_list = {0};

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	/* number of inputs other than the msh_directive */
	int num_in_vars = nrhs - 1;
	
	/* inputs */
	const mxArray* in_directive = prhs[0];
	const mxArray** in_vars = prhs + 1;
	
	/* resultant matshare directive */
	msh_directive_t directive;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "NotEnoughInputsError", "Minimum input arguments missing. You must supply a msh_directive.");
	}
	
	/* get the msh_directive, no check made since the function should not be used without entry functions */
	directive = (msh_directive_t)mxGetScalar(in_directive);
	
	/* Don't initialize if we are detaching and matshare has not been initialized for this process yet */
	if(directive == msh_DETACH && !g_local_info.is_initialized)
	{
		return;
	}
	
	msh_InitializeMatshare();
	
	if(g_shared_info->has_fatal_error)
	{
#ifdef MSH_WIN
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_FATAL, 0, "FatalError", "There was previously a fatal error to the matshare system. Please restart MATLAB.");
#else
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_FATAL, 0, "FatalError", "There was previously a fatal error to the matshare system. Please restart MATLAB. You may have to delete files "
																	"located in /dev/shm/.");
#endif
	}

#ifdef MSH_DEBUG_PERF
	msh_GetTick(&total_time.old);
#endif
	
	msh_CleanSegmentList(&g_local_seg_list);
	
	switch(directive)
	{
		case msh_SHARE:
			
			msh_Share(nlhs, plhs, num_in_vars, in_vars);
			
			break;
		
		case msh_FETCH:
			
			msh_Fetch(nlhs, plhs, directive);
			
			break;
		case msh_DETACH:
			
			msh_OnExit();
			
			break;
		
		case msh_CONFIG:
			
			if(num_in_vars % 2 != 0)
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidNumArgsError", "Each parameter must have a value associated to it.");
			}
			
			msh_Config(num_in_vars/2, in_vars);
			
			break;
		
		case msh_LOCALCOPY:
			
			if(num_in_vars == 0)
			{
				msh_Fetch(nlhs, plhs, directive);
			}
			else if(num_in_vars == 1)
			{
				if(nlhs == 1)
				{
					plhs[0] = mxDuplicateArray(in_vars[0]);
				}
				else if(nlhs > 1)
				{
					meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "TooManyOutputsError", "Too many outputs were requested.");
				}
			}
			else
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "TooManyInputsError", "Too many inputs.");
			}
			
			break;
		case msh_DEBUG:
#ifdef MSH_DEBUG_PERF
			mexPrintf("Total time: " SIZE_FORMAT_SPEC "\nTime waiting for the lock: " SIZE_FORMAT_SPEC "\nTime spent in busy wait: " SIZE_FORMAT_SPEC "\n",
			g_shared_info->debug_perf.total_time, g_shared_info->debug_perf.lock_time, g_shared_info->debug_perf.busy_wait_time);
#endif
			break;
		case msh_CLEAR:
			
			msh_Clear(num_in_vars, in_vars);
			
			break;
		case msh_RESET:
			
			msh_AcquireProcessLock(g_process_lock);
				msh_Clear(0, NULL);
				msh_SetDefaultConfiguration();
				msh_WriteConfiguration();
			msh_ReleaseProcessLock(g_process_lock);
			
			break;
			
		case msh_OVERWRITE:
		case msh_SAFEOVERWRITE:
			
			if(nrhs == 2)
			{
				msh_Overwrite(prhs[0], prhs[1], directive);
			}
			else
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "IncorrectNumberOfInputsError", "Incorrect number of inputs. Please use one of the entry functions provided.");
			}
			
			break;
		
		case msh_LOCK:
			
			msh_AcquireProcessLock(g_process_lock);
			
			break;
		case msh_UNLOCK:
			
			msh_ReleaseProcessLock(g_process_lock);
			
			break;
		default:
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "UnknownDirectiveError", "Unrecognized matshare directive. Please use the supplied entry functions.");
			break;
	}

#ifdef MSH_DEBUG_PERF
	if(g_local_info.shared_info_wrapper.ptr != NULL)
	{
		msh_GetTick(&total_time.new);
		msh_AtomicAddSizeWithMax(&g_shared_info->debug_perf.total_time, msh_GetTickDifference(&total_time), SIZE_MAX);
	}
#endif

}


void msh_Share(int nlhs, mxArray** plhs, int num_vars, const mxArray** in_vars)
{
	SegmentNode_t* new_seg_node;
	const mxArray* in_var;
	int i;
	
	/* check the inputs */
	if(num_vars < 1)
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "NoVariableError", "No variable supplied to clone.");
	}
	
	for(i = 0, in_var = in_vars[i]; i < num_vars; i++, in_var = in_vars[i])
	{
		
		if(g_shared_info->user_defined.sharetype == msh_SHARETYPE_OVERWRITE)
		{
			/* if the variable is exactly the same size, perform a rewrite */
			if(g_local_seg_list.last != NULL && (msh_CompareHeaderSize(msh_GetSegmentData(g_local_seg_list.last), in_var) == TRUE))
			{
				/* this is an in-place change */
				/* do the rewrite after checking because the comparison is cheap */
				msh_OverwriteHeader(msh_GetSegmentData(g_local_seg_list.last), in_var);

#ifdef MSH_UNIX
				/* only need to update because we have overwritten an established segment */
				if(msync(msh_GetSegmentInfo(g_local_seg_list.last)->raw_ptr, msh_GetSegmentInfo(g_local_seg_list.last)->total_segment_size, MS_SYNC | MS_INVALIDATE) != 0)
				{
					/* error severity: system */
					meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "MsyncMatlabError", "There was an error with syncing a shared data segment.");
				}
#endif
				
				/* DON'T DO ANYTHING ELSE */
				continue;
			}
		}
		
		/* scan input data to get required size and create the segment */
		new_seg_node = msh_CreateSegment(msh_FindSharedSize(in_var));
		
		/* copy data to the shared memory */
		msh_CopyVariable(msh_GetSegmentData(new_seg_node), in_var);
		
		/* segment must also be tracked locally, so do that now */
		msh_AddSegmentToList(&g_local_seg_list, new_seg_node);
		
		/* Add the new segment to the shared list */
		msh_AddSegmentToSharedList(new_seg_node);
		
	}
	
	if(nlhs == 1)
	{
		/* Returns the variable we just put into shared memory. At least one segment must have been created. */
		if(msh_GetVariableNode(g_local_seg_list.last) == NULL)
		{
			msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(g_local_seg_list.last));
		}
		plhs[0] = mxCreateSharedDataCopy(msh_GetVariableData(msh_GetVariableNode(g_local_seg_list.last)));
	}
	else if(nlhs > 1)
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "TooManyOutputsError", "Too many outputs were requested.");
	}
	
}


void msh_Fetch(int nlhs, mxArray** plhs, msh_directive_t directive)
{
	
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;

#if MSH_BITNESS==64
	size_t i, num_new_vars = 0;
	size_t ret_dims[2];
#elif MSH_BITNESS==32
	int i, num_new_vars = 0;
	int ret_dims[2];
#endif
	
	if(nlhs >= 1)
	{
		if(nlhs >= 2)
		{
			
			/* perform a full update */
			msh_UpdateSegmentTracking(&g_local_seg_list);
			
			for(i = 0, curr_seg_node = g_local_seg_list.first;
			    (uint32_T)i < g_local_seg_list.num_segs;
					i++, curr_seg_node = msh_GetNextSegment(curr_seg_node))
			{
				if(msh_GetVariableNode(curr_seg_node) == NULL)
				{
					/* create the variable node if it hasnt been created yet */
					msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(curr_seg_node));
					num_new_vars += 1;
				}
			}
			
			/* return the new variables detected */
#if MSH_BITNESS==64
			ret_dims[0] = (size_t)(num_new_vars != 0);
#elif MSH_BITNESS==32
			ret_dims[0] = (int)(num_new_vars != 0);
#endif
			
			ret_dims[1] = num_new_vars;
			plhs[1] = mxCreateCellArray(2, ret_dims);
			
			for(i = 0, curr_var_node = g_local_var_list.last;
					i < num_new_vars;
					i++, curr_var_node = msh_GetPreviousVariable(curr_var_node))
			{
				if(directive == msh_LOCALCOPY)
				{
					mxSetCell(plhs[1], num_new_vars - 1 - i, mxDuplicateArray(msh_GetVariableData(curr_var_node)));
				}
				else
				{
					mxSetCell(plhs[1], num_new_vars - 1 - i, mxCreateSharedDataCopy(msh_GetVariableData(curr_var_node)));
				}
			}
			
			if(nlhs >= 3)
			{
				
				if(nlhs > 3)
				{
					meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "TooManyOutputsError", "Too many outputs. Got %d, need between 0 and 3.", nlhs);
				}
				
				/* retrieve all segment variables */
				ret_dims[0] = (size_t)(g_local_seg_list.num_segs != 0);
				ret_dims[1] = g_local_seg_list.num_segs;
				plhs[2] = mxCreateCellArray(2, ret_dims);
				
				for(i = 0, curr_seg_node = g_local_seg_list.first;
				    		(uint32_T)i < g_local_seg_list.num_segs;
						i++, curr_seg_node = msh_GetNextSegment(curr_seg_node))
				{
					if(directive == msh_LOCALCOPY)
					{
						mxSetCell(plhs[2], i, mxDuplicateArray(msh_GetVariableData(msh_GetVariableNode(curr_seg_node))));
					}
					else
					{
						mxSetCell(plhs[2], i, mxCreateSharedDataCopy(msh_GetVariableData(msh_GetVariableNode(curr_seg_node))));
					}
				}
			}
		}
		else
		{
			
			/* only update the latest segment */
			msh_UpdateLatestSegment(&g_local_seg_list);
			
			if(g_local_seg_list.last != NULL && msh_GetVariableNode(g_local_seg_list.last) == NULL)
			{
				msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(g_local_seg_list.last));
			}
		}
		
		if(g_local_seg_list.last != NULL)
		{
			if(directive == msh_LOCALCOPY)
			{
				plhs[0] = mxDuplicateArray(msh_GetVariableData(msh_GetVariableNode(g_local_seg_list.last)));
			}
			else
			{
				plhs[0] = mxCreateSharedDataCopy(msh_GetVariableData(msh_GetVariableNode(g_local_seg_list.last)));
			}
		}
		else
		{
			plhs[0] = mxCreateDoubleMatrix(0, 0, mxREAL);
		}
		
	}
	
}


void msh_Clear(int num_inputs, const mxArray** in_vars)
{
	
	VariableNode_t* curr_var_node;
	mxArray* link;
	bool_t found_variable;
	int input_num;
	msh_AcquireProcessLock(g_process_lock);
	
	if(num_inputs > 0)
	{
		
		msh_UpdateSegmentTracking(&g_local_seg_list);
		
		for(input_num = 0; input_num < num_inputs; input_num++)
		{
			for(curr_var_node = g_local_var_list.first, found_variable = FALSE; curr_var_node != NULL && !found_variable; curr_var_node = msh_GetNextVariable(curr_var_node))
			{
				/* begin hack */
				link = msh_GetVariableData(curr_var_node);
				do
				{
					if(link == in_vars[input_num])
					{
						msh_RemoveSegmentFromSharedList(msh_GetSegmentNode(curr_var_node));
						msh_RemoveSegmentFromList(msh_GetSegmentNode(curr_var_node));
						msh_DetachSegment(msh_GetSegmentNode(curr_var_node));
						found_variable = TRUE;
						break;
					}
					link = msh_GetCrosslink(link);
				} while(link != NULL && link != msh_GetVariableData(curr_var_node));
				/* end hack */
			}
		}
	}
	else
	{
		msh_ClearSharedSegments(&g_local_seg_list);
	}
	msh_ReleaseProcessLock(g_process_lock);
}


void msh_Overwrite(const mxArray* dest_var, const mxArray* in_var, msh_directive_t directive)
{
	if(msh_CompareVariableSize(dest_var, in_var))
	{
		if(directive == msh_SAFEOVERWRITE)
		{
			msh_AcquireProcessLock(g_process_lock);
		}
		
		msh_OverwriteVariable(dest_var, in_var);
		
		if(directive == msh_SAFEOVERWRITE)
		{
			msh_ReleaseProcessLock(g_process_lock);
		}
	}
	else
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "VariableSizeError", "The size of the variable to be overwritten was incompatible with the input variable.");
	}
}


void msh_Config(int num_params, const mxArray** in)
{
	int i, j, ps_len, vs_len;
	char param_str[MSH_MAX_NAME_LEN] = {0}, val_str[MSH_MAX_NAME_LEN] = {0}, param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
#ifdef MSH_UNIX
	SegmentNode_t* curr_seg_node;
#endif
	
	if(num_params == 0)
	{
#ifdef MSH_WIN
		mexPrintf(MSH_PARAM_INFO,
				msh_GetCounterFlag(&g_shared_info->user_defined.lock_counter)? "true" : "false",
				g_shared_info->user_defined.sharetype == msh_SHARETYPE_COPY? "true" : "false",
				g_shared_info->user_defined.will_gc? "true" : "false");
#else
		mexPrintf(MSH_PARAM_INFO,
				msh_GetCounterFlag(&g_shared_info->user_defined.lock_counter)? "true" : "false",
				g_shared_info->user_defined.sharetype == msh_SHARETYPE_COPY? "true" : "false",
				g_shared_info->user_defined.will_gc? "true" : "false",
				g_shared_info->user_defined.security);
#endif
	}
	
	for(i = 0; i < num_params; i++)
	{
		param = in[2*i];
		val = in[2*i + 1];
		
		if(!mxIsChar(param) || !mxIsChar(val))
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidArgumentError", "All parameters and values must be input as character arrays.");
		}
		
		ps_len = (int)mxGetNumberOfElements(param);
		vs_len = (int)mxGetNumberOfElements(val);
		
		if(ps_len + 1 > MSH_MAX_NAME_LEN)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		else if(vs_len + 1 > MSH_MAX_NAME_LEN)
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, param_str);
		}
		
		mxGetString(param, param_str, MSH_MAX_NAME_LEN);
		mxGetString(val, val_str, MSH_MAX_NAME_LEN);
		
		for(j = 0; j < ps_len; j++)
		{
			param_str_l[j] = (char)tolower(param_str[j]);
		}
		
		for(j = 0; j < vs_len; j++)
		{
			val_str_l[j] = (char)tolower(val_str[j]);
		}
		
		if(strcmp(param_str_l, MSH_PARAM_SHARETYPE_L) == 0 || strcmp(param_str_l, MSH_PARAM_SHARETYPE_AB) == 0)
		{
			if(strncmp(val_str_l, "copy", 4) == 0)
			{
				g_shared_info->user_defined.sharetype = msh_SHARETYPE_COPY;
			}
			else if(strcmp(val_str_l, "overwrite") == 0)
			{
				g_shared_info->user_defined.sharetype = msh_SHARETYPE_OVERWRITE;
			}
			else
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_SHARETYPE);
			}
		}
		else if(strcmp(param_str_l, MSH_PARAM_THREADSAFETY_L) == 0 || strcmp(param_str_l, MSH_PARAM_THREADSAFETY_AB) == 0)
		{
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				msh_WaitSetCounter(&g_shared_info->user_defined.lock_counter, TRUE);
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				msh_WaitSetCounter(&g_shared_info->user_defined.lock_counter, FALSE);
			}
			else
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_THREADSAFETY);
			}
		}
		else if(strcmp(param_str_l, MSH_PARAM_MAX_VARIABLES_L) == 0 || strcmp(param_str_l, MSH_PARAM_MAX_VARIABLES_AB) == 0)
		{
			g_shared_info->user_defined.max_shared_segments = strtoul(val_str_l, NULL, 10);
		}
		else if(strcmp(param_str_l, MSH_PARAM_MAX_SIZE_L) == 0 || strcmp(param_str_l, MSH_PARAM_MAX_SIZE_AB) == 0)
		{
#if MSH_BITNESS==64
			g_shared_info->user_defined.max_shared_size = strtoull(val_str_l, NULL, 10);
#elif MSH_BITNESS==32
			g_shared_info->user_defined.max_shared_size = strtoul(val_str_l, NULL, 10);
#endif
		}
		else if(strcmp(param_str_l, MSH_PARAM_GC_L) == 0 || strcmp(param_str_l, MSH_PARAM_GC_AB) == 0)
		{
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				g_shared_info->user_defined.will_gc = TRUE;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_shared_info->user_defined.will_gc = FALSE;
			}
			else
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_GC);
			}
		}
		else if(strcmp(param_str_l, MSH_PARAM_SECURITY_L) == 0 || strcmp(param_str_l, MSH_PARAM_SECURITY_AB) == 0)
		{
#ifdef MSH_UNIX
			if(vs_len < 3 || vs_len > 4)
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidValueError", "Too many or too few digits in \"%s\" for parameter \"%s\". Must have either 3 or 4 digits.",
							   val_str, MSH_PARAM_SECURITY);
			}
			else
			{
				msh_AcquireProcessLock(g_process_lock);
				g_shared_info->user_defined.security = (mode_t)strtol(val_str_l, NULL, 8);
				if(fchmod(g_local_info.shared_info_wrapper.handle, g_shared_info->user_defined.security) != 0)
				{
					meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "ChmodError", "There was an error modifying permissions for the shared info segment.");
				}
				for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
				{
					if(fchmod(msh_GetSegmentInfo(curr_seg_node)->handle, g_shared_info->user_defined.security) != 0)
					{
						meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_SYSTEM, errno, "ChmodError", "There was an error modifying permissions for the data segment.");
					}
				}
				msh_ReleaseProcessLock(g_process_lock);;
			}
#else
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", param_str);
#endif
		}
		else
		{
			meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
	}
	
}
