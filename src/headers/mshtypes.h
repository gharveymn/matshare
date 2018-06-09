#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mex.h"
#include "mshexterntypes.h"
#include "mshheader.h"
#include "mshbasictypes.h"

extern mxArray* mxCreateSharedDataCopy(mxArray*);

#define MSH_MAX_NAME_LEN 64

/* differentiate these in case we have both running at the same time for some reason */
#if MSH_BITNESS==64
#  define MSH_SHARED_INFO_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT"
#  define MSH_SEGMENT_NAME "/MSH_SEGMENT%0lx"
#  define MSH_CONFIG_FOLDER_NAME "matshare"
#    define MSH_CONFIG_FILE_NAME "mshconfig"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME "/MSH_LOCK"
#  else
#    define HOME_CONFIG_FOLDER ".config"
#  endif
#elif MSH_BITNESS==32
#  define MSH_SHARED_INFO_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT32"
#  define MSH_SEGMENT_NAME "/MSH_32SEGMENT%0lx"
#  define MSH_CONFIG_FOLDER_NAME "matshare"
#    define MSH_CONFIG_FILE_NAME "mshconfig32"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME "/MSH_LOCK32"
#  else
#    define HOME_CONFIG_FOLDER ".config"
#  endif
#endif

#define msh_SHARETYPE_COPY 0
#define msh_SHARETYPE_OVERWRITE 1

typedef union LockFreeCounter_ut
{
	long span;
	struct values_tag
	{
		unsigned long count : 30;
		unsigned long flag : 1;
		unsigned long post : 1;
	} values;
} LockFreeCounter_t;

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

typedef struct UserConfig_t
{
	/* these are aligned for lockless assignment */
	size_t max_shared_size;
	volatile LockFreeCounter_t lock_counter;
	unsigned long max_shared_segments;
	msh_sharetype_t sharetype;
	alignedbool_t will_gc;
#ifdef MSH_UNIX
	mode_t security;
#endif
} UserConfig_t;

/* structure of shared info about the shared segments */
typedef volatile struct SharedInfo_t
{
	size_t rev_num;
	size_t total_shared_size;
	UserConfig_t user_defined;
	msh_segmentnumber_t first_seg_num;     /* the first segment number in the list */
	msh_segmentnumber_t last_seg_num;      /* the last segment number in the list */
	uint32_T num_shared_segments;
	alignedbool_t has_fatal_error;
	alignedbool_t is_initialized;
#ifdef MSH_WIN
	long num_procs;
#else
	LockFreeCounter_t num_procs;
#endif
	pid_t update_pid;
} SharedInfo_t;


typedef struct GlobalInfo_t
{
	size_t rev_num;
	size_t num_registered_objs;
	uint32_T lock_level;
	pid_t this_pid;
	
	struct shared_info_wrapper_tag
	{
		SharedInfo_t* ptr;
		handle_t handle;
	} shared_info_wrapper;

#ifdef MSH_WIN
	handle_t process_lock;
#endif
	
	bool_t is_mex_locked;
	bool_t is_initialized;
	
} GlobalInfo_t;


extern GlobalInfo_t g_local_info;
extern VariableList_t g_local_var_list;
extern SegmentList_t g_local_seg_list;

#define g_shared_info (g_local_info.shared_info_wrapper.ptr)

#ifdef MSH_WIN
#  define g_process_lock (g_local_info.process_lock)
#else
#  define g_process_lock (g_local_info.shared_info_wrapper.handle)
#endif

#define msh_IsUpdated() (g_local_info.rev_num == g_shared_info->rev_num)

#endif /* MATSHARE_MSH_TYPES_H */
