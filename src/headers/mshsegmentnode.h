#ifndef MATSHARE_MSHSEGMENTNODE_H
#define MATSHARE_MSHSEGMENTNODE_H

#include "mex.h"
#include "mshbasictypes.h"

typedef struct SegmentNode_t SegmentNode_t;

#include "mshvariablenode.h"

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

SegmentNode_t* msh_CreateSegmentNode(SegmentInfo_t* seg_info_cache);
void msh_DestroySegmentNode(SegmentNode_t* seg_node);
void msh_InitializeSegmentInfo(SegmentInfo_t* seg_info);

SegmentList_t* msh_GetSegmentList(SegmentNode_t* seg_node);
SegmentNode_t* msh_GetNextSegment(SegmentNode_t* seg_node);
SegmentNode_t* msh_GetPrevSegment(SegmentNode_t* seg_node);
SegmentNode_t* msh_GetHashNext(SegmentNode_t* seg_node);
SegmentInfo_t* msh_GetSegmentInfo(SegmentNode_t* seg_node);
VariableNode_t* msh_GetVariableNode(SegmentNode_t* seg_node);

void msh_SetSegmentList(SegmentNode_t* seg_node, SegmentList_t* seg_list);
void msh_SetNextSegment(SegmentNode_t* seg_node, SegmentNode_t* next_seg_node);
void msh_SetPrevSegment(SegmentNode_t* seg_node, SegmentNode_t* prev_seg_node);
void msh_SetHashNext(SegmentNode_t* seg_node, SegmentNode_t* hash_next_seg_node);
void msh_SetSegmentInfo(SegmentNode_t* seg_node, SegmentInfo_t* seg_info);
void msh_SetVariableNode(SegmentNode_t* seg_node, VariableNode_t* var_node);

#endif /* MATSHARE_MSHSEGMENTNODE_H */
