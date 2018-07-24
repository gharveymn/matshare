/** matshare_.c
 * Defines the MEX entry function and other top level functions.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "mex.h"

#include <ctype.h>

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

#ifdef MSH_UNIX
#  include <string.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif

char_t* g_msh_library_name = "matshare";
char_t* g_msh_error_help_message = "";
char_t* g_msh_warning_help_message = "";

LocalInfo_t g_local_info = {MSH_INITIAL_STATE,          /* rev_num */
                            0,                          /* lock_level */
                            0,                          /* this_pid */
                            {NULL,                  /* ptr */
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

SegmentTable_t g_seg_table = {NULL, 0, msh_GetSegmentHashByNumber, msh_CompareNumericKey};
SegmentTable_t g_name_table = {NULL, 0, msh_GetSegmentHashByName, msh_CompareStringKey};

SegmentList_t g_local_seg_list = {&g_seg_table, &g_name_table, NULL, NULL, 0, 0};

VariableList_t g_local_var_list = {NULL, NULL};

static mxArray* msh_WrapOutput(mxArray* shared_data_copy);

static mxArray* msh_CreateNamedOutput(const char_t* name);

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	/* inputs */
	const mxArray* in_directive = prhs[0];
	const mxArray** in_args = prhs + 1;
	
	/* number of inputs other than the msh_directive */
	int num_in_args = nrhs - 1;
	
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
		case(msh_DEBUG):
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
		case(msh_STATUS):
		{
			mexPrintf("\n<strong>Information on the current state of matshare:</strong>\n");
			if(g_local_info.is_deinitialized)
			{
				mexPrintf("    matshare is fully deinitialized for this process.\n\n");
			}
			else
			{
				if(g_local_info.is_initialized)
				{
					mexPrintf("    Number of shared variables:      %lu\n"
					          "    Total size of shared memory:     "SIZE_FORMAT" bytes\n"
					          "    PID of the most recent revision: %lu\n", g_shared_info->num_shared_segments, g_shared_info->total_shared_size, g_shared_info->update_pid);
					mexPrintf(MSH_CONFIG_STRING_FORMAT "\n",
					          msh_GetCounterFlag(&g_user_config.lock_counter)? "on" : "off",
					          g_user_config.max_shared_segments,
					          g_user_config.max_shared_size,
					          g_user_config.will_shared_gc? "on" : "off",
					          g_user_config.fetch_default);
#ifdef MSH_UNIX
					mexPrintf(MSH_CONFIG_SECURITY_STRING_FORMAT, g_user_config.security);
#endif
				}
				else
				{
					mexPrintf("    matshare was interrupted in the middle of initialization or deinitialization.\n\n");
				}
			}
			return;
		}
		case(msh_DETACH):
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
	
	msh_CleanSegmentList(&g_local_seg_list);
	
	switch(directive)
	{
		case(msh_SHARE):
		{
			/* we use varargin and extract the result */
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Share(nlhs, plhs, mxGetNumberOfElements(in_args[0]), mxGetData(in_args[0]));
			break;
		}
		case(msh_FETCH):
			/* we use varargin and extract the result */
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Fetch(nlhs, plhs, mxGetNumberOfElements(in_args[0]), mxGetData(in_args[0]));
			break;
		case(msh_COPY):
		{
			msh_Copy(nlhs, plhs, num_in_args, in_args);
			break;
		}
		case(msh_CONFIG):
		{
			if(num_in_args < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Config(mxGetNumberOfElements(in_args[0])/2, mxGetData(in_args[0]));
			break;
		}
		case(msh_CLEAR):
		{
			msh_Clear(num_in_args, in_args);
			break;
		}
		case(msh_RESET):
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
		case(msh_OVERWRITE):
		{
			msh_Overwrite(num_in_args, in_args);
			break;
		}
		case(msh_LOCK):
		{
			msh_AcquireProcessLock(g_process_lock);
			break;
		}
		case(msh_UNLOCK):
		{
			msh_ReleaseProcessLock(g_process_lock);
			break;
		}
		case(msh_CLEAN):
			/* do nothing */
			break;
		default:
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownDirectiveError", "Unrecognized matshare directive. Please use the supplied entry functions.");
			break;
		}
	}
	
	msh_CleanVariableList(&g_local_var_list, directive == msh_CLEAN);
	
}


void msh_Share(int nlhs, mxArray** plhs, size_t num_args, const mxArray** in_args)
{
	int i;
	SegmentNode_t* new_seg_node = NULL;
	VariableNode_t* new_var_node = NULL;
	int will_persist = FALSE, with_names = FALSE;
	const mxArray* curr_in_var, * input_id = NULL;
	mxChar* in_opt;
	
	int num_vars;
	const mxArray** in_vars;
	
	if(num_args > INT_MAX)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumInputsError", "Too many arguments.");
	}
	
	/* check the inputs */
	if(num_args < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NoVariableError", "No variable supplied to share.");
	}
	
	
	in_vars = mxMalloc(num_args * sizeof(const mxArray*));
	for(i = 0, num_vars = 0; i < num_args; i++)
	{
		if(mxIsChar(in_args[i])
		   && mxGetNumberOfElements(in_args[i]) > 1
		   && (in_opt = mxGetChars(in_args[i]))[0] == '-')
		{
			switch(in_opt[1])
			{
				case('p'):
				{
					will_persist = TRUE;
					break;
				}
				case('n'):
				{
					with_names = TRUE;
					break;
				}
				default:
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "ShareOptionError", "Invalid option flag. Note that character vectors longer than 1 starting with '-' are reserved for option flags.");
				}
			}
		}
		else
		{
			in_vars[num_vars] = in_args[i];
			num_vars += 1;
		}
	}
	
	if(with_names && num_vars % 2 != 0)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InputError", "Each input must have be preceded by a name when '-n' is specified.");
	}
	
	for(i = 0; i < num_vars; i = (with_names? i + 2 : i + 1))
	{
		
		if(with_names)
		{
			input_id = in_vars[i];
			curr_in_var = in_vars[i + 1];
		}
		else
		{
			curr_in_var = in_vars[i];
		}
		
		/* scan input data to get required size and create the segment */
		new_seg_node = msh_CreateSegment(msh_FindSharedSize(curr_in_var), input_id, will_persist);
		
		/* copy data to the shared memory */
		msh_CopyVariable(msh_GetSegmentData(new_seg_node), curr_in_var);
		
		/* segment must also be tracked locally, so do that now */
		msh_AddSegmentToList(&g_local_seg_list, new_seg_node);
		
		/* Add the new segment to the shared list */
		msh_AddSegmentToSharedList(new_seg_node);
		
		/* create a shared variable to pass back to the caller */
		new_var_node = msh_CreateVariable(new_seg_node);
		
		/* add that variable to a tracking list */
		msh_AddVariableToList(&g_local_var_list, new_var_node);
		
		/* create and set the return */
		if(i < nlhs)
		{
			plhs[i] = msh_WrapOutput(msh_CreateSharedDataCopy(new_var_node, TRUE));
		}
		else if(i == 0 && nlhs == 0)
		{
			plhs[i] = msh_WrapOutput(msh_CreateSharedDataCopy(new_var_node, FALSE));
		}
		
	}
	
	mxFree((void*)in_vars);
	
}


void msh_Fetch(int nlhs, mxArray** plhs, size_t num_args, const mxArray** in_args)
{
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;
	UpdateFunction_t update_function = NULL;
	mxChar* wide_input_str;
	char_t input_str[MSH_NAME_LEN_MAX];
	size_t j, num_new_vars, num_str_elems;
	int arg_num, out_num, num_out;
	int output_as_struct = FALSE, will_fetch_default = FALSE;
	
	char_t* curr_name;
	char_t* curr_out_name = NULL;
	mxArray* curr_out_var = NULL;
	
	
	if(nlhs < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumOutputsError", "Too few outputs were requested from the fetch operation. Please use the provided entry functions.");
	}
	
	if(num_args > INT_MAX)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumInputsError", "Too many arguments.");
	}
	
	/* preprocessing pass */
	for(arg_num = 0, num_out = (int)num_args; arg_num < num_args; arg_num++)
	{
		
		if(!mxIsChar(in_args[arg_num]))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InputError", "All inputs must be of type 'char'.");
		}
		
		wide_input_str = mxGetChars(in_args[arg_num]);
		if(wide_input_str[0] == '-')
		{
			switch(wide_input_str[1])
			{
				case(MSH_FETCHOPT_STRUCT):
					num_out -= 1;
					output_as_struct = TRUE;
					break;
				case(MSH_FETCHOPT_RECENT):
				{
					/* only update the most recent (fast-track) */
					if(update_function != msh_UpdateAllSegments)
					{
						update_function = msh_UpdateLatestSegment;
					}
					break;
				}
				case(MSH_FETCHOPT_NEW):
				case(MSH_FETCHOPT_ALL):
				case(MSH_FETCHOPT_NAMED):
				{
					update_function = msh_UpdateAllSegments;
					break;
				}
				default:
				{
					/* normally the string produced here should be freed, but since we have an error anyway just let MATLAB do the GC */
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedOptionError", "Unrecognized option %s", mxArrayToString(in_args[arg_num]));
				}
			}
		}
		else
		{
			update_function = msh_UpdateAllSegments;
		}
	}
	
	if(num_out == 0)
	{
		/* output default */
		will_fetch_default = TRUE;
		num_out = 1;
		
		/* set for the default fetch */
		if(g_user_config.fetch_default[0] == '-' && g_user_config.fetch_default[1] == 'r')
		{
			/* only update the most recent (fast-track) */
			update_function = msh_UpdateLatestSegment;
		}
		else
		{
			/* do a full update */
			update_function = msh_UpdateAllSegments;
		}
	}
	
	if((output_as_struct && nlhs > 1) || nlhs > num_out)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "TooManyOutputsError", "Too many outputs requested.");
	}
	
	/* run the update operation */
	update_function(&g_local_seg_list);
	
	/* create backing variables for each segment; this isn't very expensive so do it in any case */
	for(curr_seg_node = g_local_seg_list.first, num_new_vars = 0; curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
	{
		if(msh_GetVariableNode(curr_seg_node) == NULL)
		{
			/* create the variable node if it hasnt been created yet */
			msh_AddVariableToList(&g_local_var_list, msh_CreateVariable(curr_seg_node));
			num_new_vars += 1;
		}
	}
	
	if(output_as_struct)
	{
		plhs[0] = mxCreateStructMatrix(1, 1, 0, NULL);
	}
	
	/* create outputs */
	for(arg_num = 0, out_num = 0; out_num < num_out; arg_num++)
	{
		if(will_fetch_default)
		{
			strcpy(input_str, (char*)g_user_config.fetch_default);
		}
		else
		{
			if(!mxIsChar(in_args[arg_num]))
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidFetchError", "All arguments must be of type 'char'.");
			}
			
			if(mxIsEmpty(in_args[arg_num]))
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidFetchError", "All arguments must have a length more than zero.");
			}
			
			if((num_str_elems = mxGetNumberOfElements(in_args[arg_num])) > MSH_NAME_LEN_MAX-1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidFetchError", "All arguments must have length of less than %d characters.", MSH_NAME_LEN_MAX);
			}
			
			/* validate only using ANSI chars */
			wide_input_str = mxGetChars(in_args[arg_num]);
			if(wide_input_str[0] != '-')
			{
				for(j = 0; j < num_str_elems; j++)
				{
					if(!isalpha(wide_input_str[j]))
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIDError", "Variable IDs must consist only of ANSI alphabetic characters.");
					}
				}
			}
			mxGetString(in_args[arg_num], input_str, sizeof(input_str));
		}
		
		if(input_str[0] == '-')
		{
			switch(input_str[1])
			{
				case(MSH_FETCHOPT_STRUCT):
					/* do nothing */
					continue;
				case(MSH_FETCHOPT_RECENT):
				{
					curr_out_name = "recent";
					curr_out_var = mxCreateCellMatrix((size_t)(g_local_seg_list.last != NULL), (size_t)(g_local_seg_list.last != NULL));
					if(g_local_seg_list.last != NULL)
					{
						mxSetCell(curr_out_var, 0, msh_WrapOutput(msh_CreateSharedDataCopy(msh_GetVariableNode(g_local_seg_list.last), TRUE)));
					}
					out_num += 1;
					break;
				}
				case(MSH_FETCHOPT_NEW):
				{
					/* return the new variables detected */
					curr_out_name = "new";
					curr_out_var = mxCreateCellMatrix(num_new_vars, (size_t)(num_new_vars > 0));
					for(j = 0, curr_var_node = g_local_var_list.last; j < num_new_vars; j++, curr_var_node = msh_GetPreviousVariable(curr_var_node))
					{
						mxSetCell(curr_out_var, num_new_vars - 1 - j, msh_WrapOutput(msh_CreateSharedDataCopy(curr_var_node, TRUE)));
					}
					out_num += 1;
					break;
				}
				case(MSH_FETCHOPT_ALL):
				{
					/* retrieve all segment variables */
					curr_out_name = "all";
					curr_out_var = mxCreateCellMatrix(g_local_seg_list.num_segs, (size_t)(g_local_seg_list.num_segs > 0));
					for(j = 0, curr_seg_node = g_local_seg_list.first; j < g_local_seg_list.num_segs; j++, curr_seg_node = msh_GetNextSegment(curr_seg_node))
					{
						mxSetCell(curr_out_var, j, msh_WrapOutput(msh_CreateSharedDataCopy(msh_GetVariableNode(curr_seg_node), TRUE)));
					}
					out_num += 1;
					break;
				}
				case(MSH_FETCHOPT_NAMED):
				{
					curr_out_name = "named";
					curr_out_var = mxCreateStructMatrix(1, 1, 0, NULL);
					for(j = 0, curr_seg_node = g_local_seg_list.first; j < g_local_seg_list.num_named && curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
					{
						if(msh_HasVariableName(curr_seg_node))
						{
							curr_name = msh_GetSegmentMetadata(curr_seg_node)->name;
							if(!mxGetField(curr_out_var, 0, curr_name))
							{
								mxAddField(curr_out_var, curr_name);
								mxSetField(curr_out_var, 0, curr_name, msh_CreateNamedOutput(curr_name));
							}
							j += 1;
						}
					}
					out_num += 1;
					break;
					
				}
				default:
				{
					/* normally the string produced here should be freed, but since we have an error anyway just let MATLAB do the GC */
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedOptionError", "Unrecognized option \"%s\"", mxArrayToString(in_args[arg_num]));
				}
			}
		}
		else
		{
			/* find variable by identifier */
			
			curr_out_name = input_str;
			curr_out_var = msh_CreateNamedOutput(input_str);
			out_num += 1;
		}
		
		if(output_as_struct && !mxGetField(plhs[0], 0, curr_out_name))
		{
			mxAddField(plhs[0], curr_out_name);
			mxSetField(plhs[0], 0, curr_out_name, curr_out_var);
		}
		else
		{
			plhs[arg_num] = curr_out_var;
		}
		
	}
	
}


static mxArray* msh_CreateNamedOutput(const char_t* name)
{
	size_t i;
	SegmentList_t named_var_list;
	mxArray* named_var_ret;
	
	msh_FindAllSegmentNodes(g_local_seg_list.name_table, &named_var_list, (void*)name);
	named_var_ret = mxCreateCellMatrix(named_var_list.num_segs, (size_t)(named_var_list.num_segs > 0));
	for(i = 0; named_var_list.first != NULL; i++)
	{
		mxSetCell(named_var_ret, i, msh_WrapOutput(msh_CreateSharedDataCopy(msh_GetVariableNode(named_var_list.first), TRUE)));
		msh_DestroySegmentNode(msh_RemoveSegmentFromList(named_var_list.first));
	}
	return named_var_ret;
}


static mxArray* msh_WrapOutput(mxArray* shared_data_copy)
{
	mxArray* output = mxCreateCellMatrix(1, 1);
	mxSetCell(output, 0, shared_data_copy);
	return output;
}


void msh_Copy(int nlhs, mxArray** plhs, int num_inputs, const mxArray** in_vars)
{
	int i;
	size_t j, num_elems;
	
	if(num_inputs < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please supply at least one "
		                                                                     "<a href=\"matlab:helpPopup matshare.object\" style=\"font-weight:bold\">matshare object</a> "
		                                                                     "to copy.");
	}
	
	for(i = 0; i < num_inputs; i++)
	{
		
		if(!(mxIsClass(in_vars[i], "matshare.object") || mxIsClass(in_vars[i], "matshare.compatobject")))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidInputError", "Invalid input. All arguments for the copy function must be <a href=\"matlab:helpPopup matshare.object\" "
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


void msh_Clear(int num_inputs, const mxArray** in_vars)
{
	SegmentNode_t* clear_seg_node;
	VariableNode_t* curr_var_node;
	mxArray* link;
	const mxArray* clear_var, * curr_in_var;
	mxChar* wide_input_str;
	char input_str[MSH_NAME_LEN_MAX];
	bool_t found_variable;
	size_t num_str_elems, j;
	int input_num;
	
	msh_AcquireProcessLock(g_process_lock);
	if(num_inputs > 0)
	{
		
		msh_UpdateAllSegments(&g_local_seg_list);
		
		for(input_num = 0; input_num < num_inputs; input_num++)
		{
			curr_in_var = in_vars[input_num];
			if(mxIsCell(curr_in_var))
			{
				clear_var = mxGetCell(curr_in_var, 0);
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
			else if(mxIsChar(curr_in_var))
			{
				if(mxIsEmpty(curr_in_var))
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidClearError", "All arguments must have a length more than zero.");
				}
				
				if((num_str_elems = mxGetNumberOfElements(curr_in_var)) > MSH_NAME_LEN_MAX-1)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidClearError", "All arguments must have length of less than %d characters.", MSH_NAME_LEN_MAX);
				}
				
				wide_input_str = mxGetChars(curr_in_var);
				for(j = 0; j < num_str_elems; j++)
				{
					if(!isalpha(wide_input_str[j]))
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidClearError", "Variable IDs must consist only of ANSI alphabetic characters.");
					}
				}
				mxGetString(curr_in_var, input_str, sizeof(input_str));
				
				while((clear_seg_node = msh_FindSegmentNode(g_local_seg_list.name_table, input_str)) != NULL)
				{
					msh_RemoveSegmentFromSharedList(clear_seg_node);
					msh_RemoveSegmentFromList(clear_seg_node);
					msh_DetachSegment(clear_seg_node);
				}
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
	int will_sync = FALSE; /* asynchronous by default */
	
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
				case('s'):
					/* sync */
					will_sync = TRUE;
					break;
				case('a'):
					/* async */
					will_sync = FALSE;
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
		if(will_sync)
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
	int j;
	size_t i, ps_len, vs_len;
	char param_str[MSH_NAME_LEN_MAX] = {0}, val_str[MSH_NAME_LEN_MAX] = {0}, param_str_l[MSH_NAME_LEN_MAX] = {0}, val_str_l[MSH_NAME_LEN_MAX] = {0};
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
		          msh_GetCounterFlag(&g_user_config.lock_counter)? "on" : "off",
		          g_user_config.max_shared_segments,
		          g_user_config.max_shared_size,
		          g_user_config.will_shared_gc? "on" : "off",
		          g_user_config.fetch_default);
#ifdef MSH_UNIX
		mexPrintf(MSH_CONFIG_SECURITY_STRING_FORMAT, g_user_config.security);
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
		
		ps_len = mxGetNumberOfElements(param);
		vs_len = mxGetNumberOfElements(val);
		
		if(ps_len > sizeof(param_str) - 1)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidParamError", "Unrecognised parameter \"%s\".", mxArrayToString(param));
		}
		else if(vs_len > sizeof(val_str) - 1)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidValueError", "Value \"%s\" for parameter \"%s\" is too long.", mxArrayToString(val), mxArrayToString(param));
		}
		
		/* used for error messages */
		mxGetString(param, param_str, sizeof(param_str));
		mxGetString(val, val_str, sizeof(val_str));
		
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
				msh_WaitSetCounter(&g_user_config.lock_counter, TRUE);
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				msh_WaitSetCounter(&g_user_config.lock_counter, FALSE);
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
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NegativeNumVarsError", "The maximum number of shared variables must be non-negative.");
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
			g_user_config.max_shared_segments = maxvars_temp;
			
		}
		else if(strcmp(param_str_l, MSH_PARAM_MAX_SIZE_L) == 0 || strcmp(param_str_l, MSH_PARAM_MAX_SIZE_AB) == 0)
		{
			
			/* this can't be negative */
			if(val_str_l[0] == '-')
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NegativeSizeError", "The maximum total size of shared memory must be non-negative.");
			}
			
			errno = 0;
#if MSH_BITNESS == 64
			maxsize_temp = strtoull(val_str_l, NULL, 0);
#elif MSH_BITNESS == 32
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
			g_user_config.max_shared_size = maxsize_temp;
			
		}
		else if(strncmp(param_str_l, MSH_PARAM_GC_L, 7) == 0 || strcmp(param_str_l, MSH_PARAM_GC_AB) == 0)
		{
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				g_user_config.will_shared_gc = TRUE;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_user_config.will_shared_gc = FALSE;
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
				
				msh_UpdateAllSegments(&g_local_seg_list);
				
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
				g_user_config.security = sec_temp;
				msh_ReleaseProcessLock(g_process_lock);
			}
#else
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", MSH_PARAM_SECURITY);
#endif
		}
		else if(strcmp(param_str_l, MSH_PARAM_FETCH_DEFAULT_L) == 0 || strcmp(param_str_l, MSH_PARAM_FETCH_DEFAULT_AB) == 0)
		{
			/* don't use the lowercase string in this case */
			strcpy((char*)g_user_config.fetch_default, val_str);
		}
		else
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
	}
	
}
