/** mshsegmentnode.c
 * Provides a definition for segment nodes and provides access
 * functions.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#include "mex.h"

#include "mshsegmentnode.h"
#include "mshvariablenode.h"

struct SegmentNode_t
{
	struct SegmentList_t* parent_seg_list;
	SegmentNode_t* next;
	SegmentNode_t* prev;
	VariableNode_t* var_node;
	SegmentInfo_t seg_info;
};


SegmentNode_t* msh_CreateSegmentNode(SegmentInfo_t* seg_info_cache)
{
	SegmentNode_t* new_seg_node = mxMalloc(sizeof(SegmentNode_t));
	mexMakeMemoryPersistent(new_seg_node);
	
	msh_SetSegmentInfo(new_seg_node, seg_info_cache);
	new_seg_node->var_node = NULL;
	new_seg_node->parent_seg_list = NULL;
	new_seg_node->prev = NULL;
	new_seg_node->next = NULL;
	
	return new_seg_node;
}


void msh_DestroySegmentNode(SegmentNode_t* seg_node)
{
	mxFree(seg_node);
}


/** getters **/

SegmentList_t* msh_GetSegmentList(SegmentNode_t* seg_node)
{
	return seg_node->parent_seg_list;
}


SegmentNode_t* msh_GetNextSegment(SegmentNode_t* seg_node)
{
	return seg_node->next;
}


SegmentNode_t* msh_GetPreviousSegment(SegmentNode_t* seg_node)
{
	return seg_node->prev;
}


SegmentInfo_t* msh_GetSegmentInfo(SegmentNode_t* seg_node)
{
	return &seg_node->seg_info;
}


VariableNode_t* msh_GetVariableNode(SegmentNode_t* seg_node)
{
	return seg_node->var_node;
}


/** setters **/

void msh_SetSegmentList(SegmentNode_t* seg_node, SegmentList_t* seg_list)
{
	seg_node->parent_seg_list = seg_list;
}


void msh_SetNextSegment(SegmentNode_t* seg_node, SegmentNode_t* next_seg_node)
{
	seg_node->next = next_seg_node;
}


void msh_SetPreviousSegment(SegmentNode_t* seg_node, SegmentNode_t* prev_seg_node)
{
	seg_node->prev = prev_seg_node;
}


void msh_SetSegmentInfo(SegmentNode_t* seg_node, SegmentInfo_t* seg_info)
{
	seg_node->seg_info = *seg_info;
}


void msh_SetVariableNode(SegmentNode_t* seg_node, VariableNode_t* var_node)
{
	seg_node->var_node = var_node;
}
