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
#include "mshvarops.h"

#ifdef MSH_UNIX
#  include <string.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif

char_T* g_msh_library_name         = "matshare";
char_T* g_msh_error_help_message   = "";
char_T* g_msh_warning_help_message = "";

LocalInfo_T g_local_info =
{
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

SegmentTable_T g_seg_table =
{
	NULL,
	0,
	msh_GetSegmentHashByNumber,
	msh_CompareNumericKey
};

SegmentTable_T g_name_table =
{
	NULL,
	0,
	msh_GetSegmentHashByName,
	msh_CompareStringKey
};

SegmentList_T g_local_seg_list =
{
	&g_seg_table,
	&g_name_table,
	NULL,
	NULL,
	0,
	0
};

VariableList_T g_local_var_list =
{
	NULL,
	NULL
};

/**
 * Wraps the output in a 1x1 cell array.
 *
 * @param shared_data_copy the output which should be a shared data copy.
 * @return the wrapped output.
 */
static mxArray* msh_WrapOutput(mxArray* shared_data_copy);


/**
 * Creates output for most recently shared variable.
 *
 * @return the output for the most recently shared variable.
 */
static mxArray* msh_CreateOutputRecent(void);


/**
 * Creates output for all newly tracked variables.
 *
 * @param num_new_vars the number of newly tracked variables.
 * @return the output for the newly tracked variables.
 */
static mxArray* msh_CreateOutputNew(size_t num_new_vars);


/**
 * Creates output for all named shared variables.
 *
 * @return the output for all named shared variables.
 */
static mxArray* msh_CreateOutputAll(void);


/**
 * Creates output for all named variables.
 *
 * @return the output for all named variables.
 */
static mxArray* msh_CreateOutputNamed(void);


/**
 * Creates output for the shared variable of the given name.
 *
 * @param name the name of the shared variable.
 * @return the shared variable(s) as a cell array.
 */
static mxArray* msh_CreateNamedOutput(const char_T* name);

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
	msh_directive_T directive;
	
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
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Minimum input arguments missing. Please use the supplied entry functions.");
	}
	
	/* get the directive, no check is made since the matshare should not be used without entry functions */
	directive = (msh_directive_T)mxGetScalar(in_directive);
	
	
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
					mexPrintf(MSH_CONFIG_STRING_FORMAT "\n", MSH_CONFIG_STRING_ARGS);
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
			if(num_in_args < 2)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "Not enough inputs. Please use the entry functions provided.");
			}
			msh_Share(nlhs, plhs, mxGetNumberOfElements(in_args[1]), mxGetData(in_args[1]), (int)mxGetScalar(in_args[0]));
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
				msh_SetDefaultConfiguration((void*)&g_user_config);
				msh_WriteConfiguration();
			}
			msh_ReleaseProcessLock(g_process_lock);
			break;
		}
		case(msh_VAROP):
		{
			if(nrhs < 2)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NotEnoughInputsError", "No variable operation supplied. Please use the supplied entry functions.");
			}
			msh_VarOps(nlhs, plhs, num_in_args - 1, in_args + 1, (msh_varop_T)mxGetScalar(prhs[1]));
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
		{
			/* do nothing */
			break;
		}
		default:
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownDirectiveError", "Unrecognized matshare directive. Please use the supplied entry functions.");
			break;
		}
	}
	
	msh_CleanVariableList(&g_local_var_list, directive == msh_CLEAN);
	
}


void msh_Share(int nlhs, mxArray** plhs, size_t num_args, const mxArray** in_args, int return_to_ans)
{
	size_t              i, j, num_vars;
	mxChar*             in_opt;
	const mxArray*      curr_in_var;
	const mxArray*      input_id;
	const mxArray**     in_vars;
	
	int                 will_persist = FALSE;
	int                 with_names   = FALSE;
	SegmentNode_T*      new_seg_node = NULL;
	VariableNode_T*     new_var_node = NULL;
	
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
	
	if(with_names)
	{
		if(num_vars%2 != 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InputError", "Each input must have be preceded by a name when '-n' is specified.");
		}
		num_vars/=2;
	}
	
	if(num_vars < (size_t)nlhs)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "OutputError", "Too many outputs.");
	}
	
	for(i = 0, j = 0, input_id = NULL; i < num_vars; i++)
	{
		
		if(with_names)
		{
			input_id = in_vars[2*i];
			curr_in_var = in_vars[2*i + 1];
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
		if(j < (size_t)nlhs)
		{
			plhs[j] = msh_WrapOutput(msh_CreateSharedDataCopy(new_var_node, j > 0 || !return_to_ans));
			j += 1;
		}
	}
	
	mxFree((void*)in_vars);
	
}


void msh_Fetch(int nlhs, mxArray** plhs, size_t num_args, const mxArray** in_args)
{
	unsigned            arg_num, out_num, num_out;
	size_t              num_new_vars, num_op_args;
	char_T              input_str[MSH_NAME_LEN_MAX];
	SegmentNode_T*      curr_seg_node;
	
	
	int                 output_as_struct   = FALSE;
	int                 will_fetch_default = FALSE;
	UpdateFunction_t    update_function    = NULL;
	const char_T*       all_out_names[]    = {"recent", "new", "all", "named"};
	
	if(nlhs < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumOutputsError", "Too few outputs were requested from the fetch operation. Please use the provided entry functions.");
	}
	
	if(num_args > INT_MAX)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumInputsError", "Too many arguments.");
	}
	
	/* preprocessing pass */
	num_out = (int)num_args;
	for(arg_num = 0, num_op_args = 0; arg_num < num_args; arg_num++)
	{
		/* input validation */
		if(!mxIsChar(in_args[arg_num]))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidFetchError", "All arguments must be of type 'char'.");
		}
		
		if(mxIsEmpty(in_args[arg_num]))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidFetchError", "All arguments must have a length more than zero.");
		}
		
		if(mxGetChars(in_args[arg_num])[0] == '-')
		{
			
			if(mxGetNumberOfElements(in_args[arg_num]) < 2)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidFetchError", "Option flags must have length more than 1.");
			}
			
			switch(mxGetChars(in_args[arg_num])[1])
			{
				case(MSH_FETCHOPT_STRUCT):
					num_out = 1;
					output_as_struct = TRUE;
					break;
				case(MSH_FETCHOPT_RECENT):
				{
					/* only update the most recent (fast-track) */
					if(update_function != msh_UpdateAllSegments)
					{
						update_function = msh_UpdateLatestSegment;
					}
					num_op_args++;
					break;
				}
				case(MSH_FETCHOPT_NEW):
				case(MSH_FETCHOPT_ALL):
				case(MSH_FETCHOPT_NAMED):
				{
					update_function = msh_UpdateAllSegments;
					num_op_args++;
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
			msh_CheckVarname(in_args[arg_num]);
			update_function = msh_UpdateAllSegments;
			num_op_args++;
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
	else if(output_as_struct && num_op_args == 0)
	{
		/* there weren't any other operators, update everything */
		update_function = msh_UpdateAllSegments;
	}
	
	if(num_out > (unsigned)nlhs)
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
		
		/* create outputs */
		if(num_op_args > 0)
		{
			plhs[0] = mxCreateStructMatrix(1, 1, 0, NULL);
			for(arg_num = 0; arg_num < num_args; arg_num++)
			{
				mxGetString(in_args[arg_num], input_str, sizeof(input_str));
				if(input_str[0] == '-')
				{
					switch(input_str[1])
					{
						case(MSH_FETCHOPT_STRUCT):
						{
							/* do nothing */
							break;
						}
						case(MSH_FETCHOPT_RECENT):
						{
							/* return only the most recent */
							if(!mxGetField(plhs[0], 0, "recent"))
							{
								mxAddField(plhs[0], "recent");
								mxSetField(plhs[0], 0, "recent", msh_CreateOutputRecent());
							}
							break;
						}
						case(MSH_FETCHOPT_NEW):
						{
							/* return the new variables detected */
							if(!mxGetField(plhs[0], 0, "new"))
							{
								mxAddField(plhs[0], "new");
								mxSetField(plhs[0], 0, "new", msh_CreateOutputNew(num_new_vars));
							}
							break;
						}
						case(MSH_FETCHOPT_ALL):
						{
							/* retrieve all segment variables */
							if(!mxGetField(plhs[0], 0, "all"))
							{
								mxAddField(plhs[0], "all");
								mxSetField(plhs[0], 0, "all", msh_CreateOutputAll());
							}
							break;
						}
						case(MSH_FETCHOPT_NAMED):
						{
							/* return all named variables */
							if(!mxGetField(plhs[0], 0, "named"))
							{
								mxAddField(plhs[0], "named");
								mxSetField(plhs[0], 0, "named", msh_CreateOutputNamed());
							}
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
					if(!mxGetField(plhs[0], 0, input_str))
					{
						mxAddField(plhs[0], input_str);
						mxSetField(plhs[0], 0, input_str, msh_CreateNamedOutput(input_str));
					}
				}
			}
		}
		else
		{
			/* there weren't any other operation arguments, so output everything */
			plhs[0] = mxCreateStructMatrix(1, 1, 4, all_out_names);
			mxSetField(plhs[0], 0, "recent", msh_CreateOutputRecent());
			mxSetField(plhs[0], 0, "new", msh_CreateOutputNew(num_new_vars));
			mxSetField(plhs[0], 0, "all", msh_CreateOutputAll());
			mxSetField(plhs[0], 0, "named", msh_CreateOutputNamed());
		}
		
	}
	else
	{
		/* create outputs */
		for(arg_num = 0, out_num = 0; out_num < num_out; arg_num++)
		{
			if(will_fetch_default)
			{
				strcpy(input_str, (char_T*)g_user_config.fetch_default);
			}
			else
			{
				mxGetString(in_args[arg_num], input_str, sizeof(input_str));
			}
			
			if(input_str[0] == '-')
			{
				switch(input_str[1])
				{
					case (MSH_FETCHOPT_RECENT):
					{
						plhs[out_num] = msh_CreateOutputRecent();
						out_num += 1;
						break;
					}
					case (MSH_FETCHOPT_NEW):
					{
						/* return the new variables detected */
						plhs[out_num] = msh_CreateOutputNew(num_new_vars);
						out_num += 1;
						break;
					}
					case (MSH_FETCHOPT_ALL):
					{
						/* retrieve all segment variables */
						plhs[out_num] = msh_CreateOutputAll();
						out_num += 1;
						break;
					}
					case (MSH_FETCHOPT_NAMED):
					{
						plhs[out_num] = msh_CreateOutputNamed();
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
				plhs[arg_num] = msh_CreateNamedOutput(input_str);
				out_num += 1;
			}
			
		}
	}
	
}


static mxArray* msh_CreateOutputRecent(void)
{
	mxArray* out = mxCreateCellMatrix((size_t)(g_local_seg_list.last != NULL), (size_t)(g_local_seg_list.last != NULL));
	if(g_local_seg_list.last != NULL)
	{
		mxSetCell(out, 0, msh_WrapOutput(msh_CreateSharedDataCopy(msh_GetVariableNode(g_local_seg_list.last), TRUE)));
	}
	return out;
}


static mxArray* msh_CreateOutputNew(size_t num_new_vars)
{
	size_t i;
	VariableNode_T* curr_var_node;
	mxArray* out = mxCreateCellMatrix(num_new_vars, (size_t)(num_new_vars > 0));
	for(i = 0, curr_var_node = g_local_var_list.last; i < num_new_vars; i++, curr_var_node = msh_GetPreviousVariable(curr_var_node))
	{
		mxSetCell(out, num_new_vars - 1 - i, msh_WrapOutput(msh_CreateSharedDataCopy(curr_var_node, TRUE)));
	}
	return out;
}


static mxArray* msh_CreateOutputAll(void)
{
	size_t i;
	SegmentNode_T* curr_seg_node;
	mxArray* out = mxCreateCellMatrix(g_local_seg_list.num_segs, (size_t)(g_local_seg_list.num_segs > 0));
	for(i = 0, curr_seg_node = g_local_seg_list.first; i < g_local_seg_list.num_segs; i++, curr_seg_node = msh_GetNextSegment(curr_seg_node))
	{
		mxSetCell(out, i, msh_WrapOutput(msh_CreateSharedDataCopy(msh_GetVariableNode(curr_seg_node), TRUE)));
	}
	return out;
}


static mxArray* msh_CreateOutputNamed(void)
{
	size_t i;
	SegmentNode_T* curr_seg_node;
	const char* curr_name;
	mxArray* out = mxCreateStructMatrix(1, 1, 0, NULL);
	for(i = 0, curr_seg_node = g_local_seg_list.first; i < g_local_seg_list.num_named && curr_seg_node != NULL; curr_seg_node = msh_GetNextSegment(curr_seg_node))
	{
		if(msh_HasVariableName(curr_seg_node))
		{
			curr_name = msh_GetSegmentMetadata(curr_seg_node)->name;
			if(!mxGetField(out, 0, curr_name))
			{
				mxAddField(out, curr_name);
				mxSetField(out, 0, curr_name, msh_CreateNamedOutput(curr_name));
			}
			i += 1;
		}
	}
	return out;
}


static mxArray* msh_CreateNamedOutput(const char_T* name)
{
	size_t i;
	SegmentList_T named_var_list;
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
	SegmentNode_T* clear_seg_node;
	VariableNode_T* curr_var_node;
	mxArray* link;
	const mxArray* clear_var, * curr_in_var;
	char input_str[MSH_NAME_LEN_MAX];
	bool_T found_variable;
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
				msh_CheckVarname(curr_in_var);
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


void msh_Config(size_t num_params, const mxArray** in_params)
{
	size_t              i, j, ps_len, vs_len, maxsize_temp;
	unsigned long       maxvars_temp;
	const mxArray*      param;
	const mxArray*      val;
	
#ifdef MSH_UNIX
	mode_t              sec_temp;
	SegmentNode_T*      curr_seg_node;
#endif
	
	char                param_str[MSH_NAME_LEN_MAX]   = {0};
	char                val_str[MSH_NAME_LEN_MAX]     = {0};
	char                param_str_l[MSH_NAME_LEN_MAX] = {0};
	char                val_str_l[MSH_NAME_LEN_MAX]   = {0};
	
	if(num_params == 0)
	{
		mexPrintf(MSH_CONFIG_STRING_FORMAT, MSH_CONFIG_STRING_ARGS);
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
		
		if(strncmp(param_str_l, MSH_PARAM_MAX_VARIABLES_L, 6) == 0 || strcmp(param_str_l, MSH_PARAM_MAX_VARIABLES_AB) == 0)
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
		else if(strcmp(param_str_l, MSH_PARAM_SYNC_DEFAULT_L) == 0 || strcmp(param_str_l, MSH_PARAM_SYNC_DEFAULT_AB) == 0)
		{
			/* don't use the lowercase string in this case */
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				g_user_config.sync_default = TRUE;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_user_config.sync_default = FALSE;
			}
			else
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_SYNC_DEFAULT);
			}
		}
		else
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
	}
	
}

void msh_VarOps(int nlhs, mxArray** plhs, int num_args, const mxArray** in_args, msh_varop_T varop)
{
	
	/* input order (including arguments handled by mexFunction)
	 *
	 * 0. directive
	 * 1. varop
	 * 2. dest_var
	 * 3. {in_var1, in_var2, ...}
	 * 4. {substruct, opt1, opt2, ...}
	 */
	
	size_t            i, num_varargin;
	mxChar*           input_option;
	mxArray*          opt_input;
	const mxArray*    parent_var;
	const mxArray*    in_vars;
	
	int               exp_num_in_args = msh_GetNumVarOpArgs(varop)-1;
	int               index_once      = FALSE;
	int               opts            = 0;
	mxArray*          subs_struct     = NULL;
	
	if(num_args != 3)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNumberOfArgumentsError", "Too many or too few arguments. Please use the provided entry functions.");
	}
	
	parent_var = mxGetCell(in_args[0], 0);
	in_vars = in_args[1];
	
	if(mxIsCell(in_vars) && mxGetNumberOfElements(in_vars) != exp_num_in_args)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNumberOfInputsError", "Too many or too few inputs.");
	}
	
	/*
	 * check dest compared to subscripting while
	 * check input compared to subscripting and dest variable
	 */
	
	if(num_args == 3)
	{
		for(i = 0, num_varargin = mxGetNumberOfElements(in_args[2]); i < num_varargin; i++)
		{
			opt_input = mxGetCell(in_args[2], i);
			if(mxIsStruct(opt_input))
			{
				if(index_once)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "MultipleSubsStructError", "Must be only one subscripting struct input.");
				}
				index_once = TRUE;
				
				subs_struct = opt_input;
				
			}
			else if(mxIsChar(opt_input))
			{
				if(mxGetNumberOfElements(opt_input) < 2)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidOptionError", "Option must have length more than 1.");
				}
				input_option = mxGetChars(opt_input);
				if(input_option[0] == '-')
				{
					/* switch for possible future additions */
					switch(input_option[1])
					{
						case('s'):
						{
							/* sync */
							opts |= MSH_IS_SYNCHRONOUS;
							break;
						}
						case('a'):
						{
							/* async */
							opts &= ~MSH_IS_SYNCHRONOUS;
							break;
						}
						case('t'):
						{
							/* atomic */
							opts |= MSH_USE_ATOMIC_OPS;
							break;
						}
						case('n'):
						{
							/* non-atomic */
							opts &= ~MSH_USE_ATOMIC_OPS;
							break;
						}
						default:
						{
							/* normally the string produced here should be freed with mxFree, but since we have an error just let MATLAB do the GC */
							meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedOptionError", "Unrecognized option \"%s\"", mxArrayToString(in_args[2]));
						}
					}
				}
				else
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedOptionError", "Input options must start with '-'. Invalid option \"%s\"", mxArrayToString(in_args[2]));
				}
			}
			else
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidOptionError", "Option must be of class 'struct' or 'char'.");
			}
		}
	}
	
	
	if(nlhs > 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "TooManyOutputsError", "Too many outputs");
	}
	
	
	/* returns output only if requested to save time and memory */
	msh_VariableOperation(parent_var, in_vars, subs_struct, varop, opts, (nlhs==1)? plhs : NULL);
	
}
