#include "headers/matshare_.h"

GlobalInfo_t* g_local_info = NULL;

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	/* number of inputs other than the msh_directive */
	int num_in_vars = nrhs - 1;
	
	/* inputs */
	const mxArray* in_directive = prhs[0];
	const mxArray** in_vars = prhs + 1;
	
	/* resultant matshare directive */
	msh_directive_t msh_directive;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		ReadMexError("NotEnoughInputsError", "Minimum input arguments missing; must supply a msh_directive.");
	}
	
	/* get the msh_directive */
	msh_directive = ParseDirective(in_directive);
	
	/* Don't initialize if we are detaching and matshare has not been initialized for this process yet */
	if(msh_directive == msh_DETACH && g_local_info == NULL)
	{
		return;
	}
	
	InitializeMatshare();
	
	switch(msh_directive)
	{
		case msh_SHARE:
			
			AcquireProcessLock();
			{
				msh_Share(nlhs, plhs, num_in_vars, in_vars);
			}
			ReleaseProcessLock();
			
			break;
		
		case msh_FETCH:
			
			AcquireProcessLock();
			{
				msh_Fetch(nlhs, plhs, MSH_SHARED_COPY);
			}
			ReleaseProcessLock();
			
			break;
		
		case msh_OBJ_DEREGISTER:
			/* deregister an object tracking this function */
			g_local_info->num_registered_objs -= 1;
			if(g_local_info->num_registered_objs == 0)
			{
				/* if we deregistered the last object then remove variables that aren't being used elsewhere in this process */
				AcquireProcessLock();
				{
					UpdateSharedSegments();
					RemoveUnusedVariables();
				}
				ReleaseProcessLock();
			}
			
			break;
		case msh_DETACH:
			
			MshOnExit();
			
			break;
		
		case msh_PARAM:
			
			msh_Param(num_in_vars, in_vars);
			
			break;
		
		case msh_DEEPCOPY:
			
			AcquireProcessLock();
			{
				msh_Fetch(nlhs, plhs, MSH_DUPLICATE);
			}
			ReleaseProcessLock();
			
			break;
		case msh_DEBUG:
			/* STUB: maybe print debug information at some point */
			break;
		case msh_OBJ_REGISTER:
			
			/* tell matshare to register a new object */
			g_local_info->num_registered_objs += 1;
			
			break;
		case msh_INIT:
			/* deprecated; does nothing */
			break;
		case msh_CLEAR:
			
			AcquireProcessLock();
			{
				msh_Clear(num_in_vars, in_vars);
			}
			ReleaseProcessLock();
			
			break;
		default:
			ReadMexError("UnknownDirectiveError", "Unrecognized msh_directive.");
			break;
	}
	
}


void msh_Share(int nlhs, mxArray** plhs, int num_vars, const mxArray** in_vars)
{
	SegmentNode_t* new_seg_node;
	const mxArray* in_var;
	int i;
	
	/* check the inputs */
	if(num_vars < 1)
	{
		ReadMexError("NoVariableError", "No variable supplied to clone.");
	}
	
	UpdateSharedSegments();
	
	for(i = 0, in_var = in_vars[i]; i < num_vars; i++, in_var = in_vars[i])
	{
		
		if(!(mxIsNumeric(in_var) || mxIsLogical(in_var) || mxIsChar(in_var) || mxIsStruct(in_var) || mxIsCell(in_var)))
		{
			ReadMexError("InvalidTypeError", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
		}
		
		if(g_shared_info->user_def.sharetype == msh_SHARETYPE_COPY)
		{
			RemoveUnusedVariables();
		}
		else if(g_shared_info->user_def.sharetype == msh_SHARETYPE_OVERWRITE)
		{
			if(g_local_var_list.num_vars != 0 && (msh_CompareVariableSize(MshGetSegmentData(g_local_seg_list.last), in_var) == TRUE))
			{
				/* DON'T INCREMENT THE REVISION NUMBER */
				/* this is an in-place change, so everyone is still fine */
				
				/* do the rewrite after checking because the comparison is cheap */
				msh_OverwriteData(MshGetSegmentData(g_local_seg_list.last), in_var, g_local_var_list.last->var);
				
				/* DON'T DO ANYTHING ELSE */
				continue;
			}
			else
			{
				ClearSegmentList(&g_local_seg_list);
			}
		}
		else
		{
			ReadMexError("UnexpectedError", "Invalid sharetype. The shared memory has been corrupted.");
		}
		
		/* scan input data to get required size and create the segment */
		new_seg_node = CreateSegment(&g_local_seg_list, msh_FindSegmentSize(in_var));
		
		/* copy data to the shared memory */
		msh_CopyVariable(MshGetSegmentData(new_seg_node), in_var);
		
	}
	
	if(nlhs > 0)
	{
		msh_Fetch(nlhs, plhs, MSH_SHARED_COPY);
	}
	
	UpdateAll();
	
}


void msh_Fetch(int nlhs, mxArray** plhs, bool_t will_duplicate)
{
	
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;
	size_t i, num_new_vars = 0;
	size_t ret_dims[2];
	
	UpdateSharedSegments();
	
	if(g_shared_info->user_def.sharetype == msh_SHARETYPE_COPY)
	{
		RemoveUnusedVariables();
	}
	
	if(nlhs >= 1)
	{
		if(nlhs >= 2)
		{
			
			for(i = 0, curr_seg_node = g_local_seg_list.first;
					i < g_local_seg_list.num_segs;
					i++, curr_seg_node = curr_seg_node->next)
			{
				if(curr_seg_node->var_node == NULL)
				{
					/* create the variable node if it hasnt been created yet */
					CreateVariable(&g_local_var_list, curr_seg_node);
					num_new_vars += 1;
				}
			}
			
			/* return the new variables detected */
			ret_dims[0] = (size_t)(num_new_vars != 0);
			ret_dims[1] = num_new_vars;
			plhs[1] = mxCreateCellArray(2, ret_dims);
			
			for(i = 0, curr_var_node = g_local_var_list.last;
					i < num_new_vars;
					i++, curr_var_node = curr_var_node->prev)
			{
				if(will_duplicate)
				{
					mxSetCell(plhs[1], num_new_vars - 1 - i, mxDuplicateArray(curr_var_node->var));
				}
				else
				{
					mxSetCell(plhs[1], num_new_vars - 1 - i, mxCreateSharedDataCopy(curr_var_node->var));
				}
			}
			
			if(nlhs == 3)
			{
				/* retrieve all segment variables */
				ret_dims[0] = (size_t)(g_local_seg_list.num_segs != 0);
				ret_dims[1] = g_local_seg_list.num_segs;
				plhs[2] = mxCreateCellArray(2, ret_dims);
				
				for(i = 0, curr_seg_node = g_local_seg_list.first;
						i < g_local_seg_list.num_segs;
						i++, curr_seg_node = curr_seg_node->next)
				{
					if(will_duplicate)
					{
						mxSetCell(plhs[2], i, mxDuplicateArray(curr_seg_node->var_node->var));
					}
					else
					{
						mxSetCell(plhs[2], i, mxCreateSharedDataCopy(curr_seg_node->var_node->var));
					}
				}
			}
			else
			{
				ReadMexError("TooManyOutputsError", "Too many outputs.");
			}
		}
		else
		{
			if(g_local_seg_list.last != NULL && g_local_seg_list.last->var_node == NULL)
			{
				CreateVariable(&g_local_var_list, g_local_seg_list.last);
			}
		}
		
		if(g_local_seg_list.last != NULL)
		{
			if(will_duplicate)
			{
				plhs[0] = mxDuplicateArray(g_local_seg_list.last->var_node->var);
			}
			else
			{
				plhs[0] = mxCreateSharedDataCopy(g_local_seg_list.last->var_node->var);
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
	
	VariableNode_t* curr_var_node, * next_var_node;
	mxArray* link;
	int i;
	
	UpdateSharedSegments();
	
	if(num_inputs > 0)
	{
		for(i = 0; i < num_inputs; i++)
		{
			curr_var_node = g_local_var_list.first;
			while(curr_var_node != NULL)
			{
				next_var_node = curr_var_node->next;
				
				/* begin hack */
				link = curr_var_node->var;
				do
				{
					if(link == in_vars[i])
					{
						DestroySegment(curr_var_node->seg_node);
						break;
					}
					link = msh_GetCrosslink(link);
				} while(link != NULL && link != curr_var_node->var);
				/* end hack */
				
				curr_var_node = next_var_node;
			}
		}
		
	}
	else
	{
		ClearSegmentList(&g_local_seg_list);
	}
}


void msh_Param(int num_params, const mxArray** in)
{
	int i, j, ps_len, vs_len;
	char param_str[MSH_MAX_NAME_LEN] = {0}, val_str[MSH_MAX_NAME_LEN] = {0}, param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
	
	if(num_params == 0)
	{
#ifdef MSH_WIN
		mexPrintf(MSH_PARAM_INFO, g_shared_info->user_def.is_thread_safe? "true" : "false", g_shared_info->user_def.sharetype == msh_SHARETYPE_COPY? "true" : "false");
#else
		mexPrintf(MSH_PARAM_INFO, g_shared_info->user_def.is_thread_safe? "true" : "false", g_shared_info->user_def.sharetype == msh_SHARETYPE_COPY? "true" : "false", g_shared_info->security);
#endif
	}
	
	if(num_params % 2 != 0)
	{
		ReadMexError("InvalNumArgsError", "The number of parameters input must be a multiple of two.");
	}
	
	for(i = 0; i < num_params/2; i++)
	{
		param = in[2*i];
		val = in[2*i + 1];
		
		if(!mxIsChar(param) || !mxIsChar(val))
		{
			ReadMexError("InvalidArgumentError", "All parameters and values must be input as character arrays.");
		}
		
		ps_len = (int)mxGetNumberOfElements(param);
		vs_len = (int)mxGetNumberOfElements(val);
		
		if(ps_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadMexError("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		else if(vs_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, param_str);
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
		
		if(strcmp(param_str_l, MSH_PARAM_THREADSAFETY_L) == 0)
		{
#ifdef MSH_THREAD_SAFE
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				g_shared_info->user_def.is_thread_safe = TRUE;
			}
			if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_shared_info->user_def.is_thread_safe = FALSE;
			}
			else
			{
				ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_THREADSAFETY);
			}
			UpdateAll();
			ReleaseProcessLock();
#else
			ReadMexError("InvalidParamError", "Cannot change the state of thread safety for matshare compiled with thread safety turned off.");
#endif
		
		}
		else if(strcmp(param_str_l, MSH_PARAM_SECURITY_L) == 0)
		{
#ifdef MSH_UNIX
			if(vs_len < 3 || vs_len > 4)
			{
				ReadMexError("InvalidParamValueError", "Too many or too few digits in \"%s\" for parameter \"%s\". Must have either 3 or 4 digits.", val_str, MSH_PARAM_SECURITY);
			}
			else
			{
				AcquireProcessLock();
				g_shared_info->security = (mode_t)strtol(val_str_l, NULL, 8);
				if(fchmod(g_local_info->shm_info_seg.handle, g_shared_info->security) != 0)
				{
					ReadFchmodError(errno);
				}
				SegmentNode_t* curr_seg_node = g_local_seg_list.first;
				while(curr_seg_node != NULL)
				{
					if(fchmod(curr_seg_node->seg_info.handle, g_shared_info->security) != 0)
					{
						ReadFchmodError(errno);
					}
					curr_seg_node = curr_seg_node->next;
				}
#ifdef MSH_THREAD_SAFE
				if(fchmod(g_local_info->proc_lock, g_shared_info->security) != 0)
				{
					ReadFchmodError(errno);
				}
#endif
				UpdateAll();
				ReleaseProcessLock();
			}
#else
			ReadMexError("InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", param_str);
#endif
		}
		else if(strcmp(param_str_l, MSH_PARAM_COPYONWRITE_L) == 0)
		{
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				g_shared_info->user_def.sharetype = msh_SHARETYPE_COPY;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_shared_info->user_def.sharetype = msh_SHARETYPE_OVERWRITE;
			}
			else
			{
				ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_COPYONWRITE);
			}
			ReleaseProcessLock();
		}
		else if(strcmp(param_str_l, MSH_PARAM_GC_L) == 0)
		{
			AcquireProcessLock();
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				g_shared_info->user_def.will_remove_unused = TRUE;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_shared_info->user_def.will_remove_unused = FALSE;
			}
			else
			{
				ReadMexError("InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_GC);
			}
			ReleaseProcessLock();
		}
		else
		{
			ReadMexError("InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		
	}
}



