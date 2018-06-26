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

char_t* g_msh_library_name = "matshare";
char_t* g_msh_error_help_message = "";
char_t* g_msh_warning_help_message = "";

LocalInfo_t g_local_info = {
							0,                          /* rev_num */
							0,                          /* lock_level */
							0,                          /* this_pid */
							{
								NULL,                  /* ptr */
								MSH_INVALID_HANDLE     /* handle */
							},                          /* shared_info_wrapper */
#ifdef MSH_WIN
							MSH_INVALID_HANDLE,         /* process_lock */
#else
							MSH_INVALID_HANDLE,         /* process_lock */
							0,                          /* lock_size */
#endif
							FALSE,                      /* is_mex_locked */
							FALSE,                      /* is_initialized */
							TRUE                        /* is_deinitialized */
						};

VariableList_t g_local_var_list = {0};
VariableList_t g_virtual_scalar_list = {0};
SegmentList_t g_local_seg_list = {0};

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	/* number of inputs other than the msh_directive */
	int num_in_args = nrhs - 1;
	
	/* inputs */
	const mxArray* in_directive = prhs[0];
	const mxArray** in_args = prhs + 1;
	
	/* resultant matshare directive */
	msh_directive_t directive;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "NotEnoughInputsError", "Minimum input arguments missing. You must supply a directive.");
	}
	
	/* get the directive, no check is made since the matshare should not be used without entry functions */
	directive = (msh_directive_t)mxGetScalar(in_directive);
	
	/* if we are printing out debug information do not change anything, but print out info and exit */
	if(directive == msh_DEBUG)
	{
		/* first print out local information */
		mexPrintf(MSH_DEBUG_LOCAL_FORMAT, MSH_DEBUG_LOCAL_ARGS);
		
		/* now print out shared information if available */
		if(g_local_info.shared_info_wrapper.ptr != NULL)
		{
			mexPrintf(MSH_DEBUG_SHARED_FORMAT, MSH_DEBUG_SHARED_ARGS);
#ifdef MSH_DEBUG_PERF
			mexPrintf("Total time: " SIZE_FORMAT "\nTime waiting for the lock: " SIZE_FORMAT "\nTime spent in busy wait: " SIZE_FORMAT "\n", g_shared_info->debug_perf.total_time,
					g_shared_info->debug_perf.lock_time, g_shared_info->debug_perf.busy_wait_time);
#endif
		}
		
		/* return immediately */
		return;
	}
	
	/* Run the detach operation first */
	if(directive == msh_DETACH)
	{
		msh_OnExit();
		return;
	}
	
	/* run initialization */
	msh_InitializeMatshare();
	
	if(g_shared_info->has_fatal_error)
	{
#ifdef MSH_WIN
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_FATAL, 0, "FatalError", "There was previously a fatal error in the matshare system. Please restart MATLAB.");
#else
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_FATAL, 0, "FatalError", "There was previously a fatal error in the matshare system. Please restart MATLAB. You may have to delete files "
																	"located in /dev/shm/.");
#endif
	}

#ifdef MSH_DEBUG_PERF
	msh_GetTick(&total_time.old);
#endif
	
	/* clean invalid segments out of tracking */
	msh_CleanSegmentList(&g_local_seg_list, directive == msh_CLEAN);
	
	switch(directive)
	{
		case msh_SHARE:
		case msh_PERSISTSHARE:
			
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, 0, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			
			msh_Share(num_in_args-1, in_args+1, (int)mxGetScalar(in_args[0]), directive);
			
			break;
		
		case msh_FETCH:
			
			msh_Fetch(nlhs, plhs);
			
			break;
			/*
		case msh_DETACH:
			break;
			 */
		case msh_CONFIG:
			
			if(num_in_args % 2 != 0)
			{
				meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "InvalidNumArgsError", "Each parameter must have a value associated to it.");
			}
			
			msh_Config(num_in_args/2, in_args);
			
			break;
		
		case msh_LOCALCOPY:
			
			if(num_in_args == 0)
			{
				msh_LocalCopy(nlhs, plhs);
			}
			else if(num_in_args == 1)
			{
				if(nlhs == 1)
				{
					plhs[0] = mxDuplicateArray(in_args[0]);
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
		/*
		case msh_DEBUG:
			break;
		 */
		case msh_CLEAR:
			
			msh_Clear(num_in_args, in_args);
			
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
			
			if(nrhs == 3)
			{
				msh_Overwrite(prhs[1], prhs[2], directive);
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
		case msh_VIRTUALRESIZE:
			
			msh_VirtualResize();
			
			break;
		case msh_CLEAN:
			/* do nothing, operation was performed above */
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


void msh_Share(int num_vars, const mxArray** in_vars, int return_expected, msh_directive_t directive)
{
	SegmentNode_t* new_seg_node = NULL;
	VariableNode_t* new_var_node = NULL;
	int input_num, is_persistent = (directive == msh_PERSISTSHARE);
	
	/* check the inputs */
	if(num_vars < 1)
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "NoVariableError", "No variable supplied to share.");
	}
	
	for(input_num = 0; input_num < num_vars; input_num++)
	{
		/* scan input data to get required size and create the segment */
		new_seg_node = msh_CreateSegment(msh_FindSharedSize(in_vars[input_num]), is_persistent);
		
		/* copy data to the shared memory */
		msh_CopyVariable(msh_GetSegmentData(new_seg_node), in_vars[input_num], TRUE);
		
		/* segment must also be tracked locally, so do that now */
		msh_AddSegmentToList(&g_local_seg_list, new_seg_node);
		
		/* Add the new segment to the shared list */
		msh_AddSegmentToSharedList(new_seg_node);
	}
	
	if(return_expected)
	{
		/* Returns the variable we just put into shared memory. At least one segment must have been created. */
		if((new_var_node = msh_GetVariableNode(new_seg_node)) == NULL)
		{
			new_var_node = msh_CreateVariable(new_seg_node);
			msh_AddVariableToList(&g_local_var_list, new_var_node);
		}
		met_PutSharedCopy("caller", "shared", msh_CreateSharedDataCopy(new_var_node));
	}
	
}


void msh_Fetch(int nlhs, mxArray** plhs)
{
	
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;
	
	uint32_T i, num_new_vars = 0;
	
	if(nlhs >= 0)
	{
		if(nlhs >= 1)
		{
			
			/* perform a full update */
			msh_UpdateSegmentTracking(&g_local_seg_list);
			
			for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
			{
				if(msh_GetVariableNode(curr_seg_node) == NULL)
				{
					/* create the variable node if it hasnt been created yet */
					msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(curr_seg_node));
					num_new_vars += 1;
				}
			}
			
			/* return the new variables detected */
			plhs[0] = mxCreateCellMatrix((size_t)(num_new_vars != 0), num_new_vars);
			
			for(i = 0, curr_var_node = g_local_var_list.last;
					i < num_new_vars;
					i++, curr_var_node = msh_GetPreviousVariable(curr_var_node))
			{
				mxSetCell(plhs[0], num_new_vars - 1 - i, msh_CreateSharedDataCopy(curr_var_node));
			}
			
			if(nlhs >= 2)
			{
				
				if(nlhs > 2)
				{
					meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "TooManyOutputsError", "Too many outputs. Got %d, need between 0 and 2.", nlhs);
				}
				
				/* retrieve all segment variables */
				plhs[1] = mxCreateCellMatrix((size_t)(g_local_seg_list.num_segs != 0), g_local_seg_list.num_segs);
				
				
				for(i = 0, curr_seg_node = g_local_seg_list.first;
				    		i < g_local_seg_list.num_segs;
						i++, curr_seg_node = msh_GetNextSegment(curr_seg_node))
				{
					mxSetCell(plhs[1], i, msh_CreateSharedDataCopy(msh_GetVariableNode(curr_seg_node)));
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
			met_PutSharedCopy("caller", "latest", msh_CreateSharedDataCopy(msh_GetVariableNode(g_local_seg_list.last)));
		}
		/* do nothing otherwise; the variable is set in the workspace */
		
	}
	
}


void msh_LocalCopy(int nlhs, mxArray** plhs)
{
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;
	
	uint32_T i, num_new_vars = 0;
	
	if(nlhs >= 1)
	{
		if(nlhs >= 2)
		{
			
			/* perform a full update */
			msh_UpdateSegmentTracking(&g_local_seg_list);
			
			for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
			{
				if(msh_GetVariableNode(curr_seg_node) == NULL)
				{
					/* create the variable node if it hasnt been created yet */
					msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(curr_seg_node));
					num_new_vars += 1;
				}
			}
			
			/* return the new variables detected */
			plhs[1] = mxCreateCellMatrix((size_t)(num_new_vars != 0), num_new_vars);
			
			for(i = 0, curr_var_node = g_local_var_list.last;
			    i < num_new_vars;
			    i++, curr_var_node = msh_GetPreviousVariable(curr_var_node))
			{
				mxSetCell(plhs[1], num_new_vars - 1 - i, mxDuplicateArray(msh_GetVariableData(curr_var_node)));
			}
			
			if(nlhs >= 3)
			{
				
				if(nlhs > 3)
				{
					meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "TooManyOutputsError", "Too many outputs. Got %d, need between 0 and 2.", nlhs);
				}
				
				/* retrieve all segment variables */
				plhs[2] = mxCreateCellMatrix((size_t)(g_local_seg_list.num_segs != 0), g_local_seg_list.num_segs);
				
				
				for(i = 0, curr_seg_node = g_local_seg_list.first;
				    i < g_local_seg_list.num_segs;
				    i++, curr_seg_node = msh_GetNextSegment(curr_seg_node))
				{
					mxSetCell(plhs[2], i, mxDuplicateArray(msh_GetVariableData(msh_GetVariableNode(curr_seg_node))));
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
			plhs[0] = mxDuplicateArray(msh_GetVariableData(msh_GetVariableNode(g_local_seg_list.last)));
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
					link = met_GetCrosslink(link);
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
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_USER, 0, "VariableSizeError", "The size of the variable to be overwritten is incompatible with the input variable.");
	}
}


void msh_VirtualResize(void)
{
	VariableNode_t* curr_virtual_scalar;
	mxArray* link,* resize_var;
	mwSize* resize_dims;
	size_t resize_num_dims;
	for(curr_virtual_scalar = g_virtual_scalar_list.first; curr_virtual_scalar != NULL; curr_virtual_scalar = msh_GetNextVariable(curr_virtual_scalar))
	{
		/* do the resizing operation */
		resize_var = msh_GetVariableData(curr_virtual_scalar);
		resize_dims = msh_GetDimensions(msh_GetSharedHeader(curr_virtual_scalar));
		resize_num_dims = msh_GetNumDims(msh_GetSharedHeader(curr_virtual_scalar));
		
		if(msh_GetIsEmpty(msh_GetSharedHeader(curr_virtual_scalar)))
		{
			for(link = met_GetCrosslink(resize_var); link != NULL && link != resize_var; link = met_GetCrosslink(link))
			{
				mxSetDimensions(link, resize_dims, resize_num_dims);
				mxSetData(link, NULL);
			}
		}
		else
		{
			for(link = met_GetCrosslink(resize_var); link != NULL && link != resize_var; link = met_GetCrosslink(link))
			{
				mxSetDimensions(link, resize_dims, resize_num_dims);
			}
		}
	}
}


void msh_Config(int num_params, const mxArray** in_params)
{
	int i, j, ps_len, vs_len;
	char param_str[MSH_MAX_NAME_LEN] = {0}, val_str[MSH_MAX_NAME_LEN] = {0}, param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
#ifdef MSH_UNIX
	SegmentNode_t* curr_seg_node;
#endif
	
	if(num_params == 0)
	{
		mexPrintf(MSH_CONFIG_STRING_FORMAT,
				msh_GetCounterFlag(&g_shared_info->user_defined.lock_counter)? "true" : "false",
				g_shared_info->user_defined.will_shared_gc? "true" : "false");
#ifdef MSH_UNIX
		mexPrintf(MSH_CONFIG_SECURITY_STRING_FORMAT, g_shared_info->user_defined.security);
#endif
	}
	
	for(i = 0; i < num_params; i++)
	{
		param = in_params[2*i];
		val = in_params[2*i + 1];
		
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
		
		if(strcmp(param_str_l, MSH_PARAM_THREADSAFETY_L) == 0 || strcmp(param_str_l, MSH_PARAM_THREADSAFETY_AB) == 0)
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
				g_shared_info->user_defined.will_shared_gc = TRUE;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_shared_info->user_defined.will_shared_gc = FALSE;
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
				msh_ReleaseProcessLock(g_process_lock);
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
