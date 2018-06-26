/** mshvariablenode.h
 * Defines the variable list struct and declares functions for
 * handling variable nodes.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_MSHVARIABLENODE_H
#define MATSHARE_MSHVARIABLENODE_H

#include "mshbasictypes.h"

/* forward declaration to avoid include */
typedef struct mxArray_tag mxArray;

/* forward declaration, defined in source file */
typedef struct VariableNode_t VariableNode_t;

/* forward declaration, definition in mshsegmentnode.h */
typedef struct SegmentNode_t SegmentNode_t;


typedef struct VariableList_t
{
	VariableNode_t* first;
	VariableNode_t* last;
	/* uint32_T num_vars; */
} VariableList_t;


/** Forward declaration for SharedVariableHeader_t **/
typedef struct SharedVariableHeader_t SharedVariableHeader_t;


/**
 * Creates a new variable node for the specified mxArray and segment node.
 *
 * @param seg_node The segment node linked to this variable node.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
VariableNode_t* msh_CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var, SharedVariableHeader_t* shared_header);


/**
 * Destroys the variable node.
 *
 * @param var_node The variable node to be destroyed.
 */
void msh_DestroyVariableNode(VariableNode_t* var_node);


/**
 * Gets the parent variable list for the specified variable node.
 *
 * @param var_node The variable node.
 * @return The parents variable list.
 */
VariableList_t* msh_GetVariableList(VariableNode_t* var_node);


/**
 * Gets the next variable node.
 *
 * @param var_node The variable node.
 * @return The next variable node.
 */
VariableNode_t* msh_GetNextVariable(VariableNode_t* var_node);


/**
 * Gets the previous variable node.
 *
 * @param var_node The variable node.
 * @return The previous variable node.
 */
VariableNode_t* msh_GetPreviousVariable(VariableNode_t* var_node);


/**
 * Gets the mxArray tracked by the variable node.
 *
 * @param var_node The variable node.
 * @return The tracked mxArray.
 */
mxArray* msh_GetVariableData(VariableNode_t* var_node);


/**
 * Gets the segment node whose segment this variable uses.
 *
 * @param var_node The variable node.
 * @return The segment node whose segment this variable uses.
 */
SegmentNode_t* msh_GetSegmentNode(VariableNode_t* var_node);


VariableNode_t* msh_GetFirstVirtualScalar(VariableNode_t* var_node);


VariableNode_t* msh_GetLastVirtualScalar(VariableNode_t* var_node);


SharedVariableHeader_t* msh_GetSharedHeader(VariableNode_t* var_node);

int msh_GetIsUsed(VariableNode_t* var_node);


/**
 * Sets the parent variable list.
 *
 * @param var_node The variable node.
 * @param var_list The variable list.
 */
void msh_SetVariableList(VariableNode_t* var_node, VariableList_t* var_list);


/**
 * Sets the next variable node.
 *
 * @param var_node The variable node.
 * @param next_var_node The next variable node.
 */
void msh_SetNextVariable(VariableNode_t* var_node, VariableNode_t* next_var_node);


/**
 * Sets the previous variable node.
 *
 * @param var_node The variable node.
 * @param prev_var_node The previous variable node.
 */
void msh_SetPreviousVariable(VariableNode_t* var_node, VariableNode_t* prev_var_node);


/**
 * Sets the mxArray for this variable node.
 *
 * @param var_node The variable node.
 * @param var The mxArray.
 */
void msh_SetVariableData(VariableNode_t* var_node, mxArray* var);


/**
 * Sets the segment node which this variable node uses.
 *
 * @param var_node The variable node.
 * @param seg_node The segment node.
 */
void msh_SetSegmentNode(VariableNode_t* var_node, SegmentNode_t* seg_node);


void msh_SetFirstVirtualScalar(VariableNode_t* var_node, VariableNode_t* virtual_scalar_node);


void msh_SetLastVirtualScalar(VariableNode_t* var_node, VariableNode_t* virtual_scalar_node);


void msh_SetSharedHeader(VariableNode_t* var_node, SharedVariableHeader_t* shared_header);

void msh_SetIsUsed(VariableNode_t* var_node, int is_used);

#endif /* MATSHARE_MSHVARIABLENODE_H */
