#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mex.h"
#include "mshexterntypes.h"
#include "mshheader.h"
#include "mshbasictypes.h"

extern mxArray* mxCreateSharedDataCopy(mxArray*);

#define MSH_MAX_NAME_LEN 64
#define MSH_SHARED_INFO_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT"
#define MSH_LOCK_NAME "/MSH_LOCK"
#define MSH_SEGMENT_NAME "/MSH_SEGMENT%0lx"

#define msh_SHARETYPE_COPY 0
#define msh_SHARETYPE_OVERWRITE 1

typedef struct SegmentMetadata_t
{
	/* use these to link together the memory segments */
	size_t seg_sz;							/* segment size---this won't change so it's not volatile */
	volatile alignedbool_t is_used;                    /* ensures that this memory segment has been referenced at least once before removing */
	volatile alignedbool_t is_invalid;                    /* set to TRUE if this segment is to be freed by all processes */
	volatile msh_segmentnumber_t prev_seg_num;
	volatile msh_segmentnumber_t next_seg_num;
	volatile long procs_using;             /* number of processes using this variable */
	volatile long procs_tracking;          /* number of processes tracking this memory segment */
} SegmentMetadata_t;

typedef struct SegmentInfo_t
{
	volatile void* shared_memory_ptr;
	size_t seg_sz;
	handle_t handle;
	msh_segmentnumber_t seg_num;
	alignedbool_t is_init;
	alignedbool_t is_mapped;
	alignedbool_t is_local_invalid;
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
typedef volatile struct SharedInfo_t
{
	size_t rev_num;
	struct user_def_tag
	{
		/* these are aligned for lockless assignment */
		msh_sharetype_t sharetype;
		alignedbool_t is_thread_safe;
		alignedbool_t will_gc;
	} user_def;
	uint32_T num_valid_segments;
	msh_segmentnumber_t first_seg_num;     /* the first segment number in the list */
	msh_segmentnumber_t last_seg_num;      /* the last segment number in the list */
	pid_t update_pid;
#ifdef MSH_UNIX
	mode_t security;
#endif
	long num_procs;
} SharedInfo_t;


typedef struct GlobalInfo_t
{
	struct VariableList_t var_list;
	struct SegmentList_t seg_list;
	SegmentInfo_t shm_info_seg;
	
	size_t rev_num;
	size_t num_registered_objs;
	
#ifdef MSH_THREAD_SAFE
#  ifdef MSH_WIN
	SECURITY_ATTRIBUTES lock_sec;
#  endif
	handle_t proc_lock;
#endif
	
	pid_t this_pid;
	
	struct flags_tag
	{
		alignedbool_t is_proc_lock_init;
		alignedbool_t proc_lock_level;
	} flags;
	
} GlobalInfo_t;


extern GlobalInfo_t* g_local_info;
#define g_local_var_list (g_local_info->var_list)
#define g_local_seg_list (g_local_info->seg_list)
#define g_shared_info ((SharedInfo_t*)g_local_info->shm_info_seg.shared_memory_ptr)

#define MshIsUpdated() (g_local_info->rev_num == g_shared_info->rev_num)

#endif /* MATSHARE_MSH_TYPES_H */
