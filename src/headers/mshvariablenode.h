#ifndef MATSHARE_MSHVARIABLENODE_H
#define MATSHARE_MSHVARIABLENODE_H

#include "mex.h"

typedef struct VariableNode_t VariableNode_t;

#include "mshsegmentnode.h"

typedef struct VariableList_t
{
	VariableNode_t* first;
	VariableNode_t* last;
	uint32_T num_vars;
} VariableList_t;

/**
 * Creates a new variable node for the specified mxArray and segment node.
 *
 * @note Completely local.
 * @param seg_node The segment node linked to this variable node.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
VariableNode_t* msh_CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var);

void msh_DestroyVariableNode(VariableNode_t* var_node);

VariableList_t* msh_GetVariableList(VariableNode_t* var_node);
VariableNode_t* msh_GetNextVariable(VariableNode_t* var_node);
VariableNode_t* msh_GetPrevVariable(VariableNode_t* var_node);
mxArray* msh_GetVariableData(VariableNode_t* var_node);
SegmentNode_t* msh_GetSegmentNode(VariableNode_t* var_node);

void msh_SetVariableList(VariableNode_t* var_node, VariableList_t* var_list);
void msh_SetNextVariable(VariableNode_t* var_node, VariableNode_t* next_var_node);
void msh_SetPrevVariable(VariableNode_t* var_node, VariableNode_t* prev_var_node);
void msh_SetVariableData(VariableNode_t* var_node, mxArray* var);
void msh_SetSegmentNode(VariableNode_t* var_node, SegmentNode_t* seg_node);

#endif /* MATSHARE_MSHVARIABLENODE_H */
