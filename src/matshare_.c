/** matshare_.c
 * Defines the MEX entry function and other top level functions.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "mex.h"

#include "matshare_.h"
#include "mshheader.h"
#include "mshexterntypes.h"
#include "mshtypes.h"
#include "mlerrorutils.h"
#include "mshutils.h"
#include "mshvariables.h"
#include "mshsegments.h"
#include "mshinit.h"
#include "mshlockfree.h"

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
		                      MSH_INITIAL_STATE,          /* rev_num */
		                      0,                          /* lock_level */
		                      0,                          /* this_pid */
		                      {
		                           NULL,                  /* ptr */
		                           MSH_INVALID_HANDLE     /* handle */
		                      },                          /* shared_info_wrapper */
#ifdef MSH_WIN
                                MSH_INVALID_HANDLE,         /* process_lock */
#else
		                      {
		                           MSH_INVALID_HANDLE,         /* process_lock */
		                           0,                          /* lock_size */
		                      },
#endif
		                      FALSE,                      /* has_fatal_error */
		                      FALSE,                      /* is_initialized */
		                      TRUE                        /* is_deinitialized */
                           };

VariableList_t g_local_var_list = {0};
VariableList_t g_virtual_scalar_list = {0};
SegmentList_t g_local_seg_list = {0};

static const char s_out_id_recent[] = "recent";
static const char s_out_id_new[] = "new";
static const char s_out_id_all[] = "all";

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
	
	/* check the local struct for fatal errors */
	if(g_local_info.has_fatal_error)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_FATAL, "FatalError", "There was previously a fatal error in the matshare system. To continue using matshare please restart MATLAB."
#ifdef MSH_UNIX
				"You may have to delete files located in /dev/shm/."
#endif
												  );
	}
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Minimum input arguments missing. You must supply a directive.");
	}
	
	/* get the directive, no check is made since the matshare should not be used without entry functions */
	directive = (msh_directive_t)mxGetScalar(in_directive);
	
	
	/* pre-initialization directives */
	switch(directive)
	{
		case msh_DEBUG:
		{
			/* first print out local information */
			mexPrintf(MSH_DEBUG_LOCAL_FORMAT, MSH_DEBUG_LOCAL_ARGS);
			
			/* now print out shared information if available */
			if(g_local_info.shared_info_wrapper.ptr != NULL)
			{
				mexPrintf(MSH_DEBUG_SHARED_FORMAT, MSH_DEBUG_SHARED_ARGS);
			}
			return;
		}
		case msh_INFO:
		{
			mexPrintf("\n<strong>Information on the current state of matshare:</strong>\n");
			if(g_local_info.is_deinitialized)
			{
				mexPrintf("    matshare is fully deinitialized for this process.\n");
			}
			else
			{
				if(g_local_info.is_initialized)
				{
					mexPrintf("    Current number of shared variables:  %lu\n"
			                    "    Current total size of shared memory: "SIZE_FORMAT" bytes\n"
							"    Process ID of the most recent usage: %lu\n",
					          g_shared_info->num_shared_segments,
					          g_shared_info->total_shared_size,
					          g_shared_info->update_pid);
					mexPrintf(MSH_CONFIG_STRING_FORMAT "\n",
					          msh_GetCounterCount(&g_shared_info->user_defined.lock_counter),
					          msh_GetCounterFlag(&g_shared_info->user_defined.lock_counter)? "on" : "off",
					          g_shared_info->user_defined.max_shared_segments,
					          g_shared_info->user_defined.max_shared_size,
					          g_shared_info->user_defined.will_shared_gc? "on" : "off");
#ifdef MSH_UNIX
					mexPrintf(MSH_CONFIG_SECURITY_STRING_FORMAT, g_shared_info->user_defined.security);
#endif
				}
				else
				{
					mexPrintf("    matshare was interrupted in the middle of initialization or deinitialization.\n");
				}
			}
			return;
		}
		case msh_DETACH:
		{
			msh_OnExit();
			return;
		}
		default:
			break;
	}
	
	/* run initialization */
	msh_InitializeMatshare();
	
	/* check the shared segment for fatal errors */
	if(g_shared_info->has_fatal_error)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_FATAL, "FatalError", "There was previously a fatal error in the matshare system. To continue using matshare please restart MATLAB."
#ifdef MSH_UNIX
				"You may have to delete files located in /dev/shm/."
#endif
		                 );
	}
	
	/* clean invalid segments out of tracking */
	msh_CleanSegmentList(&g_local_seg_list, directive == msh_CLEAN);
	
	switch(directive)
	{
		case msh_SHARE:
		case msh_PSHARE:
		{
			/* we use varargin in this case and extract the result */
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Share(nlhs, plhs, mxGetNumberOfElements(in_args[0]), mxGetData(in_args[0]), directive);
			break;
		}
		case msh_FETCH:
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Fetch(nlhs, plhs, mxGetNumberOfElements(in_args[0]), mxGetData(in_args[0]));
			break;
		case msh_COPY:
		{
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Copy(nlhs, plhs,  mxGetNumberOfElements(in_args[0]), mxGetData(in_args[0]));
			break;
		}
		/*
		case msh_DETACH:
			break;
		*/
		case msh_CONFIG:
		{
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Config(mxGetNumberOfElements(in_args[0])/2, mxGetData(in_args[0]));
			break;
		}
		/*
		case msh_DEBUG:
			break;
		 */
		case msh_CLEAR:
		{
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Clear(mxGetNumberOfElements(in_args[0]), mxGetData(in_args[0]));
			break;
		}
		case msh_RESET:
		{
			msh_AcquireProcessLock(g_process_lock);
			{
				msh_Clear(0, NULL);
				msh_SetDefaultConfiguration();
				msh_WriteConfiguration();
			}
			msh_ReleaseProcessLock(g_process_lock);
			break;
		}
		case msh_OVERWRITE:
		{
			msh_Overwrite(num_in_args, in_args);
			break;
		}
		case msh_LOCK:
		{
			msh_AcquireProcessLock(g_process_lock);
			break;
		}
		case msh_UNLOCK:
		{
			msh_ReleaseProcessLock(g_process_lock);
			break;
		}
		/* operation was performed above
		case msh_CLEAN:
			break;
		*/
		/*
		case msh_INFO:
			break;
		*/
		default:
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownDirectiveError", "Unrecognized matshare directive. Please use the supplied entry functions.");
			break;
		}
	}

}


void msh_Share(int nlhs, mxArray** plhs, size_t num_vars, const mxArray** in_vars, msh_directive_t directive)
{
	SegmentNode_t* new_seg_node = NULL;
	VariableNode_t* new_var_node = NULL;
	size_t input_num;
	int  is_persistent = (directive == msh_PSHARE);
	
	/* check the inputs */
	if(num_vars < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NoVariableError", "No variable supplied to share.");
	}
	
	for(input_num = 0; input_num < num_vars; input_num++)
	{
		/* scan input data to get required size and create the segment */
		new_seg_node = msh_CreateSegment(msh_FindSharedSize(in_vars[input_num]), is_persistent);
		
		/* copy data to the shared memory */
		msh_CopyVariable(msh_GetSegmentData(new_seg_node), in_vars[input_num]);
		
		/* segment must also be tracked locally, so do that now */
		msh_AddSegmentToList(&g_local_seg_list, new_seg_node);
		
		/* Add the new segment to the shared list */
		msh_AddSegmentToSharedList(new_seg_node);
		
		/* create a shared variable to pass back to the caller */
		new_var_node = msh_CreateVariable(new_seg_node);
		
		/* add that variable to a tracking list */
		msh_AddVariableToList(&g_local_var_list, new_var_node);
		
		/* create and set the return */
		if(input_num < nlhs)
		{
			plhs[input_num] = msh_CreateOutput(msh_CreateSharedDataCopy(new_var_node, FALSE));
		}
		
	}
	
}


void msh_Fetch(int nlhs, mxArray** plhs, size_t num_args, const mxArray** in_args)
{
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;
	mxArray* recent_out, * new_out, * all_out;
	mxChar* input_option;
	size_t i, num_new_vars = 0;
	
	int num_outs = 0;
	struct
	{
		bool_t recent;
		bool_t new;
		bool_t all;
	} out_flags = {0};
	const char* out_names[3];
	
	if(nlhs != 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumOutputsError", "Too many or too few outputs were requested from the fetch operation. Please use the provided entry functions.");
	}
	
	/* parse the input */
	for(i = 0; i < num_args && num_outs < 3; i++)
	{
		input_option = mxGetChars(in_args[i]);
		if(input_option[0] == '-')
		{
			switch(input_option[1])
			{
				case 'r':
				{
					/* recent */
					out_flags.recent = TRUE;
					out_names[num_outs] = s_out_id_recent;
					break;
				}
				case 'n':
				{
					/* new */
					out_flags.new = TRUE;
					out_names[num_outs] = s_out_id_new;
					break;
				}
				case 'a':
				{
					/* all */
					out_flags.all = TRUE;
					out_names[num_outs] = s_out_id_all;
					break;
				}
				default:
				{
					goto OPTION_ERROR;
				}
			}
			num_outs += 1;
		}
		else
		{
			OPTION_ERROR:
			{
				/* normally the string produced here should be freed with mxFree, but since we have an error just let MATLAB do the GC */
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedOptionError", "Unrecognized option %s", mxArrayToString(in_args[i]));
			}
		}
	}
	
	/* if none were set then set all to true */
	if(!out_flags.recent && !out_flags.new && !out_flags.all)
	{
		num_outs = 3;
		
		out_flags.recent = TRUE;
		out_names[0] = s_out_id_recent;
		
		out_flags.new = TRUE;
		out_names[1] = s_out_id_new;
		
		out_flags.all = TRUE;
		out_names[2] = s_out_id_all;
	}
	
	/* create the return struct */
	plhs[0] = mxCreateStructMatrix(1, 1, num_outs, out_names);
	
	/* check how we should update */
	if(out_flags.recent && !(out_flags.new || out_flags.all))
	{
		/* only update the most recent (fast-track) */
		msh_UpdateLatestSegment(&g_local_seg_list);
		
		if(g_local_seg_list.last != NULL)
		{
			if(msh_GetVariableNode(g_local_seg_list.last) == NULL)
			{
				msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(g_local_seg_list.last));
			}
		}
	}
	else if(out_flags.new || out_flags.all)
	{
		/* update everything */
		msh_UpdateSegmentTracking(&g_local_seg_list);
		
		/* create backing variables for each segment */
		for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
		{
			if(msh_GetVariableNode(curr_seg_node) == NULL)
			{
				/* create the variable node if it hasnt been created yet */
				msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(curr_seg_node));
				num_new_vars += 1;
			}
		}
	}
	else
	{
		return;
	}
	
	if(out_flags.recent)
	{
		if(g_local_seg_list.last != NULL)
		{
			recent_out = mxCreateCellMatrix(1, 1);
			mxSetCell(recent_out, 0, msh_CreateOutput(msh_CreateSharedDataCopy(msh_GetVariableNode(g_local_seg_list.last), TRUE)));
			mxSetField(plhs[0], 0, s_out_id_recent, recent_out);
		}
	}
	
	if(out_flags.new)
	{
		/* return the new variables detected */
		if(num_new_vars > 0)
		{
			new_out = mxCreateCellMatrix(num_new_vars, 1);
			for(i = 0, curr_var_node = g_local_var_list.last; i < num_new_vars; i++, curr_var_node = msh_GetPreviousVariable(curr_var_node))
			{
				mxSetCell(new_out, num_new_vars - 1 - i, msh_CreateOutput(msh_CreateSharedDataCopy(curr_var_node, TRUE)));
			}
			mxSetField(plhs[0], 0, s_out_id_new, new_out);
		}
	}
	
	if(out_flags.all)
	{
		/* retrieve all segment variables */
		if(g_local_seg_list.num_segs > 0)
		{
			all_out = mxCreateCellMatrix(g_local_seg_list.num_segs, 1);
			for(i = 0, curr_seg_node = g_local_seg_list.first; i < g_local_seg_list.num_segs; i++, curr_seg_node = msh_GetNextSegment(curr_seg_node))
			{
				mxSetCell(all_out, i, msh_CreateOutput(msh_CreateSharedDataCopy(msh_GetVariableNode(curr_seg_node), TRUE)));
			}
			mxSetField(plhs[0], 0, s_out_id_all, all_out);
		}
	}
	
}

void msh_Copy(int nlhs, mxArray** plhs, size_t num_inputs, const mxArray** in_vars)
{
	size_t i, j, num_elems;
	
	if(num_inputs < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please supply at least one "
														 "<a href=\"matlab:helpPopup matshare\" style=\"font-weight:bold\">matshare object</a> "
			                                                        "to copy.");
	}
	
	for(i = 0; i < num_inputs; i++)
	{
		
		if(!mxIsClass(in_vars[i], "matshare"))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidInputError", "Invalid input. All arguments for the copy function must be <a href=\"matlab:helpPopup matshare\" "
													    "style=\"font-weight:bold\">matshare objects</a>.");
		}
		
		if(i < nlhs)
		{
			if((num_elems = mxGetNumberOfElements(in_vars[i])) == 1)
			{
				plhs[i] = mxGetProperty(in_vars[i], 0, "data");
			}
			else
			{
				plhs[i] = mxCreateCellArray(mxGetNumberOfDimensions(in_vars[i]), mxGetDimensions(in_vars[i]));
				for(j = 0; j < num_elems; j++)
				{
					mxSetCell(plhs[i], j, mxGetProperty(in_vars[i], j, "data"));
				}
			}
		}
		
	}
}


void msh_Clear(size_t num_inputs, const mxArray** in_vars)
{
	
	VariableNode_t* curr_var_node;
	mxArray* link;
	const mxArray* clear_var;
	bool_t found_variable;
	int input_num;
	
	msh_AcquireProcessLock(g_process_lock);
	if(num_inputs > 0)
	{
		
		msh_UpdateSegmentTracking(&g_local_seg_list);
		
		for(input_num = 0; input_num < num_inputs; input_num++)
		{
			clear_var = in_vars[input_num];
			
			if(!mxIsCell(clear_var) || mxGetNumberOfElements(clear_var) != 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidInputError", "Invalid input variable. Please use the entry functions provided.");
			}
			
			for(curr_var_node = g_local_var_list.first, found_variable = FALSE; curr_var_node != NULL && !found_variable; curr_var_node = msh_GetNextVariable(curr_var_node))
			{
				/* begin hack */
				link = msh_GetVariableData(curr_var_node);
				do
				{
					if(link == clear_var)
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


void msh_Overwrite(int num_args, const mxArray** in_args)
{
	mxArray* dest;
	const mxArray* in;
	mxChar* input_option;
	int will_lock = TRUE; /* will lock by default */
	
	if(num_args < 2 || num_args > 3)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumInputsError", "Too many inputs. The overwrite function takes either 2 or 3 arguments.");
	}
	
	if(num_args == 3)
	{
		input_option = mxGetChars(in_args[2]);
		if(input_option[0])
		{
			/* switch for possible future additions */
			switch(input_option[1])
			{
				case 's':
					/* safe */
					will_lock = TRUE;
					break;
				case 'u':
					/* unsafe */
					will_lock = FALSE;
					break;
				default:
				{
					goto OPTION_ERROR;
				}
			}
		}
		else
		{
			OPTION_ERROR:
			{
				/* normally the string produced here should be freed with mxFree, but since we have an error just let MATLAB do the GC */
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedOptionError", "Unrecognized option %s", mxArrayToString(in_args[2]));
			}
		}
	}
	
	if(!mxIsCell(in_args[0]) || mxGetNumberOfElements(in_args[0]) != 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidInputError", "Invalid input variable. Please use the entry functions provided.");
	}
	
	dest = mxGetCell(in_args[0], 0);
	in = in_args[1];
	
	if(msh_CompareVariableSize(dest, in))
	{
		if(will_lock)
		{
			msh_AcquireProcessLock(g_process_lock);
			{
				msh_OverwriteVariable(dest, in);
			}
			msh_ReleaseProcessLock(g_process_lock);
		}
		else
		{
			msh_OverwriteVariable(dest, in);
		}
	}
	else
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "VariableSizeError", "The size of the variable to be overwritten is incompatible with the input variable.");
	}
}


void msh_Config(size_t num_params, const mxArray** in_params)
{
	int i, j, ps_len, vs_len;
	char param_str[MSH_MAX_NAME_LEN] = {0}, val_str[MSH_MAX_NAME_LEN] = {0}, param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
	unsigned long maxvars_temp;
	size_t maxsize_temp;
#ifdef MSH_UNIX
	mode_t sec_temp;
	SegmentNode_t* curr_seg_node;
#endif
	
	if(num_params == 0)
	{
		mexPrintf(MSH_CONFIG_STRING_FORMAT,
				msh_GetCounterCount(&g_shared_info->user_defined.lock_counter),
				msh_GetCounterFlag(&g_shared_info->user_defined.lock_counter)? "on" : "off",
				g_shared_info->user_defined.max_shared_segments,
				g_shared_info->user_defined.max_shared_size,
				g_shared_info->user_defined.will_shared_gc? "on" : "off");
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
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidArgumentError", "All parameters and values must be input as character arrays.");
		}
		
		ps_len = (int)mxGetNumberOfElements(param);
		vs_len = (int)mxGetNumberOfElements(val);
		
		if(ps_len + 1 > MSH_MAX_NAME_LEN)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidParamError", "Unrecognised parameter \"%s\".", mxArrayToString(param));
		}
		else if(vs_len + 1 > MSH_MAX_NAME_LEN)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", mxArrayToString(val), mxArrayToString(param));
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
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_THREADSAFETY);
			}
		}
		else if(strncmp(param_str_l, MSH_PARAM_MAX_VARIABLES_L, 6) == 0 || strcmp(param_str_l, MSH_PARAM_MAX_VARIABLES_AB) == 0)
		{
			/* this can't be negative */
			if(val_str_l[0] == '-')
			{
				meu_PrintMexError(MEU_FL,
				                  MEU_SEVERITY_USER,
				                  "NegativeNumVarsError",
				                  "The maximum number of shared variables must be non-negative.");
			}
			
			errno = 0;
			maxvars_temp = strtoul(val_str_l, NULL, 0);
			if(errno)
			{
				meu_PrintMexError(MEU_FL,
				                  MEU_SEVERITY_USER | MEU_SEVERITY_SYSTEM | MEU_ERRNO,
				                  "MaxVarsParsingError",
				                  "There was an error parsing the value for the maximum number of shared variables.");
			}
			
			if(maxvars_temp == 0)
			{
				meu_PrintMexWarning("ZeroVariablesWarning", "The maximum number of variables has been set to zero. Please verify your input if this was not intentional.");
			}

#ifdef MSH_UNIX
			if(maxvars_temp > MSH_FD_HARD_LIMIT)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER | MEU_SEVERITY_INTERNAL, "MaxVarsRangeError",
				                  "The value input exceeds the hard limit of %lu shared variables. This limit will be removed in a future update.", MSH_FD_HARD_LIMIT);
			}
#endif
			g_shared_info->user_defined.max_shared_segments = maxvars_temp;
			
		}
		else if(strcmp(param_str_l, MSH_PARAM_MAX_SIZE_L) == 0 || strcmp(param_str_l, MSH_PARAM_MAX_SIZE_AB) == 0)
		{
			
			/* this can't be negative */
			if(val_str_l[0] == '-')
			{
				meu_PrintMexError(MEU_FL,
				                  MEU_SEVERITY_USER,
				                  "NegativeSizeError",
				                  "The maximum total size of shared memory must be non-negative.");
			}
			
			errno = 0;
#if MSH_BITNESS==64
			maxsize_temp = strtoull(val_str_l, NULL, 0);
#elif MSH_BITNESS==32
			maxsize_temp = strtoul(val_str_l, NULL, 0);
#endif
			if(errno)
			{
				meu_PrintMexError(MEU_FL,
				                  MEU_SEVERITY_USER | MEU_SEVERITY_SYSTEM | MEU_ERRNO,
				                  "MaxSizeParsingError",
				                  "There was an error parsing the value for the maximum total size of shared memory.");
			}
			
			if(maxsize_temp == 0)
			{
				meu_PrintMexWarning("ZeroSizeWarning", "The maximum size has been set to zero. Please verify your input if this was not intentional.");
			}
			g_shared_info->user_defined.max_shared_size = maxsize_temp;
			
		}
		else if(strncmp(param_str_l, MSH_PARAM_GC_L, 7) == 0 || strcmp(param_str_l, MSH_PARAM_GC_AB) == 0)
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
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_GC);
			}
		}
		else if(strcmp(param_str_l, MSH_PARAM_SECURITY_L) == 0 || strcmp(param_str_l, MSH_PARAM_SECURITY_AB) == 0)
		{
#ifdef MSH_UNIX
			if(vs_len < 3 || vs_len > 4)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidValueError", "Too many or too few digits in \"%s\" for parameter \"%s\". Must have either 3 or 4 digits.",
							   val_str, MSH_PARAM_SECURITY);
			}
			else
			{
				msh_AcquireProcessLock(g_process_lock);
				
				msh_UpdateSegmentTracking(&g_local_seg_list);
				
				sec_temp = (mode_t)strtol(val_str_l, NULL, 0);
				if(fchmod(g_local_info.shared_info_wrapper.handle, sec_temp) != 0)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ChmodError", "There was an error modifying permissions for the shared info segment.");
				}
				for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
				{
					if(fchmod(msh_GetSegmentInfo(curr_seg_node)->handle, sec_temp) != 0)
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "ChmodError", "There was an error modifying permissions for the data segment.");
					}
				}
				g_shared_info->user_defined.security = sec_temp;
				msh_ReleaseProcessLock(g_process_lock);
			}
#else
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", MSH_PARAM_SECURITY);
#endif
		}
		else
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
	}
	
}
