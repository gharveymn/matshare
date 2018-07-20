/** mshvariables.h
 * Declares functions for variable creation and tracking.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_MSHLISTS_H
#define MATSHARE_MSHLISTS_H

#include "mshtypes.h"
#include "mshvariablenode.h"

typedef struct VariableList_t
{
	VariableNode_t* first;
	VariableNode_t* last;
	/* uint32_T num_vars; */
} VariableList_t;

/**
 * Creates a new MATLAB variable from the specified shared segment.
 *
 * @param seg_node The segment node associated to the shared data.
 * @return A variable node containing the new MATLAB variable.
 */
VariableNode_t* msh_CreateVariable(SegmentNode_t* seg_node);


/**
 * Detaches and destroys the variable contained in the specified variable node.
 *
 * @param var_node The variable node containing the variable to be destroyed.
 * @return whether the associated segment was also removed.
 */
int msh_DestroyVariable(VariableNode_t* var_node);


/**
 * Adds a new variable node to the specified variable list.
 *
 * @param var_list The variable list to which the variable node will be appended.
 * @param var_node The variable node to append.
 */
void msh_AddVariableToList(VariableList_t* var_list, VariableNode_t* var_node);


/**
 * Removes a new variable node from the specified variable list.
 *
 * @param var_node The variable node to removed.
 */
void msh_RemoveVariableFromList(VariableNode_t* var_node);


/**
 * Destroys all variables in the specified variable list.
 *
 * @param var_list The variable list which will be cleared.
 */
void msh_ClearVariableList(VariableList_t* var_list);


/**
 * Does garbage collection on variables which have no crosslinks
 * into the MATLAB workspace.
 *
 * @param var_list The variable list to be cleaned.
 */
void msh_CleanVariableList(VariableList_t* var_list);


mxArray* msh_CreateSharedDataCopy(VariableNode_t* var_node, int will_set_used);



/** Forward declaration of the global variable list. **/
extern VariableList_t g_local_var_list;


#endif /* MATSHARE_MSHLISTS_H */
