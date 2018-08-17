/** mshtable.c
 * Defines functions for handling segment hash tables.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#include "mex.h"

#include <string.h>

#include "mlerrorutils.h"
#include "mshsegments.h"
#include "mshtable.h"

#define MSH_INIT_TABLE_SIZE 64

/**
 * Resizes the segment table.
 *
 * @param seg_table The segment table.
 * @param num_segs The number of segments hooked into the table.
 */
static void msh_ResizeTable(SegmentTable_t* seg_table, uint32_T num_segs);


static void msh_AddNodeToTable(SegmentTable_t* seg_table, SegmentTableNode_t* new_table_node);


/**
 * Calculates the new hash table size.
 *
 * @param curr_sz The current table size.
 * @return The new table size.
 */
static uint32_T msh_FindNewTableSize(uint32_T curr_sz);


void msh_InitializeTable(SegmentTable_t* seg_table)
{
	seg_table->table_sz = MSH_INIT_TABLE_SIZE;
	seg_table->table = mxCalloc(seg_table->table_sz, sizeof(SegmentTableNode_t*));
	mexMakeMemoryPersistent(seg_table->table);
}


void msh_AddSegmentToTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node, void* key)
{
	SegmentTableNode_t* new_table_node;
	
	/* table must be initialized; don't want spooky initialization here */
	
	/* create new table node */
	new_table_node = mxMalloc(sizeof(SegmentTableNode_t));
	mexMakeMemoryPersistent(new_table_node);
	new_table_node->seg_node = seg_node;
	new_table_node->key = key;
	new_table_node->next = NULL;
	
	if(msh_GetSegmentList(seg_node)->num_segs >= seg_table->table_sz/2)
	{
		msh_ResizeTable(seg_table, msh_GetSegmentList(seg_node)->num_segs);
	}
	
	msh_AddNodeToTable(seg_table, new_table_node);
}


void msh_RemoveSegmentFromTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node, void* key)
{
	
	SegmentTableNode_t* curr_table_node,* removed_table_node = NULL;
	uint32_T removed_seg_hash;
	
	/* find the segment node immediately preceding this segment node in the table */
	removed_seg_hash = seg_table->get_hash(seg_table, key);
	if((curr_table_node = seg_table->table[removed_seg_hash]) == NULL)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL | MEU_SEVERITY_FATAL, "TableLogicError", "There was a failure in internal table logic (tried to remove a segment which wasn't in the table).");
	}
	else if(curr_table_node->seg_node == seg_node)
	{
		removed_table_node = seg_table->table[removed_seg_hash];
		seg_table->table[removed_seg_hash] = removed_table_node->next;
	}
	else
	{
		/* find the segment node immediately preceding this segment node in the table */
		while(curr_table_node->next->seg_node != seg_node)
		{
			curr_table_node = curr_table_node->next;
			if(curr_table_node->next == NULL)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL | MEU_SEVERITY_FATAL, "TableLogicError", "There was a failure in internal table logic (tried to remove a segment which wasn't in the table).");
			}
		}
		removed_table_node = curr_table_node->next;
		curr_table_node->next = removed_table_node->next;
	}
	
	mxFree(removed_table_node);
	
}


void msh_DestroyTable(SegmentTable_t* seg_table)
{
	SegmentTableNode_t* curr_table_node,* next_table_node;
	uint32_T curr_seg_hash;
	if(seg_table->table != NULL)
	{
		for(curr_seg_hash = 0; curr_seg_hash < seg_table->table_sz; curr_seg_hash++)
		{
			for(curr_table_node = seg_table->table[curr_seg_hash]; curr_table_node != NULL; curr_table_node = next_table_node)
			{
				next_table_node = curr_table_node->next;
				mxFree(curr_table_node);
			}
			seg_table->table[curr_seg_hash] = NULL;
		}
		mxFree(seg_table->table);
		seg_table->table = NULL;
		seg_table->table_sz = 0;
	}
}


SegmentNode_t* msh_FindSegmentNode(SegmentTable_t* seg_table, void* key)
{
	SegmentTableNode_t* curr_table_node;
	if(seg_table->table != NULL)
	{
		for(curr_table_node = seg_table->table[seg_table->get_hash(seg_table, key)]; curr_table_node != NULL; curr_table_node = curr_table_node->next)
		{
			if(seg_table->compare_keys(curr_table_node->key, key))
			{
				return curr_table_node->seg_node;
			}
		}
	}
	return NULL;
}


void msh_FindAllSegmentNodes(SegmentTable_t* seg_table, SegmentList_t* seg_list_cache, void* key)
{
	SegmentNode_t* seg_node_copy;
	SegmentTableNode_t* curr_table_node;
	memset(seg_list_cache, 0, sizeof(SegmentList_t));
	if(seg_table->table != NULL)
	{
		for(curr_table_node = seg_table->table[seg_table->get_hash(seg_table, key)]; curr_table_node != NULL; curr_table_node = curr_table_node->next)
		{
			if(seg_table->compare_keys(curr_table_node->key, key))
			{
				seg_node_copy = msh_CreateSegmentNode(msh_GetSegmentInfo(curr_table_node->seg_node));
				msh_AddSegmentToList(seg_list_cache, seg_node_copy);
				msh_SetVariableNode(seg_node_copy, msh_GetVariableNode(curr_table_node->seg_node));
			}
		}
	}
}


static void msh_ResizeTable(SegmentTable_t* seg_table, uint32_T num_segs)
{
	
	SegmentTableNode_t* curr_table_node,* next_table_node;
	SegmentTable_t new_table;
	uint32_T curr_seg_hash;
	
	if(seg_table->table_sz >= (uint32_T)(1 << 31))
	{
		/* dont do anything if we approach the max size for some reason */
		return;
	}
	
	/* copy any immutable members */
	new_table = *seg_table;
	new_table.table_sz = msh_FindNewTableSize(num_segs);
	new_table.table = mxCalloc(new_table.table_sz, sizeof(SegmentTableNode_t*));
	mexMakeMemoryPersistent(new_table.table);
	
	for(curr_seg_hash = 0; curr_seg_hash < seg_table->table_sz; curr_seg_hash++)
	{
		for(curr_table_node = seg_table->table[curr_seg_hash]; curr_table_node != NULL; curr_table_node = next_table_node)
		{
			next_table_node = curr_table_node->next;
			msh_AddNodeToTable(&new_table, curr_table_node);
		}
	}
	
	mxFree(seg_table->table);
	seg_table->table = new_table.table;
	seg_table->table_sz = new_table.table_sz;
	
}


static void msh_AddNodeToTable(SegmentTable_t* seg_table, SegmentTableNode_t* new_table_node)
{
	SegmentTableNode_t* curr_table_node;
	uint32_T seg_hash = seg_table->get_hash(seg_table, new_table_node->key);
	
	if(seg_table->table[seg_hash] == NULL)
	{
		seg_table->table[seg_hash] = new_table_node;
	}
	else
	{
		curr_table_node = seg_table->table[seg_hash];
		while(curr_table_node->next != NULL)
		{
			curr_table_node = curr_table_node->next;
		}
		curr_table_node->next = new_table_node;
	}

	/* make sure next is NULL in case we are resizing */
	new_table_node->next = NULL;
	
}


static uint32_T msh_FindNewTableSize(uint32_T curr_sz)
{
	if(curr_sz < MSH_INIT_TABLE_SIZE)
	{
		return MSH_INIT_TABLE_SIZE;
	}
	else
	{
		return curr_sz << 1;
	}
}
