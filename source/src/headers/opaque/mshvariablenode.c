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


struct VariableNode_t
{
	VariableList_t* parent_var_list;
	VariableNode_t* next; /* next variable that is currently fetched and being used */
	VariableNode_t* prev;
	mxArray* var;
	SegmentNode_t* seg_node;
	int is_used;
};


VariableNode_t* msh_CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var)
{
	VariableNode_t* new_var_node = mxCalloc(1, sizeof(VariableNode_t));
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

VariableList_t* msh_GetVariableList(VariableNode_t* var_node)
{
	return var_node->parent_var_list;
}


VariableNode_t* msh_GetNextVariable(VariableNode_t* var_node)
{
	return var_node->next;
}


VariableNode_t* msh_GetPreviousVariable(VariableNode_t* var_node)
{
	return var_node->prev;
}


mxArray* msh_GetVariableData(VariableNode_t* var_node)
{
	return var_node->var;
}


SegmentNode_t* msh_GetSegmentNode(VariableNode_t* var_node)
{
	return var_node->seg_node;
}


int msh_GetIsUsed(VariableNode_t* var_node)
{
	return var_node->is_used;
}


/** setters **/

void msh_SetVariableList(VariableNode_t* var_node, VariableList_t* var_list)
{
	var_node->parent_var_list = var_list;
}


void msh_SetNextVariable(VariableNode_t* var_node, VariableNode_t* next_var_node)
{
	var_node->next = next_var_node;
}


void msh_SetPreviousVariable(VariableNode_t* var_node, VariableNode_t* prev_var_node)
{
	var_node->prev = prev_var_node;
}


void msh_SetVariableData(VariableNode_t* var_node, mxArray* var)
{
	var_node->var = var;
}


void msh_SetSegmentNode(VariableNode_t* var_node, SegmentNode_t* seg_node)
{
	var_node->seg_node = seg_node;
}


void msh_SetIsUsed(VariableNode_t* var_node, int is_used)
{
	var_node->is_used = is_used;
}


void msh_DestroyVariableNode(VariableNode_t* var_node)
{
	mxFree(var_node);
}
