#ifndef MATSHARE_MSHSEGMENTS_H
#define MATSHARE_MSHSEGMENTS_H

#include "mshtypes.h"

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

SharedVariableHeader_t* msh_GetSegmentData(SegmentNode_t* seg_node);

size_t msh_FindSegmentSize(size_t data_size);

/**
 * Creates a new shared memory segment with the specified size and hooks it into
 * the shared memory linked list. Automatically ensures that the segment tracking
 * by this process is up to date.
 *
 * @note Modifies the shared memory linked list.
 * @param seg_list The list which will track the segment.
 * @param data_size The size of the new segment.
 * @return A struct containing information about the segment.
 */
SegmentNode_t* msh_CreateSegment(size_t data_size);


/**
 * Opens a segment in shared memory based on the segment number.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_list The list which will track the segment.
 * @param seg_num The segment number of the shared memory segment (used by matshare to identify the memory segment).
 * @return A struct containing information about the segment.
 */
SegmentNode_t* msh_OpenSegment(msh_segmentnumber_t seg_num);


/**
 * Closes the memory segment that the segment node is associated with.
 * This should only run if the segment has been invalidated.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_node The segment node containing the segment to close.
 */
void msh_DetachSegment(SegmentNode_t* seg_node);


void msh_AddSegmentToSharedList(SegmentNode_t* seg_node);

void msh_RemoveSegmentFromSharedList(SegmentNode_t* seg_node);


/**
 * Detaches all segments in the specified segment list. Must be up to date.
 *
 * @note May call functions which modify the shared memory linked list.
 * @param seg_list The segment list for which all segments will be detached.
 */
void msh_DetachSegmentList(SegmentList_t* seg_list);


/**
 * Destroys all segments in the specified segment list.
 *
 * @note Calls functions which modify the shared memory linked list.
 * @param seg_list The segment list to be cleared.
 */
void msh_ClearSharedSegments(SegmentList_t* seg_list);


/**
 * Updates the local tracking of the shared memory linked list. Must be called
 * before making modifications to shared memory.
 */
void msh_UpdateSegmentTracking(SegmentList_t* seg_list);

/**
 * Add a segment node to the specified segment list.
 *
 * @note Completely local.
 * @param seg_list The segment list which the segment node will be appended to.
 * @param seg_node The segment node to be appended.
 */
void msh_AddSegmentToList(SegmentList_t* seg_list, SegmentNode_t* seg_node);



void msh_PlaceSegmentAtEnd(SegmentNode_t* seg_node);


/**
 * Removes a segment node from the specified segment list.
 *
 * @note Completely local.
 * @param seg_list The segment list which the segment node will be removed from.
 * @param seg_node The segment to be removed.
 */
void msh_RemoveSegmentFromList(SegmentNode_t* seg_node);

void msh_InitializeTable(SegmentTable_t* seg_table);
void msh_AddSegmentToTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node, uint32_T num_segs);
void msh_RemoveSegmentFromTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node);
void msh_DestroyTable(SegmentTable_t* seg_table);
SegmentNode_t* msh_FindSegmentNode(SegmentTable_t* seg_table, msh_segmentnumber_t seg_num);

void msh_CleanSegmentList(SegmentList_t* seg_list);

void msh_UpdateLatestSegment(SegmentList_t* seg_list);

handle_t msh_CreateSharedMemory(char_t* segment_name, size_t segment_size);
handle_t msh_OpenSharedMemory(char_t* segment_name);

void* msh_MapMemory(handle_t segment_handle, size_t map_sz);

void msh_UnmapMemory(void* segment_pointer, size_t map_sz);

void msh_CloseSharedMemory(handle_t segment_handle);

void msh_LockMemory(void* ptr, size_t sz);
void msh_UnlockMemory(void* ptr, size_t sz);

#endif /* MATSHARE_MSHSEGMENTS_H */
