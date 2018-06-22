/** mshsegment.h
 * Declares functions for segment creation and tracking.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef MATSHARE_MSHSEGMENTS_H
#define MATSHARE_MSHSEGMENTS_H

#include "mshbasictypes.h"
#include "mshsegmentnode.h"

/* forward declaration, definition in mshheader.c */
typedef struct SharedVariableHeader_t SharedVariableHeader_t;

/**
 * Note: matshare links segments in shared memory by assigning each segment a "segment number."
 *       This is done as a doubly linked list, which each process tracks locally. This tracking
 *       may be asynchronous since the shared linked list is updated behind a lock.
 */

/**
 * Gets the pointer to the segment metadata through struct access.
 *
 * @param seg_node The segment node.
 * @return The pointer to the segment metadata.
 */
#define msh_GetSegmentMetadata(seg_node) (msh_GetSegmentInfo(seg_node)->metadata)


/**
 * Gets the segment data (the first shared variable header in the segment).
 *
 * @param seg_node The segment node.
 * @return The segment data.
 */
SharedVariableHeader_t* msh_GetSegmentData(SegmentNode_t* seg_node);


/**
 * Calculates the required segment size given the size of the data
 * by adding the aligned size of the metadata struct.
 *
 * @param data_size The size of the data to be stored.
 * @return The total size required for the segment.
 */
size_t msh_FindSegmentSize(size_t data_size);


/**
 * Creates a new shared memory segment and places information into a
 * segment node wrapper.
 *
 * @param data_size The size of the new segment.
 * @return The new segment node which holds information about the new segment.
 */
SegmentNode_t* msh_CreateSegment(size_t data_size);


/**
 * Opens a segment in shared memory based on the segment number and places
 * information into a segment node wrapper.
 *
 * @param seg_num The segment number of the shared memory segment (used by matshare to identify the memory segment).
 * @return The new segment node which holds information about the new segment.
 */
SegmentNode_t* msh_OpenSegment(msh_segmentnumber_t seg_num);


/**
 * Closes the shared memory handle and unmaps pointers to shared memory. Also
 * destroys the segment node.
 *
 * @param seg_node The segment node containing the segment to detach.
 */
void msh_DetachSegment(SegmentNode_t* seg_node);


/**
 * Appends the segment to the end of the shared linked list. Does
 * this behind a lock and fetches required segments to do so.
 *
 * @param seg_node The segment node containing the segment to append.
 */
void msh_AddSegmentToSharedList(SegmentNode_t* seg_node);


/**
 * Removes the specified segment from the shared linked list. Does
 * this behind a lock and fetches required segments to do so.
 *
 * @param seg_node The segment node containing the segment to remove.
 */
void msh_RemoveSegmentFromSharedList(SegmentNode_t* seg_node);


/**
 * Detaches all segments in the specified segment list.
 *
 * @param seg_list The segment list for which all segments will be detached.
 */
void msh_DetachSegmentList(SegmentList_t* seg_list);


/**
 * Destroys all segments in shared memory and uses the
 * specified segment list as a cache to find segments that
 * have already been tracked by this process.
 *
 * @param seg_cache_list The segment list caching previously tracked variables.
 */
void msh_ClearSharedSegments(SegmentList_t* seg_cache_list);


/**
 * Updates the local tracking the shared linked list.
 *
 * @param seg_list The segment list to be updated.
 */
void msh_UpdateSegmentTracking(SegmentList_t* seg_list);


/**
 * Adds a segment node to the specified segment list.
 *
 * @param seg_list The segment list which the segment node will be appended to.
 * @param seg_node The segment node to be appended.
 */
void msh_AddSegmentToList(SegmentList_t* seg_list, SegmentNode_t* seg_node);


/**
 * Places the specified segment at the end of its parent linked list.
 *
 * @param seg_node The segment node to be placed at the end.
 */
void msh_PlaceSegmentAtEnd(SegmentNode_t* seg_node);


/**
 * Removes a segment node from its parent segment list.
 *
 * @param seg_node The segment to be removed.
 */
void msh_RemoveSegmentFromList(SegmentNode_t* seg_node);


/**
 * Initializes the segment hash table.
 *
 * @note allocates an array of size MSH_INIT_TABLE_SIZE.
 * @param seg_table A pointer to a segment hash table struct.
 */
void msh_InitializeTable(SegmentTable_t* seg_table);


/**
 * Adds the specified segment node to the hash table with
 * automatic resizing.
 *
 * @param seg_table The hash table to which the segment node will be added to.
 * @param seg_node The segment node to be added.
 */
void msh_AddSegmentToTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node);


/**
 * Removes the specified segment node from the hash table.
 *
 * @param seg_table The hash table from which the segment node will be removed.
 * @param seg_node The segment node to be removed.
 */
void msh_RemoveSegmentFromTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node);


/**
 * Frees and resets the table.
 *
 * @note Does not free the pointer that was passed.
 * @param seg_table The table to be reset.
 */
void msh_FreeTable(SegmentTable_t* seg_table);


/**
 * Attempts to find the segment node associated to
 * the segment number specified in the segment table.
 *
 * @param seg_table The segment table to be searched.
 * @param seg_num The segment number.
 * @return The segment node if found, otherwise NULL.
 */
SegmentNode_t* msh_FindSegmentNode(SegmentTable_t* seg_table, msh_segmentnumber_t seg_num);


/**
 * Detaches segments that have been marked as invalid in shared memory.
 *
 * @param seg_list The segment list to be cleaned.
 */
void msh_CleanSegmentList(SegmentList_t* seg_list, int gc_override);


/**
 * Updates the segment list placing the latest segment node
 * in shared memory at the end.
 *
 * @param seg_list The segment list to be updated.
 */
void msh_UpdateLatestSegment(SegmentList_t* seg_list);


/**
 * Creates the shared memory segment with the specified name.
 *
 * @param segment_name The name of the new segment.
 * @param segment_size The size of the new segment.
 * @return A handle to the newly created segment.
 */
handle_t msh_CreateSharedMemory(char_t* segment_name, size_t segment_size);


/**
 * Opens a previously created shared memory segment with the specified name.
 *
 * @param segment_name The name of the segment.
 * @return A handle to the segment.
 */
handle_t msh_OpenSharedMemory(char_t* segment_name);


/**
 * Creates a map to the segment specified by the handle with the
 * specified size.
 *
 * @param segment_handle A handle to the segment to be mapped.
 * @param map_sz The size of the map.
 * @return A pointer to the start of the map.
 */
void* msh_MapMemory(handle_t segment_handle, size_t map_sz);


/**
 * Unmaps the segment pointer with the specified size.
 *
 * @param segment_pointer The segment pointer to be unmapped.
 * @param map_sz The size of the map.
 */
void msh_UnmapMemory(void* segment_pointer, size_t map_sz);


/**
 * Closes the specified handle to a shared memory segment.
 *
 * @param segment_handle The shared memory handle to be closed.
 */
void msh_CloseSharedMemory(handle_t segment_handle);


/**
 * Locks the memory map into primary memory.
 *
 * @param ptr The map to be locked.
 * @param sz The size of the map.
 */
void msh_LockMemory(void* ptr, size_t sz);


/**
 * Unlocks the memory map previously locked into primary memory.
 *
 * @param ptr The map to be unlocked.
 * @param sz The size of the map.
 */
void msh_UnlockMemory(void* ptr, size_t sz);


/**
 * Forward declaration of the global segment list.
 */
extern SegmentList_t g_local_seg_list;

#endif /* MATSHARE_MSHSEGMENTS_H */
