#ifndef MATSHARE_MSHLISTS_H
#define MATSHARE_MSHLISTS_H

#include "mshtypes.h"
#include "matshare_.h"

/**
 * Note: matshare links segments in shared memory by assigning each segment a "segment number."
 *       This is done as a doubly linked list, which each process tracks locally. Hence, each
 *       process must also update its local tracking before doing anything to modify the shared
 *       memory list.
 *
 *       However, not all functions here require this condition. Functions which interact with
 *       shared memory indicate such in their documentation.
 */


/**
 * Creates a new shared memory segment with the specified size and hooks it into
 * the shared memory linked list. Automatically ensures that the segment tracking
 * by this process is up to date.
 *
 * @note Modifies the shared memory linked list.
 * @param seg_sz The size of the new segment.
 * @return A struct containing information about the segment.
 */
SegmentInfo_t CreateSegment(size_t seg_sz);


/**
 * Opens a segment in shared memory based on the segment number.
 *
 * @note Interacts with shared memory, but does not modify.
 * @param seg_num The segment number of the shared memory segment (used by matshare to identify the memory segment).
 * @return A struct containing information about the segment.
 */
SegmentInfo_t OpenSegment(msh_segmentnumber_t seg_num);


/**
 * Create a new segment node.
 *
 * @note Completely local.
 * @param seg_info A struct containing info about a shared segment created either by OpenSegment or CreateSegment.
 * @return The newly allocated segment node.
 */
SegmentNode_t* CreateSegmentNode(SegmentInfo_t seg_info);


/**
 * Closes the memory segment that the segment node is associated with.
 *
 * @note Interacts but does not modify the shared memory linked list.
 * @param seg_node The segment node containing the segment to close.
 */
void CloseSegment(SegmentNode_t* seg_node);


/**
 * Destroys the memory segment associated with the specified segment node. Also
 * removes the segment from the shared segment linked list and marks the segment
 * for deletion by other processes.
 *
 * @note Modifies shared memory.
 * @param seg_node The segment node associated to the segment to be destroyed.
 */
void DestroySegment(SegmentNode_t* seg_node);


/**
 * Add a segment node to the specified segment list.
 *
 * @note Completely local.
 * @param seg_list The segment list which the segment node will be appended to.
 * @param seg_node The segment node to be appended.
 */
void AddSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);


/**
 * Removes a segment node from the specified segment list.
 *
 * @note Completely local.
 * @param seg_list The segment list which the segment node will be removed from.
 * @param seg_node The segment to be removed.
 */
void RemoveSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);


/**
 * Closes the segment, removes the segment from the specified segment list
 * and frees the segment node.
 *
 * @note Completely local.
 * @param seg_list
 * @param seg_node
 */
void CloseSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);


void DestroySegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);


VariableNode_t* CreateVariableNode(SegmentNode_t* seg_node);


void AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node);


void RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node);


void DestroyVariable(VariableNode_t* var_node);


void DestroyVariableNode(VariableNode_t* var_node);


void CleanVariableList(VariableList_t* var_list);


void CleanSegmentList(SegmentList_t* seg_list);


void DestroySegmentList(SegmentList_t* seg_list);


void UpdateSharedSegments(void);


#endif //MATSHARE_MSHLISTS_H
