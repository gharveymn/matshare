/** mshvariables.c
 * Defines functions for variable creation and tracking.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "mex.h"

#include "mshvariables.h"
#include "mshsegments.h"
#include "mshheader.h"
#include "mlerrorutils.h"
#include "mshutils.h"
#include "mshexterntypes.h"
#include "mshlockfree.h"

/* undocumented function */
extern mxArray* mxCreateSharedDataCopy(mxArray *);

/** public function definitions **/

VariableNode_T* msh_CreateVariable(SegmentNode_T* seg_node)
{
	
	mxArray* new_var;
	
	if(msh_GetVariableNode(seg_node) != NULL)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "CreateVariableError", "The segment targeted for variable creation already has a variable attached to it.");
	}
	
	new_var = msh_FetchVariable(msh_GetSegmentData(seg_node));
	
	/* Important! Make sure MATLAB doesn't try to free this since the data points to a non-allocated address. */
	mexMakeArrayPersistent(new_var);
	
	return msh_CreateVariableNode(seg_node, new_var);
	
}


int msh_DestroyVariable(VariableNode_T* var_node)
{
	SegmentNode_T* seg_node = msh_GetSegmentNode(var_node);
	int did_remove_segment = FALSE;
	
	/* detach all of the Matlab pointers */
	msh_DetachVariable(msh_GetVariableData(var_node));
	mxDestroyArray(msh_GetVariableData(var_node));
	
	/* remove tracking for this variable */
	msh_SetVariableNode(seg_node, NULL);

	if(msh_GetIsUsed(var_node))
	{
		msh_SetIsUsed(var_node, FALSE);
		if((msh_AtomicDecrement(&msh_GetSegmentMetadata(seg_node)->procs_using) == 0) &&
		   g_user_config.will_shared_gc &&
		   !msh_GetSegmentMetadata(seg_node)->is_persistent)
		{
			msh_RemoveSegmentFromSharedList(seg_node);
			msh_RemoveSegmentFromList(seg_node);
			msh_DetachSegment(seg_node);
			did_remove_segment = TRUE;
		}
	}
	
	/* destroy the rest */
	msh_DestroyVariableNode(var_node);

	return did_remove_segment;
	
}


void msh_ClearVariableList(VariableList_T* var_list)
{
	VariableNode_T* curr_var_node;
	while(var_list->first != NULL)
	{
		curr_var_node = var_list->first;
		msh_RemoveVariableFromList(curr_var_node);
		msh_DestroyVariable(curr_var_node);
	}
}


void msh_CleanVariableList(VariableList_T* var_list, int shared_gc_override)
{
	VariableNode_T* curr_var_node, * next_var_node;
	SegmentNode_T* curr_seg_node;
	
	for(curr_var_node = var_list->first; curr_var_node != NULL; curr_var_node = next_var_node)
	{
		next_var_node = msh_GetNextVariable(curr_var_node);
		if(met_GetCrosslink(msh_GetVariableData(curr_var_node)) == NULL && msh_GetIsUsed(curr_var_node))
		{
			curr_seg_node = msh_GetSegmentNode(curr_var_node);
			
			/* does not remove the persistent variable, but does a lazy decrement of procs_using */
			msh_SetIsUsed(curr_var_node, FALSE);
			
			/* decrement number of processes using this variable; if this is the last variable then GC */
			if(msh_AtomicDecrement(&msh_GetSegmentMetadata(curr_seg_node)->procs_using) == 0
			   && (g_user_config.will_shared_gc || shared_gc_override)
			   && !msh_GetSegmentMetadata(curr_seg_node)->is_persistent)
			{
				msh_RemoveSegmentFromSharedList(curr_seg_node);
				msh_RemoveSegmentFromList(curr_seg_node);
				msh_DetachSegment(curr_seg_node);
			}
		}
	}
}


void msh_AddVariableToList(VariableList_T* var_list, VariableNode_T* var_node)
{
	msh_SetVariableList(var_node, var_list);
	msh_SetNextVariable(var_node, NULL);
	msh_SetPreviousVariable(var_node, var_list->last);
	
	if(var_list->first != NULL)
	{
		/* set pointer in the last node to this new node */
		msh_SetNextVariable(var_list->last, var_node);
	}
	else
	{
		/* set the front to this node */
		var_list->first = var_node;
	}
	
	/* append to the end of the list */
	var_list->last = var_node;
	
	
	if(var_list->mvar_table != NULL)
	{
		msh_AddSegmentToTable(var_list->mvar_table, msh_GetSegmentNode(var_node), msh_GetVariableData(var_node));
	}
	
}


mxArray* msh_CreateSharedDataCopy(VariableNode_T* var_node, int will_set_used)
{
	
	mxArray* shared_data_copy,* var = msh_GetVariableData(var_node);
	void* temp_mem;
	
	if(!msh_GetIsUsed(var_node) && will_set_used)
	{
		msh_SetIsUsed(var_node, TRUE);
		msh_AtomicIncrement(&msh_GetSegmentMetadata(msh_GetSegmentNode(var_node))->procs_using);
	}
	
	if(mxIsEmpty(var) && !mxIsSparse(var))
	{
		/* temporarily set as non-empty so the shared data copy gets a crosslink */
		temp_mem = mxMalloc(1);
		mxSetData(var, temp_mem);
		
		shared_data_copy = mxCreateSharedDataCopy(var);
	
		mxSetData(shared_data_copy, NULL);
		mxSetData(var, NULL);
		
		mxFree(temp_mem);
	}
	else
	{
		shared_data_copy = mxCreateSharedDataCopy(var);
	}
	
	return shared_data_copy;
}


void msh_RemoveVariableFromList(VariableNode_T* var_node)
{
	
	/* cache the variable list */
	VariableList_T* var_list;
	
	if((var_list = msh_GetVariableList(var_node)) == NULL)
	{
		return;
	}
	
	if(var_list->mvar_table != NULL)
	{
		msh_RemoveSegmentFromTable(var_list->mvar_table, msh_GetSegmentNode(var_node), msh_GetVariableData(var_node));
	}
	
	/* reset references in prev and next var node */
	if(msh_GetPreviousVariable(var_node) != NULL)
	{
		msh_SetNextVariable(msh_GetPreviousVariable(var_node), msh_GetNextVariable(var_node));
	}
	
	if(msh_GetNextVariable(var_node) != NULL)
	{
		msh_SetPreviousVariable(msh_GetNextVariable(var_node), msh_GetPreviousVariable(var_node));
	}
	
	if(var_list->first == var_node)
	{
		var_list->first = msh_GetNextVariable(var_node);
	}
	
	if(var_list->last == var_node)
	{
		var_list->last = msh_GetPreviousVariable(var_node);
	}
	
	msh_SetNextVariable(var_node, NULL);
	msh_SetPreviousVariable(var_node, NULL);
	msh_SetVariableList(var_node, NULL);
	
	/* decrement number of variables in the list */
	/* var_list->num_vars -= 1; */
	
}
