#ifndef MATSHARE_MSHLISTS_H
#define MATSHARE_MSHLISTS_H

#include "mshtypes.h"

typedef struct VariableNode_t
{
	struct VariableList_t* parent_var_list;
	struct VariableNode_t* next; /* next variable that is currently fetched and being used */
	struct VariableNode_t* prev;
	mxArray* var;
	struct SegmentNode_t* seg_node;
} VariableNode_t;

typedef struct SegmentNode_t
{
	struct SegmentList_t* parent_seg_list;
	struct SegmentNode_t* next;
	struct SegmentNode_t* prev;
	VariableNode_t* var_node;
	struct SegmentInfo_t seg_info;
} SegmentNode_t;

/**
 * Note: matshare links segments in shared memory by assigning each segment a "segment number."
 *       This is done as a doubly linked list, which each process tracks locally. Hence, each
 *       process must also update its local tracking before doing anything to modify the shared
 *       memory list.
 *
 *       However, not all functions here require this condition. Functions which interact with
 *       shared memory indicate such in their documentation.
 */


SegmentMetadata_t* msh_GetSegmentMetadata(SegmentNode_t* seg_node);

SharedVariableHeader_t* MshGetSegmentData(SegmentNode_t* seg_node);

size_t msh_FindSegmentSize(const mxArray* in_var);

/**
 * Creates a new shared memory segment with the specified size and hooks it into
 * the shared memory linked list. Automatically ensures that the segment tracking
 * by this process is up to date.
 *
 * @note Modifies the shared memory linked list.
 * @param seg_list The list which will track the segment.
 * @param seg_sz The size of the new segment.
 * @return A struct containing information about the segment.
 */
SegmentNode_t* CreateSegment(SegmentList_t* seg_list, size_t seg_sz);


/**
 * Opens a segment in shared memory based on the segment number.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_list The list which will track the segment.
 * @param seg_num The segment number of the shared memory segment (used by matshare to identify the memory segment).
 * @return A struct containing information about the segment.
 */
SegmentNode_t* OpenSegment(SegmentList_t* seg_list, msh_segmentnumber_t seg_num);


/**
 * Closes the memory segment that the segment node is associated with.
 * This should only run if the segment has been invalidated.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_node The segment node containing the segment to close.
 */
void DetachSegment(SegmentNode_t* seg_node);


/**
 * Destroys the memory segment associated with the specified segment node. Also
 * removes the segment from the shared segment linked list and marks the segment
 * for deletion by other processes.
 *
 * @note Modifies the shared memory linked list.
 * @param seg_node The segment node associated to the segment to be destroyed.
 */
void DestroySegment(SegmentNode_t* seg_node);


/**
 * Creates a new MATLAB variable from the specified shared segment, stores
 * it in a variable node, and adds the variable node to the specified list.
 *
 * @note Completely local.
 * @param var_list The list which will track the variable.
 * @param seg_node The segment node associated to the shared data.
 * @return The variable node containing the new MATLAB variable.
 */
VariableNode_t* CreateVariable(VariableList_t* var_list, SegmentNode_t* seg_node);


/**
 * Detaches and destroys the variable contained in the specified variable node.
 *
 * @note Interacts with shared memory but does not modify the shared linked list.
 * @param var_node The variable node containing the variable to be destroyed.
 */
void DestroyVariable(VariableNode_t* var_node);


/**
 * Destroys all variables in the specified variable list.
 *
 * @param var_list The variable list which will be cleared.
 */
void ClearVariableList(VariableList_t* var_list);


/**
 * Detaches all segments in the specified segment list. Must be up to date.
 *
 * @note May call functions which modify the shared memory linked list.
 * @param seg_list The segment list for which all segments will be detached.
 */
void DetachSegmentList(SegmentList_t* seg_list);


/**
 * Destroys all segments in the specified segment list.
 *
 * @note Calls functions which modify the shared memory linked list.
 * @param seg_list The segment list to be cleared.
 */
void ClearSegmentList(SegmentList_t* seg_list);


/**
 * Updates the local tracking of the shared memory linked list. Must be called
 * before making modifications to shared memory.
 */
void UpdateSharedSegments(void);


#endif /* MATSHARE_MSHLISTS_H */
