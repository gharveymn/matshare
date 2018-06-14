#include "../mshsegmentnode.h"
#include "../mlerrorutils.h"
#define MSH_INIT_TABLE_SIZE 64

static void msh_ResizeTable(SegmentTable_t* seg_table, uint32_T num_segs);
static uint32_T msh_FindNewTableSize(uint32_T curr_sz);
static uint32_T msh_FindSegmentHash(SegmentTable_t* seg_table, msh_segmentnumber_t seg_num);


void msh_InitializeTable(SegmentTable_t* seg_table)
{
	seg_table->table_sz = MSH_INIT_TABLE_SIZE;
	seg_table->table = mxCalloc(seg_table->table_sz, sizeof(SegmentNode_t*));
	mexMakeMemoryPersistent(seg_table->table);
}

void msh_AddSegmentToTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node, uint32_T num_segs)
{
	SegmentNode_t* curr_seg_node;
	uint32_T seg_hash;
	
	if(num_segs >= seg_table->table_sz)
	{
		msh_ResizeTable(seg_table, num_segs);
	}
	
	msh_SetHashNext(seg_node, NULL);
	
	seg_hash = msh_FindSegmentHash(seg_table, msh_GetSegmentInfo(seg_node)->seg_num);
	if(seg_table->table[seg_hash] == NULL)
	{
		seg_table->table[seg_hash] = seg_node;
	}
	else
	{
		curr_seg_node = seg_table->table[seg_hash];
		while(msh_GetHashNext(curr_seg_node) != NULL)
		{
			curr_seg_node = msh_GetHashNext(curr_seg_node);
		}
		msh_SetHashNext(curr_seg_node, seg_node);
	}
	
}

void msh_RemoveSegmentFromTable(SegmentTable_t* seg_table, SegmentNode_t* seg_node)
{
	
	SegmentNode_t* curr_seg_node;
	uint32_T seg_hash;
	
	/* find the segment node immediately preceding this segment node in the table */
	seg_hash = msh_FindSegmentHash(seg_table, msh_GetSegmentInfo(seg_node)->seg_num);
	if(seg_table->table[seg_hash] == seg_node)
	{
		seg_table->table[seg_hash] = msh_GetHashNext(seg_node);
	}
	else
	{
		/* find the segment node immediately preceding this segment node in the table */
		curr_seg_node = seg_table->table[seg_hash];
		while(msh_GetHashNext(curr_seg_node) != seg_node)
		{
			curr_seg_node = msh_GetHashNext(curr_seg_node);
		}
		msh_SetHashNext(curr_seg_node, msh_GetHashNext(seg_node));
	}
	
	msh_SetHashNext(seg_node, NULL);
	
}


void msh_DestroyTable(SegmentTable_t* seg_table)
{
	if(seg_table->table != NULL)
	{
		mxFree(seg_table->table);
		seg_table->table = NULL;
		seg_table->table_sz = 0;
	}
}


SegmentNode_t* msh_FindSegmentNode(SegmentTable_t* seg_table, msh_segmentnumber_t seg_num)
{
	SegmentNode_t* curr_seg_node;
	
	for(curr_seg_node = seg_table->table[msh_FindSegmentHash(seg_table, seg_num)]; curr_seg_node != NULL; curr_seg_node = msh_GetHashNext(curr_seg_node))
	{
		if(msh_GetSegmentInfo(curr_seg_node)->seg_num == seg_num)
		{
			return curr_seg_node;
		}
	}
	return NULL;
}


static void msh_ResizeTable(SegmentTable_t* seg_table, uint32_T num_segs)
{
	SegmentNode_t* curr_seg_node,* next_seg_node;
	SegmentTable_t new_table;
	uint32_T curr_seg_hash;
	
	if(seg_table->table_sz >= ((uint32_T)1 << 31))
	{
		/* dont do anything if we approach the max size for some reason */
		return;
	}
	
	new_table.table_sz = msh_FindNewTableSize(num_segs);
	new_table.table = mxCalloc(new_table.table_sz, sizeof(SegmentNode_t*));
	mexMakeMemoryPersistent(new_table.table);
	
	for(curr_seg_hash = 0; curr_seg_hash < seg_table->table_sz; curr_seg_hash += 1)
	{
		for(curr_seg_node = seg_table->table[curr_seg_hash]; curr_seg_node != NULL; curr_seg_node = next_seg_node)
		{
			next_seg_node = msh_GetHashNext(curr_seg_node);
			msh_AddSegmentToTable(&new_table, curr_seg_node, num_segs);
		}
	}
	
	
	msh_DestroyTable(seg_table);
	
	*seg_table = new_table;
	
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


static uint32_T msh_FindSegmentHash(SegmentTable_t* seg_table, msh_segmentnumber_t seg_num)
{
	return seg_num % seg_table->table_sz;
}
