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


VariableNode_t* msh_CreateVariable(SegmentNode_t* seg_node)
{
	
	mxArray* new_var;
	VariableNode_t* last_virtual_scalar,* new_var_node;
	
	if(msh_GetVariableNode(seg_node) != NULL)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "CreateVariableError", "The segment targeted for variable creation already has a variable attached to it.");
	}
	
	/* keep track of the last virtual scalar before the fetch operation */
	last_virtual_scalar = g_virtual_scalar_list.last;
	
	new_var = msh_FetchVariable(msh_GetSegmentData(seg_node));
	mexMakeArrayPersistent(new_var);
	
	new_var_node = msh_CreateVariableNode(seg_node, new_var, msh_GetSegmentData(seg_node));
	
	msh_SetFirstVirtualScalar(new_var_node, ((last_virtual_scalar == NULL)? g_virtual_scalar_list.first : msh_GetNextVariable(last_virtual_scalar)));
	msh_SetLastVirtualScalar(new_var_node, g_virtual_scalar_list.last);
	
	return new_var_node;
}


int msh_DestroyVariable(VariableNode_t* var_node)
{
	
	VariableNode_t* curr_virtual_scalar;
	SegmentNode_t* seg_node = msh_GetSegmentNode(var_node);
	int did_remove_segment = FALSE;
	
	/* detach all of the Matlab pointers */
	msh_DetachVariable(msh_GetVariableData(var_node));
	mxDestroyArray(msh_GetVariableData(var_node));
	
	/* remove tracking for this variable */
	msh_SetVariableNode(seg_node, NULL);
	
	/* remove virtual scalar tracking */
	for(curr_virtual_scalar = msh_GetFirstVirtualScalar(var_node);
			curr_virtual_scalar != NULL && curr_virtual_scalar != msh_GetNextVariable(msh_GetLastVirtualScalar(var_node));
			curr_virtual_scalar = msh_GetNextVariable(curr_virtual_scalar))
	{
		msh_RemoveVariableFromList(curr_virtual_scalar);
		msh_DestroyVariableNode(curr_virtual_scalar);
	}

	if(msh_GetIsUsed(var_node))
	{
		msh_SetIsUsed(var_node, FALSE);
		if((msh_AtomicDecrement(&msh_GetSegmentMetadata(seg_node)->procs_using) == 0) &&
		   g_shared_info->user_defined.will_shared_gc &&
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


void msh_ClearVariableList(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node;
	while(var_list->first != NULL)
	{
		curr_var_node = var_list->first;
		msh_RemoveVariableFromList(curr_var_node);
		msh_DestroyVariable(curr_var_node);
	}
}


void msh_CleanVariableList(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node, * next_var_node;
	
	for(curr_var_node = var_list->first; curr_var_node != NULL; curr_var_node = next_var_node)
	{
		next_var_node = msh_GetNextVariable(curr_var_node);
		if(met_GetCrosslink(msh_GetVariableData(curr_var_node)) == NULL)
		{
			msh_RemoveVariableFromList(curr_var_node);
			msh_DestroyVariable(curr_var_node);
		}
	}
}


void msh_AddVariableToList(VariableList_t* var_list, VariableNode_t* var_node)
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
	
}


mxArray* msh_CreateSharedDataCopy(VariableNode_t* var_node, int will_set_used)
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


void msh_RemoveVariableFromList(VariableNode_t* var_node)
{
	
	/* cache the variable list */
	VariableList_t* var_list = msh_GetVariableList(var_node);
	
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
	
	/* decrement number of variables in the list */
	/* var_list->num_vars -= 1; */
	
}