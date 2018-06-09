#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/matlabutils.h"
#include "headers/mshutils.h"


/**
 * Creates a new variable node for the specified mxArray and segment node.
 *
 * @note Completely local.
 * @param seg_node The segment node linked to this variable node.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
static VariableNode_t* msh_CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var);


/** public function definitions **/


VariableNode_t* msh_CreateVariable(SegmentNode_t* seg_node)
{
	mxArray* new_var;
	if(seg_node->var_node != NULL)
	{
		ReadMexError(__FILE__, __LINE__, ERROR_SEVERITY_INTERNAL, 0, "CreateVariableError", "The segment targeted for variable creation already has a variable attached to it.");
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
	msh_DetachVariable(var_node->var);
	mxDestroyArray(var_node->var);
	
	/* remove tracking for this variable */
	var_node->seg_node->var_node = NULL;
	
	/* decrement number of processes using this variable; if this is the last variable then GC */
	if(msh_AtomicDecrement(&msh_GetSegmentMetadata(var_node->seg_node)->procs_using) == 0 &&
	   g_shared_info->user_defined.sharetype == msh_SHARETYPE_COPY &&
	   g_shared_info->user_defined.will_gc &&
	   g_local_info.num_registered_objs == 0)
	{
		msh_RemoveSegmentFromSharedList(var_node->seg_node);
		msh_RemoveSegmentFromList(var_node->seg_node);
		msh_DetachSegment(var_node->seg_node);
		did_destroy_segment = TRUE;
	}
	
	/* destroy the rest */
	mxFree(var_node);
	
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
		next_var_node = curr_var_node->next;
		if(msh_GetCrosslink(curr_var_node->var) == NULL)
		{
			msh_RemoveVariableFromList(curr_var_node);
			msh_DestroyVariable(curr_var_node);
		}
	}
}


/** static function definitions **/


static VariableNode_t* msh_CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var)
{
	VariableNode_t* new_var_node = mxCalloc(1, sizeof(VariableNode_t));
	mexMakeMemoryPersistent(new_var_node);
	
	new_var_node->seg_node = seg_node;
	new_var_node->var = new_var;
	
	seg_node->var_node = new_var_node;
	
	return new_var_node;
}


void msh_AddVariableToList(VariableList_t* var_list, VariableNode_t* var_node)
{
	var_node->parent_var_list = var_list;
	
	var_node->next = NULL;
	var_node->prev = var_list->last;
	
	if(var_list->num_vars != 0)
	{
		/* set pointer in the last node to this new node */
		var_list->last->next = var_node;
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
	/* reset references in prev and next var node */
	if(var_node->prev != NULL)
	{
		var_node->prev->next = var_node->next;
	}
	
	if(var_node->next != NULL)
	{
		var_node->next->prev = var_node->prev;
	}
	
	if(var_node->parent_var_list->first == var_node)
	{
		var_node->parent_var_list->first = var_node->next;
	}
	
	if(var_node->parent_var_list->last == var_node)
	{
		var_node->parent_var_list->last = var_node->prev;
	}
	
	var_node->next = NULL;
	var_node->prev = NULL;
	
	/* decrement number of variables in the list */
	var_node->parent_var_list->num_vars -= 1;
	
}
