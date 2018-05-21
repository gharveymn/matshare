#ifndef MATSHARE_MSHLISTS_H
#define MATSHARE_MSHLISTS_H

#include "mshtypes.h"


/**
 * Creates a new MATLAB variable from the specified shared segment, stores
 * it in a variable node, and adds the variable node to the specified list.
 *
 * @note Completely local.
 * @param var_list The list which will track the variable.
 * @param seg_node The segment node associated to the shared data.
 * @return The variable node containing the new MATLAB variable.
 */
VariableNode_t* msh_CreateVariable(VariableList_t* var_list, struct SegmentNode_t* seg_node);


/**
 * Detaches and destroys the variable contained in the specified variable node.
 *
 * @note Interacts with shared memory but does not modify the shared linked list.
 * @param var_node The variable node containing the variable to be destroyed.
 */
void msh_DestroyVariable(VariableNode_t* var_node);


/**
 * Destroys all variables in the specified variable list.
 *
 * @param var_list The variable list which will be cleared.
 */
void msh_ClearVariableList(VariableList_t* var_list);

void msh_CleanVariableList(VariableList_t* var_list);


#endif /* MATSHARE_MSHLISTS_H */
