#ifndef MATSHARE_MSHLISTS_H
#define MATSHARE_MSHLISTS_H

#include "mshtypes.h"
#include "matshare_.h"

SegmentInfo_t CreateSegment(size_t seg_sz);

SegmentInfo_t OpenSegment(seg_num_t seg_num);

SegmentNode_t* CreateSegmentNode(SegmentInfo_t seg_info);

void AddSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);

void RemoveSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);

void DestroySegmentNode(SegmentNode_t* seg_node);

VariableNode_t* CreateVariableNode(SegmentNode_t* seg_node);

void AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node);

void RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node);

void DestroyVariable(VariableNode_t* var_node);

void DestroyVariableNode(VariableNode_t* var_node);

void CleanVariableList(VariableList_t* var_list);

void CleanSegmentList(SegmentList_t* seg_list);

void DestroySegmentList(SegmentList_t* seg_list);

void CloseSegmentNode(SegmentNode_t* seg_node);

#endif //MATSHARE_MSHLISTS_H
