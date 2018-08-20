/** mshvariablenode.h
 * Defines the variable list struct and declares functions for
 * handling variable nodes.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_MSHVARIABLENODE_H
#define MATSHARE_MSHVARIABLENODE_H

#include "mshbasictypes.h"

/* forward declaration to avoid include */
typedef struct mxArray_tag mxArray;

/* forward declaration, defined in source file */
typedef struct VariableNode_T VariableNode_T;

/* forward declaration, definition in mshsegmentnode.h */
typedef struct SegmentNode_T SegmentNode_T;

/* forward declaration */
typedef struct VariableList_T VariableList_T;

/** Forward declaration for SharedVariableHeader_t **/
typedef struct SharedVariableHeader_T SharedVariableHeader_T;


/**
 * Creates a new variable node for the specified mxArray and segment node.
 *
 * @param seg_node The segment node linked to this variable node.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
VariableNode_T* msh_CreateVariableNode(SegmentNode_T* seg_node, mxArray* new_var);


/**
 * Destroys the variable node.
 *
 * @param var_node The variable node to be destroyed.
 */
void msh_DestroyVariableNode(VariableNode_T* var_node);


/**
 * Gets the parent variable list for the specified variable node.
 *
 * @param var_node The variable node.
 * @return The parents variable list.
 */
VariableList_T* msh_GetVariableList(VariableNode_T* var_node);


/**
 * Gets the next variable node.
 *
 * @param var_node The variable node.
 * @return The next variable node.
 */
VariableNode_T* msh_GetNextVariable(VariableNode_T* var_node);


/**
 * Gets the previous variable node.
 *
 * @param var_node The variable node.
 * @return The previous variable node.
 */
VariableNode_T* msh_GetPreviousVariable(VariableNode_T* var_node);


/**
 * Gets the mxArray tracked by the variable node.
 *
 * @param var_node The variable node.
 * @return The tracked mxArray.
 */
mxArray* msh_GetVariableData(VariableNode_T* var_node);


/**
 * Gets the segment node whose segment this variable uses.
 *
 * @param var_node The variable node.
 * @return The segment node whose segment this variable uses.
 */
SegmentNode_T* msh_GetSegmentNode(VariableNode_T* var_node);


int msh_GetIsUsed(VariableNode_T* var_node);


/**
 * Sets the parent variable list.
 *
 * @param var_node The variable node.
 * @param var_list The variable list.
 */
void msh_SetVariableList(VariableNode_T* var_node, VariableList_T* var_list);


/**
 * Sets the next variable node.
 *
 * @param var_node The variable node.
 * @param next_var_node The next variable node.
 */
void msh_SetNextVariable(VariableNode_T* var_node, VariableNode_T* next_var_node);


/**
 * Sets the previous variable node.
 *
 * @param var_node The variable node.
 * @param prev_var_node The previous variable node.
 */
void msh_SetPreviousVariable(VariableNode_T* var_node, VariableNode_T* prev_var_node);


/**
 * Sets the mxArray for this variable node.
 *
 * @param var_node The variable node.
 * @param var The mxArray.
 */
void msh_SetVariableData(VariableNode_T* var_node, mxArray* var);


/**
 * Sets the segment node which this variable node uses.
 *
 * @param var_node The variable node.
 * @param seg_node The segment node.
 */
void msh_SetSegmentNode(VariableNode_T* var_node, SegmentNode_T* seg_node);


void msh_SetIsUsed(VariableNode_T* var_node, int is_used);

#endif /* MATSHARE_MSHVARIABLENODE_H */
