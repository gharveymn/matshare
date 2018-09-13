/** mshsegmentnode.h
 * Defines segment list, segment metadata, and segment table
 * structs. Declares functions for dealing with segment nodes.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef MATSHARE_MSHSEGMENTNODE_H
#define MATSHARE_MSHSEGMENTNODE_H

#include "mshbasictypes.h"

/* forward declaration, defined in source file */
typedef struct SegmentNode_T SegmentNode_T;

/* forward declaration */
struct SegmentList_T;

/* forward declaration, defined in mshvariablenode.h */
struct VariableNode_T;

typedef struct SegmentMetadata_T
{
	/* use these to link together the memory segments */
	char_T name[MSH_NAME_LEN_MAX];             /* non-volatile */
	size_t data_size;						/* size without the metadata; non-volatile */
	volatile alignedbool_T is_persistent;        /* set to TRUE if the segment will not be automatically garbage collected */
	volatile alignedbool_T is_invalid;           /* set to TRUE if this segment is to be freed by all processes */
	volatile segmentnumber_T prev_seg_num;
	volatile segmentnumber_T next_seg_num;
	volatile long procs_using;                   /* number of processes using this variable */
	volatile LockFreeCounter_T procs_tracking;
} SegmentMetadata_T;

typedef struct SegmentInfo_T
{
	void*              raw_ptr;
	SegmentMetadata_T* metadata;
	size_t             total_segment_size;
	handle_T           handle;
	FileLock_T         lock;
	segmentnumber_T    seg_num;
} SegmentInfo_T;

#define msh_HasVariableName(seg_node) (msh_GetSegmentMetadata(seg_node)->name[0] != '\0')

/**
 * Creates a segment node (which is a linked list wrapper for a shared segment).
 *
 * @param seg_info_cache The segment info struct used to create the segment.
 * @return The created segment node.
 */
SegmentNode_T* msh_CreateSegmentNode(SegmentInfo_T* seg_info_cache);


/**
 * Destroys the segment node.
 *
 * @param seg_node The segment node to be destroyed.
 */
void msh_DestroySegmentNode(SegmentNode_T* seg_node);


/**
 * Gets the parent segment list.
 *
 * @param seg_node The segment node.
 * @return The parent segment list.
 */
struct SegmentList_T* msh_GetSegmentList(SegmentNode_T* seg_node);


/**
 * Gets the next segment node.
 *
 * @param seg_node The segment node.
 * @return The next segment node.
 */
SegmentNode_T* msh_GetNextSegment(SegmentNode_T* seg_node);


/**
 * Gets the previous segment node.
 *
 * @param seg_node The segment node.
 * @return The previous segment node.
 */
SegmentNode_T* msh_GetPreviousSegment(SegmentNode_T* seg_node);


/**
 * Gets a pointer to the segment info.
 *
 * @note The segment info is a nested struct inside the segment node. Do not deallocate this.
 * @param seg_node The segment node.
 * @return A pointer to the segment info.
 */
SegmentInfo_T* msh_GetSegmentInfo(SegmentNode_T* seg_node);


/**
 * Gets the associated variable node.
 *
 * @param seg_node The segment node.
 * @return The variable node.
 */
struct VariableNode_T* msh_GetVariableNode(SegmentNode_T* seg_node);


/**
 * Sets the parent segment list for the segment node.
 *
 * @param seg_node The segment node.
 * @param seg_list The new parent segment list.
 */
void msh_SetSegmentList(SegmentNode_T* seg_node, struct SegmentList_T* seg_list);


/**
 * Sets the next segment node.
 *
 * @param seg_node The segment node.
 * @param next_seg_node The next segment node.
 */
void msh_SetNextSegment(SegmentNode_T* seg_node, SegmentNode_T* next_seg_node);


/**
 * Sets the previous segment node.
 *
 * @param seg_node The segment node.
 * @param previous_seg_node The previous segment node.
 */
void msh_SetPreviousSegment(SegmentNode_T* seg_node, SegmentNode_T* prev_seg_node);


/**
 * Sets the segment info for this segment node.
 *
 * @note Copies the value of the segment info struct to the segment node.
 * @param seg_node The segment node.
 * @param seg_info The segment info to be copied.
 */
void msh_SetSegmentInfo(SegmentNode_T* seg_node, SegmentInfo_T* seg_info);


/**
 * Sets the variable node for this segment node.
 *
 * @param seg_node The segment node.
 * @param var_node The variable node to be linked.
 */
void msh_SetVariableNode(SegmentNode_T* seg_node, struct VariableNode_T* var_node);

#endif /* MATSHARE_MSHSEGMENTNODE_H */
