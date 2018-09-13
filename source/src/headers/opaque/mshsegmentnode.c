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

struct SegmentNode_T
{
	struct SegmentList_T* parent_seg_list;
	SegmentNode_T* next;
	SegmentNode_T* prev;
	VariableNode_T* var_node;
	SegmentInfo_T seg_info;
};


SegmentNode_T* msh_CreateSegmentNode(SegmentInfo_T* seg_info_cache)
{
	SegmentNode_T* new_seg_node = mxMalloc(sizeof(SegmentNode_T));
	mexMakeMemoryPersistent(new_seg_node);
	
	msh_SetSegmentInfo(new_seg_node, seg_info_cache);
	new_seg_node->var_node = NULL;
	new_seg_node->parent_seg_list = NULL;
	new_seg_node->prev = NULL;
	new_seg_node->next = NULL;
	
	return new_seg_node;
}


void msh_DestroySegmentNode(SegmentNode_T* seg_node)
{
	mxFree(seg_node);
}


/** getters **/

struct SegmentList_T* msh_GetSegmentList(SegmentNode_T* seg_node)
{
	return seg_node->parent_seg_list;
}


SegmentNode_T* msh_GetNextSegment(SegmentNode_T* seg_node)
{
	return seg_node->next;
}


SegmentNode_T* msh_GetPreviousSegment(SegmentNode_T* seg_node)
{
	return seg_node->prev;
}


SegmentInfo_T* msh_GetSegmentInfo(SegmentNode_T* seg_node)
{
	return &seg_node->seg_info;
}


VariableNode_T* msh_GetVariableNode(SegmentNode_T* seg_node)
{
	return seg_node->var_node;
}


/** setters **/

void msh_SetSegmentList(SegmentNode_T* seg_node, struct SegmentList_T* seg_list)
{
	seg_node->parent_seg_list = seg_list;
}


void msh_SetNextSegment(SegmentNode_T* seg_node, SegmentNode_T* next_seg_node)
{
	seg_node->next = next_seg_node;
}


void msh_SetPreviousSegment(SegmentNode_T* seg_node, SegmentNode_T* prev_seg_node)
{
	seg_node->prev = prev_seg_node;
}


void msh_SetSegmentInfo(SegmentNode_T* seg_node, SegmentInfo_T* seg_info)
{
	seg_node->seg_info = *seg_info;
}


void msh_SetVariableNode(SegmentNode_T* seg_node, VariableNode_T* var_node)
{
	seg_node->var_node = var_node;
}
