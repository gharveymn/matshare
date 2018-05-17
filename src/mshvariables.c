#include "headers/mshvariables.h"
#include "headers/mshsegments.h"
#include "headers/matlabutils.h"
#include "headers/mshutils.h"

/**
 * Tracks the mxArray by encapsulating it in a variable node, adding it to the variable list
 * specified and associating it to the segment node specified.
 *
 * @note Completely local.
 * @param var_list The variable list which will track the variable node containing the mxArray.
 * @param seg_node The segment which holds the mxArray data.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
static VariableNode_t* msh_TrackVariable(VariableList_t* var_list, SegmentNode_t* seg_node, mxArray* new_var);


/**
 * Creates a new variable node for the specified mxArray and segment node.
 *
 * @note Completely local.
 * @param seg_node The segment node linked to this variable node.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
static VariableNode_t* msh_CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var);


/**
 * Adds a new variable node to the specified variable list.
 *
 * @note Completely local.
 * @param var_list The variable list to which the variable node will be appended.
 * @param var_node The variable node to append.
 */
static void msh_AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node);


/**
 * Removes a new variable node from the specified variable list.
 *
 * @note Completely local.
 * @param var_list The variable list from which the variable node will be removed.
 * @param var_node The variable node to removed.
 */
static void msh_RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node);


/** public function definitions **/


VariableNode_t* msh_CreateVariable(VariableList_t* var_list, SegmentNode_t* seg_node)
{
	mxArray* new_var;
	if(seg_node->var_node != NULL)
	{
		ReadMexError(__FILE__, __LINE__, "CreateVariableError", "The segment targeted for variable creation already has a variable attached to it.");
	}
	
	new_var = msh_FetchVariable(msh_GetSegmentData(seg_node));
	mexMakeArrayPersistent(new_var);
	
	msh_AtomicIncrement(&msh_GetSegmentMetadata(seg_node)->procs_using);
	msh_GetSegmentMetadata(seg_node)->is_used = TRUE;
	
	return msh_TrackVariable(var_list, seg_node, new_var);
}


void msh_DestroyVariable(VariableNode_t* var_node)
{
	/* NULL all of the Matlab pointers */
	msh_DetachVariable(var_node->var);
	mxDestroyArray(var_node->var);
	
	/* decrement number of processes using this variable */
	msh_AtomicDecrement(&msh_GetSegmentMetadata(var_node->seg_node)->procs_using);
	
	/* tell the segment to stop tracking this variable */
	var_node->seg_node->var_node = NULL;
	
	/* remove tracking for the destroyed variable */
	msh_RemoveVariableNode(var_node->parent_var_list, var_node);
}


void msh_ClearVariableList(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node, * next_var_node;
	
	curr_var_node = var_list->first;
	while(curr_var_node != NULL)
	{
		next_var_node = curr_var_node->next;
		msh_DestroyVariable(curr_var_node);
		curr_var_node = next_var_node;
	}
}


/** static function definitions **/


static VariableNode_t* msh_TrackVariable(VariableList_t* var_list, SegmentNode_t* seg_node, mxArray* new_var)
{
	VariableNode_t* new_var_node = msh_CreateVariableNode(seg_node, new_var);
	msh_AddVariableNode(var_list, new_var_node);
	return new_var_node;
}


static VariableNode_t* msh_CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var)
{
	VariableNode_t* new_var_node = mxCalloc(1, sizeof(VariableNode_t));
	mexMakeMemoryPersistent(new_var_node);
	
	new_var_node->seg_node = seg_node;
	new_var_node->var = new_var;
	
	seg_node->var_node = new_var_node;
	
	return new_var_node;
}


static void msh_AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node)
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


static void msh_RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node)
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
	
	if(var_list->first == var_node)
	{
		var_list->first = var_node->next;
	}
	
	if(var_list->last == var_node)
	{
		var_list->last = var_node->prev;
	}
	
	/* decrement number of variables in the list */
	var_list->num_vars -= 1;
	
	mxFree(var_node);
	
}
