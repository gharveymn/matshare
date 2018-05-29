#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mex.h"
#include "mshexterntypes.h"
#include "mshheader.h"
#include "mshbasictypes.h"

extern mxArray* mxCreateSharedDataCopy(mxArray*);

#define MSH_MAX_NAME_LEN 64
#define MSH_SHARED_INFO_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT"
#define MSH_SEGMENT_NAME "/MSH_SEGMENT%0lx"

#define msh_SHARETYPE_COPY 0
#define msh_SHARETYPE_OVERWRITE 1

typedef struct SegmentMetadata_t
{
	/* use these to link together the memory segments */
	size_t data_sz;							/* segment size---this won't change so it's not volatile */
	volatile alignedbool_t is_invalid;                    /* set to TRUE if this segment is to be freed by all processes */
	volatile msh_segmentnumber_t prev_seg_num;
	volatile msh_segmentnumber_t next_seg_num;
	volatile long procs_using;             /* number of processes using this variable */
	volatile long procs_tracking;          /* number of processes tracking this memory segment */
} SegmentMetadata_t;

typedef struct SegmentInfo_t
{
	void* raw_ptr;
	SegmentMetadata_t* metadata;
	size_t total_segment_siz;
	handle_t handle;
	msh_segmentnumber_t seg_num;
} SegmentInfo_t;

typedef struct VariableNode_t VariableNode_t;
typedef struct SegmentNode_t SegmentNode_t;
typedef struct SegmentTable_t SegmentTable_t;
typedef struct VariableList_t VariableList_t;
typedef struct SegmentList_t SegmentList_t;

struct VariableNode_t
{
	VariableList_t* parent_var_list;
	VariableNode_t* next; /* next variable that is currently fetched and being used */
	VariableNode_t* prev;
	mxArray* var;
	SegmentNode_t* seg_node;
};

struct SegmentNode_t
{
	SegmentList_t* parent_seg_list;
	SegmentNode_t* next;
	SegmentNode_t* prev;
	SegmentNode_t* hash_next;
	VariableNode_t* var_node;
	SegmentInfo_t seg_info;
};


struct VariableList_t
{
	VariableNode_t* first;
	VariableNode_t* last;
	uint32_T num_vars;
};

struct SegmentTable_t
{
	SegmentNode_t** table;
	uint32_T table_sz;
};

struct SegmentList_t
{
	SegmentNode_t* first;
	SegmentNode_t* last;
	SegmentTable_t seg_table;
	uint32_T num_segs;
};

/* structure of shared info about the shared segments */
typedef struct SharedInfo_t
{
	volatile size_t rev_num;
	volatile struct user_def_tag
	{
		/* these are aligned for lockless assignment */
		msh_sharetype_t sharetype;
		alignedbool_t is_thread_safe;
		alignedbool_t will_gc;
#ifdef MSH_UNIX
		mode_t security;
#endif
	} user_def;
	volatile msh_segmentnumber_t first_seg_num;     /* the first segment number in the list */
	volatile msh_segmentnumber_t last_seg_num;      /* the last segment number in the list */
	volatile uint32_T num_valid_segments;
	volatile long num_procs;
	volatile pid_t update_pid;
} SharedInfo_t;


typedef struct GlobalInfo_t
{
	struct VariableList_t var_list;
	struct SegmentList_t seg_list;
	
	size_t rev_num;
	size_t num_registered_objs;
	
	struct shared_info_tag
	{
		SharedInfo_t* ptr;
		handle_t handle;
		alignedbool_t has_detached;
#ifdef MSH_UNIX
		alignedbool_t will_unlink;
#endif
	} shared_info;
	
#ifdef MSH_THREAD_SAFE
#  ifdef MSH_WIN
	OVERLAPPED overlapped;      /* yeah I don't know either */
#  endif
	uint32_T lock_level;
#endif
	
	pid_t this_pid;
} GlobalInfo_t;


extern GlobalInfo_t* g_local_info;
#define g_local_var_list (g_local_info->var_list)
#define g_local_seg_list (g_local_info->seg_list)
#define g_shared_info (g_local_info->shared_info.ptr)
#define g_process_lock (g_local_info->shared_info.handle)

#define msh_IsUpdated() (g_local_info->rev_num == g_shared_info->rev_num)

#endif /* MATSHARE_MSH_TYPES_H */
