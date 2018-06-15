/** mshsegmentnode.h
 * Defines segment list, segment metadata, and segment table
 * structs. Declares functions for dealing with segment nodes.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 

#ifndef MATSHARE_MSHSEGMENTNODE_H
#define MATSHARE_MSHSEGMENTNODE_H

#include "mshbasictypes.h"

/* forward declaration, defined in source file */
typedef struct SegmentNode_t SegmentNode_t;

/* forward declaration, defined in mshvariablenode.h */
typedef struct VariableNode_t VariableNode_t;

typedef struct SegmentMetadata_t
{
	/* use these to link together the memory segments */
	size_t data_size;							/* segment size---this won't change so it's not volatile */
	volatile alignedbool_t is_invalid;                    /* set to TRUE if this segment is to be freed by all processes */
	volatile msh_segmentnumber_t prev_seg_num;
	volatile msh_segmentnumber_t next_seg_num;
	volatile long procs_using;             /* number of processes using this variable */
	volatile LockFreeCounter_t procs_tracking;
} SegmentMetadata_t;

typedef struct SegmentInfo_t
{
	void* raw_ptr;
	SegmentMetadata_t* metadata;
	size_t total_segment_size;
	handle_t handle;
	msh_segmentnumber_t seg_num;
} SegmentInfo_t;

typedef struct SegmentTable_t
{
	SegmentNode_t** table;
	uint32_T table_sz;
} SegmentTable_t;

typedef struct SegmentList_t
{
	SegmentNode_t* first;
	SegmentNode_t* last;
	SegmentTable_t seg_table;
	uint32_T num_segs;
} SegmentList_t;


/**
 * Creates a segment node (which is a linked list wrapper for a shared segment).
 *
 * @param seg_info_cache The segment info struct used to create the segment.
 * @return The created segment node.
 */
SegmentNode_t* msh_CreateSegmentNode(SegmentInfo_t* seg_info_cache);


/**
 * Destroys the segment node.
 *
 * @param seg_node The segment node to be destroyed.
 */
void msh_DestroySegmentNode(SegmentNode_t* seg_node);


/**
 * Initializer for the segment info struct.
 *
 * @param seg_info The segment info struct to initialize.
 */
void msh_InitializeSegmentInfo(SegmentInfo_t* seg_info);


/**
 * Gets the parent segment list.
 *
 * @param seg_node The segment node.
 * @return The parent segment list.
 */
SegmentList_t* msh_GetSegmentList(SegmentNode_t* seg_node);


/**
 * Gets the next segment node.
 *
 * @param seg_node The segment node.
 * @return The next segment node.
 */
SegmentNode_t* msh_GetNextSegment(SegmentNode_t* seg_node);


/**
 * Gets the previous segment node.
 *
 * @param seg_node The segment node.
 * @return The previous segment node.
 */
SegmentNode_t* msh_GetPreviousSegment(SegmentNode_t* seg_node);


/**
 * Gets the next segment node in the hash table chain.
 *
 * @param seg_node The segment node.
 * @return The next segment node in the hash table chain.
 */
SegmentNode_t* msh_GetHashNext(SegmentNode_t* seg_node);


/**
 * Gets a pointer to the segment info.
 *
 * @note The segment info is a nested struct inside the segment node. Do not deallocate this.
 * @param seg_node The segment node.
 * @return A pointer to the segment info.
 */
SegmentInfo_t* msh_GetSegmentInfo(SegmentNode_t* seg_node);


/**
 * Gets the associated variable node.
 *
 * @param seg_node The segment node.
 * @return The variable node.
 */
VariableNode_t* msh_GetVariableNode(SegmentNode_t* seg_node);


/**
 * Sets the parent segment list for the segment node.
 *
 * @param seg_node The segment node.
 * @param seg_list The new parent segment list.
 */
void msh_SetSegmentList(SegmentNode_t* seg_node, SegmentList_t* seg_list);


/**
 * Sets the next segment node.
 *
 * @param seg_node The segment node.
 * @param next_seg_node The next segment node.
 */
void msh_SetNextSegment(SegmentNode_t* seg_node, SegmentNode_t* next_seg_node);


/**
 * Sets the previous segment node.
 *
 * @param seg_node The segment node.
 * @param previous_seg_node The previous segment node.
 */
void msh_SetPreviousSegment(SegmentNode_t* seg_node, SegmentNode_t* prev_seg_node);


/**
 * Sets the next segment node in the hash table.
 *
 * @param seg_node The segment node.
 * @param hash_next_seg_node The next segment node in the hash table.
 */
void msh_SetHashNext(SegmentNode_t* seg_node, SegmentNode_t* hash_next_seg_node);


/**
 * Sets the segment info for this segment node.
 *
 * @note Copies the value of the segment info struct to the segment node.
 * @param seg_node The segment node.
 * @param seg_info The segment info to be copied.
 */
void msh_SetSegmentInfo(SegmentNode_t* seg_node, SegmentInfo_t* seg_info);


/**
 * Sets the variable node for this segment node.
 *
 * @param seg_node The segment node.
 * @param var_node The variable node to be linked.
 */
void msh_SetVariableNode(SegmentNode_t* seg_node, VariableNode_t* var_node);

#endif /* MATSHARE_MSHSEGMENTNODE_H */
