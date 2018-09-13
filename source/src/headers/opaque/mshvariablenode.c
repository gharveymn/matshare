/** mshvariablenode.c
 * Defines functions for creating and handling variable nodes.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "mex.h"

#include "mshvariablenode.h"
#include "mshsegmentnode.h"


struct VariableNode_T
{
	struct VariableList_T* parent_var_list;
	VariableNode_T* next; /* next variable that is currently fetched and being used */
	VariableNode_T* prev;
	mxArray* var;
	SegmentNode_T* seg_node;
	int is_used;
};


VariableNode_T* msh_CreateVariableNode(SegmentNode_T* seg_node, mxArray* new_var)
{
	VariableNode_T* new_var_node = mxCalloc(1, sizeof(VariableNode_T));
	mexMakeMemoryPersistent(new_var_node);
	
	new_var_node->seg_node = seg_node;
	new_var_node->var = new_var;
	
	if(seg_node != NULL)
	{
		msh_SetVariableNode(seg_node, new_var_node);
	}
	
	return new_var_node;
}


/** getters */

struct VariableList_T* msh_GetVariableList(VariableNode_T* var_node)
{
	return var_node->parent_var_list;
}


VariableNode_T* msh_GetNextVariable(VariableNode_T* var_node)
{
	return var_node->next;
}


VariableNode_T* msh_GetPreviousVariable(VariableNode_T* var_node)
{
	return var_node->prev;
}


mxArray* msh_GetVariableData(VariableNode_T* var_node)
{
	return var_node->var;
}


SegmentNode_T* msh_GetSegmentNode(VariableNode_T* var_node)
{
	return var_node->seg_node;
}


int msh_GetIsUsed(VariableNode_T* var_node)
{
	return var_node->is_used;
}


/** setters **/

void msh_SetVariableList(VariableNode_T* var_node, struct VariableList_T* var_list)
{
	var_node->parent_var_list = var_list;
}


void msh_SetNextVariable(VariableNode_T* var_node, VariableNode_T* next_var_node)
{
	var_node->next = next_var_node;
}


void msh_SetPreviousVariable(VariableNode_T* var_node, VariableNode_T* prev_var_node)
{
	var_node->prev = prev_var_node;
}


void msh_SetVariableData(VariableNode_T* var_node, mxArray* var)
{
	var_node->var = var;
}


void msh_SetSegmentNode(VariableNode_T* var_node, SegmentNode_T* seg_node)
{
	var_node->seg_node = seg_node;
}


void msh_SetIsUsed(VariableNode_T* var_node, int is_used)
{
	var_node->is_used = is_used;
}


void msh_DestroyVariableNode(VariableNode_T* var_node)
{
	mxFree(var_node);
}
