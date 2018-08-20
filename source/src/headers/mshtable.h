#ifndef MATSHARE_MSHTABLE_H
#define MATSHARE_MSHTABLE_H

#include "mshsegmentnode.h"


typedef struct SegmentTableNode_T
{
	struct SegmentTableNode_T* next;
	SegmentNode_T* seg_node;
	void* key;
} SegmentTableNode_T;

typedef struct SegmentTable_T
{
	SegmentTableNode_T** table;
	uint32_T table_sz;
	uint32_T (*get_hash)(struct SegmentTable_T*, void*);
	int (*compare_keys)(void*, void*);
} SegmentTable_T;


/**
 * Initializes the segment hash table.
 *
 * @note allocates an array of size MSH_INIT_TABLE_SIZE.
 * @param seg_table A pointer to a segment hash table struct.
 */
void msh_InitializeTable(SegmentTable_T* seg_table);


/**
 * Adds the specified segment node to the hash table with
 * automatic resizing.
 *
 * @param seg_table The hash table to which the segment node will be added to.
 * @param seg_node The segment node to be added.
 */
void msh_AddSegmentToTable(SegmentTable_T* seg_table, SegmentNode_T* seg_node, void* key);


/**
 * Attempts to find the segment node associated to
 * the segment number specified in the segment table.
 *
 * @param seg_table The segment table to be searched.
 * @param seg_num The segment number.
 * @return The segment node if found, otherwise NULL.
 */
SegmentNode_T* msh_FindSegmentNode(SegmentTable_T* seg_table, void* key);


void msh_FindAllSegmentNodes(SegmentTable_T* seg_table, SegmentList_T* seg_list_cache, void* key);

/**
 * Removes the specified segment node from the hash table.
 *
 * @param seg_table The hash table from which the segment node will be removed.
 * @param seg_node The segment node to be removed.
 */
void msh_RemoveSegmentFromTable(SegmentTable_T* seg_table, SegmentNode_T* seg_node, void* key);


/**
 * Frees and resets the table.
 *
 * @note Does not free the pointer that was passed.
 * @param seg_table The table to be reset.
 */
void msh_DestroyTable(SegmentTable_T* seg_table);

#endif /* MATSHARE_MSHTABLE_H */
