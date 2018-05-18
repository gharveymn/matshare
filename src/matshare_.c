#include "headers/matshare_.h"
#include <ctype.h>
#ifdef MSH_UNIX
#include <sys/stat.h>
#endif

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
		ReadMexError(__FILE__, __LINE__, "NotEnoughInputsError", "Minimum input arguments missing; must supply a msh_directive.");
	}
	
	/* get the msh_directive */
	msh_directive = msh_ParseDirective(in_directive);
	
	/* Don't initialize if we are detaching and matshare has not been initialized for this process yet */
	if(msh_directive == msh_DETACH && g_local_info == NULL)
	{
		return;
	}
	
	msh_InitializeMatshare();
	
	switch(msh_directive)
	{
		case msh_SHARE:
			
			msh_VariableGC();
			msh_Share(nlhs, plhs, num_in_vars, in_vars);
			
			break;
		
		case msh_FETCH:
			
			msh_VariableGC();
			msh_Fetch(nlhs, plhs, MSH_SHARED_COPY);
			
			break;
		
		case msh_OBJ_DEREGISTER:
			/* deregister an object tracking this function */
			g_local_info->num_registered_objs -= 1;
			if(g_local_info->num_registered_objs == 0)
			{
				/* if we deregistered the last object then remove variables that aren't being used elsewhere in this process */
				msh_VariableGC();
			}
			
			break;
		case msh_DETACH:
			
			msh_OnExit();
			
			break;
		
		case msh_PARAM:
			
			msh_Param(num_in_vars, in_vars);
			
			break;
		
		case msh_DEEPCOPY:
			
			msh_VariableGC();
			msh_Fetch(nlhs, plhs, MSH_DUPLICATE);
			
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
			
			msh_Clear(num_in_vars, in_vars);
			
			break;
		default:
			ReadMexError(__FILE__, __LINE__, "UnknownDirectiveError", "Unrecognized matshare directive. Please use the supplied entry functions.");
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
		ReadMexError(__FILE__, __LINE__, "NoVariableError", "No variable supplied to clone.");
	}
	
	for(i = 0, in_var = in_vars[i]; i < num_vars; i++, in_var = in_vars[i])
	{
		
		if(!(mxIsNumeric(in_var) || mxIsLogical(in_var) || mxIsChar(in_var) || mxIsStruct(in_var) || mxIsCell(in_var)))
		{
			ReadMexError(__FILE__, __LINE__, "InvalidTypeError", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
		}
		
		
		if(g_shared_info->user_def.sharetype == msh_SHARETYPE_OVERWRITE)
		{
			if(g_local_seg_list.num_segs != 0 && (msh_CompareVariableSize(msh_GetSegmentData(g_local_seg_list.last), in_var) == TRUE))
			{
				/* DON'T INCREMENT THE REVISION NUMBER */
				/* this is an in-place change, so everyone is still fine */
				
				if(g_local_seg_list.last->var_node == NULL)
				{
					msh_CreateVariable(&g_local_var_list, g_local_seg_list.last);
				}
				
				/* do the rewrite after checking because the comparison is cheap */
				msh_OverwriteData(msh_GetSegmentData(g_local_seg_list.last), in_var, g_local_seg_list.last->var_node->var);
				
				/* DON'T DO ANYTHING ELSE */
				continue;
			}
		}
		
		/* scan input data to get required size and create the segment */
		new_seg_node = msh_CreateSegment(msh_FindSegmentSize(in_var));
		
		/* copy data to the shared memory */
		msh_CopyVariable(msh_GetSegmentData(new_seg_node), in_var);
		
		/* Add the new segment to the shared list */
		msh_AddSegmentToSharedList(new_seg_node);
		
		/* segment must also be tracked locally, so do that now */
		msh_AddSegmentToLocalList(&g_local_seg_list, new_seg_node);
		
	}
	
	msh_UpdateAll();
	
	if(nlhs > 0)
	{
		msh_Fetch(nlhs, plhs, MSH_SHARED_COPY);
	}
	
}


void msh_Fetch(int nlhs, mxArray** plhs, bool_t will_duplicate)
{
	
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;

#ifdef MSH_32BIT
	int i, num_new_vars = 0;
	int ret_dims[2];
#else
	size_t i, num_new_vars = 0;
	size_t ret_dims[2];
#endif
	
	/* TODO: only run the full update if nlhs >= 2, but just find the latest segment otherwise */
	msh_UpdateSegmentTracking(&g_local_seg_list);
	
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
					msh_CreateVariable(&g_local_var_list, curr_seg_node);
					num_new_vars += 1;
				}
			}
			
			/* return the new variables detected */
#ifdef MSH_32BIT
			ret_dims[0] = (int)(num_new_vars != 0);
#else
			ret_dims[0] = (size_t)(num_new_vars != 0);
#endif
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
			
			if(nlhs >= 3)
			{
				
				if(nlhs > 3)
				{
					ReadMexError(__FILE__, __LINE__, "TooManyOutputsError", "Too many outputs; got %d, need between 0 and 4.", nlhs);
				}
				
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
		}
		else
		{
			if(g_local_seg_list.last != NULL && g_local_seg_list.last->var_node == NULL)
			{
				msh_CreateVariable(&g_local_var_list, g_local_seg_list.last);
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
	msh_AcquireProcessLock();
	
	msh_UpdateSegmentTracking(&g_local_seg_list);
	
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
						msh_RemoveSegmentFromLocalList(curr_var_node->seg_node->parent_seg_list, curr_var_node->seg_node);
						msh_RemoveSegmentFromSharedList(curr_var_node->seg_node);
						msh_DetachSegment(curr_var_node->seg_node);
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
		msh_ClearSegmentList(&g_local_seg_list);
	}
	msh_ReleaseProcessLock();
}


void msh_Param(int num_params, const mxArray** in)
{
	int i, j, ps_len, vs_len;
	char param_str[MSH_MAX_NAME_LEN] = {0}, val_str[MSH_MAX_NAME_LEN] = {0}, param_str_l[MSH_MAX_NAME_LEN] = {0}, val_str_l[MSH_MAX_NAME_LEN] = {0};
	const mxArray* param, * val;
	
	if(num_params == 0)
	{
#ifdef MSH_WIN
		mexPrintf(MSH_PARAM_INFO,
				g_shared_info->user_def.is_thread_safe? "true" : "false",
				g_shared_info->user_def.sharetype == msh_SHARETYPE_COPY? "true" : "false",
				g_shared_info->user_def.will_gc? "true" : "false");
#else
		mexPrintf(MSH_PARAM_INFO,
				g_shared_info->user_def.is_thread_safe? "true" : "false",
				g_shared_info->user_def.sharetype == msh_SHARETYPE_COPY? "true" : "false",
				g_shared_info->user_def.will_gc? "true" : "false",
				g_shared_info->security);
#endif
	}
	
	if(num_params % 2 != 0)
	{
		ReadMexError(__FILE__, __LINE__, "InvalidNumArgsError", "The number of parameters input must be a multiple of two.");
	}
	
	for(i = 0; i < num_params/2; i++)
	{
		param = in[2*i];
		val = in[2*i + 1];
		
		if(!mxIsChar(param) || !mxIsChar(val))
		{
			ReadMexError(__FILE__, __LINE__, "InvalidArgumentError", "All parameters and values must be input as character arrays.");
		}
		
		ps_len = (int)mxGetNumberOfElements(param);
		vs_len = (int)mxGetNumberOfElements(val);
		
		if(ps_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadMexError(__FILE__, __LINE__, "InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		else if(vs_len + 1 > MSH_MAX_NAME_LEN)
		{
			ReadMexError(__FILE__, __LINE__, "InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, param_str);
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
			
			/* note: acquiring the process lock makes going from thread-safe to thread-unsafe a safe operation, but not in the other direction */
			msh_AcquireProcessLock();
			
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
				ReadMexError(__FILE__, __LINE__, "InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_THREADSAFETY);
			}
			
			msh_ReleaseProcessLock();
#else
			__ReadMexError__(__FILE__, __LINE__, "InvalidParamError", "Cannot change the state of thread safety for matshare compiled with thread safety turned off.");
#endif
		
		}
		else if(strcmp(param_str_l, MSH_PARAM_SECURITY_L) == 0)
		{
#ifdef MSH_UNIX
			if(vs_len < 3 || vs_len > 4)
			{
				ReadMexError(__FILE__, __LINE__, "InvalidParamValueError", "Too many or too few digits in \"%s\" for parameter \"%s\". Must have either 3 or 4 digits.", val_str, MSH_PARAM_SECURITY);
			}
			else
			{
				msh_AcquireProcessLock();
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
				msh_UpdateAll();
				msh_ReleaseProcessLock();
			}
#else
			ReadMexError(__FILE__, __LINE__, "InvalidParamError", "Parameter \"%s\" has not been implemented for Windows.", param_str);
#endif
		}
		else if(strcmp(param_str_l, MSH_PARAM_SHARETYPE_L) == 0)
		{
			if(strncmp(val_str_l, "copy", 4) == 0)
			{
				g_shared_info->user_def.sharetype = msh_SHARETYPE_COPY;
			}
			else if(strcmp(val_str_l, "overwrite") == 0)
			{
				g_shared_info->user_def.sharetype = msh_SHARETYPE_OVERWRITE;
			}
			else
			{
				ReadMexError(__FILE__, __LINE__, "InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_SHARETYPE);
			}
		}
		else if(strcmp(param_str_l, MSH_PARAM_GC_L) == 0)
		{
			if(strcmp(val_str_l, "true") == 0 || strcmp(val_str_l, "on") == 0 || strcmp(val_str_l, "enable") == 0)
			{
				g_shared_info->user_def.will_gc = TRUE;
			}
			else if(strcmp(val_str_l, "false") == 0 || strcmp(val_str_l, "off") == 0 || strcmp(val_str_l, "disable") == 0)
			{
				g_shared_info->user_def.will_gc = FALSE;
			}
			else
			{
				ReadMexError(__FILE__, __LINE__, "InvalidParamValueError", "Unrecognised value \"%s\" for parameter \"%s\".", val_str, MSH_PARAM_GC);
			}
		}
		else
		{
			ReadMexError(__FILE__, __LINE__, "InvalidParamError", "Unrecognised parameter \"%s\".", param_str);
		}
		
	}
}
