#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/mlerrorutils.h"
#include "headers/mshutils.h"


/** public function definitions **/


VariableNode_t* msh_CreateVariable(SegmentNode_t* seg_node)
{
	mxArray* new_var;
	if(msh_GetVariableNode(seg_node) != NULL)
	{
		meu_PrintMexError(__FILE__, __LINE__, MEU_SEVERITY_INTERNAL, 0, "CreateVariableError", "The segment targeted for variable creation already has a variable attached to it.");
	}
	
	new_var = msh_FetchVariable(msh_GetSegmentData(seg_node));
	mexMakeArrayPersistent(new_var);
	
	msh_AtomicIncrement(&msh_GetSegmentMetadata(seg_node)->procs_using);
	
	return msh_CreateVariableNode(seg_node, new_var);
}


bool_t msh_DestroyVariable(VariableNode_t* var_node)
{
	
	bool_t did_destroy_segment = FALSE;
	
	/* detach all of the Matlab pointers */
	msh_DetachVariable(msh_GetVariableData(var_node));
	mxDestroyArray(msh_GetVariableData(var_node));
	
	/* remove tracking for this variable */
	msh_SetVariableNode(msh_GetSegmentNode(var_node), NULL);
	
	/* decrement number of processes using this variable; if this is the last variable then GC */
	if(msh_AtomicDecrement(&msh_GetSegmentMetadata(msh_GetSegmentNode(var_node))->procs_using) == 0 &&
	   g_shared_info->user_defined.sharetype == msh_SHARETYPE_COPY &&
	   g_shared_info->user_defined.will_gc)
	{
		msh_RemoveSegmentFromSharedList(msh_GetSegmentNode(var_node));
		msh_RemoveSegmentFromList(msh_GetSegmentNode(var_node));
		msh_DetachSegment(msh_GetSegmentNode(var_node));
		did_destroy_segment = TRUE;
	}
	
	/* destroy the rest */
	msh_DestroyVariableNode(var_node);
	
	return did_destroy_segment;
	
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
		if(msh_GetCrosslink(msh_GetVariableData(curr_var_node)) == NULL)
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
	msh_SetPrevVariable(var_node, var_list->last);
	
	if(var_list->num_vars != 0)
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
	
	/* increment number of variables */
	var_list->num_vars += 1;
	
}


void msh_RemoveVariableFromList(VariableNode_t* var_node)
{
	
	/* cache the variable list */
	VariableList_t* var_list = msh_GetVariableList(var_node);
	
	/* reset references in prev and next var node */
	if(msh_GetPrevVariable(var_node) != NULL)
	{
		msh_SetNextVariable(msh_GetPrevVariable(var_node), msh_GetNextVariable(var_node));
	}
	
	if(msh_GetNextVariable(var_node) != NULL)
	{
		msh_SetPrevVariable(msh_GetNextVariable(var_node), msh_GetPrevVariable(var_node));
	}
	
	if(var_list->first == var_node)
	{
		var_list->first = msh_GetNextVariable(var_node);
	}
	
	if(var_list->last == var_node)
	{
		var_list->last = msh_GetPrevVariable(var_node);
	}
	
	msh_SetNextVariable(var_node, NULL);
	msh_SetPrevVariable(var_node, NULL);
	
	/* decrement number of variables in the list */
	var_list->num_vars -= 1;
	
}
