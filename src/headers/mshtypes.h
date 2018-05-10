#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mex.h"
#include "mshexterntypes.h"
#include "mshheader.h"
#include "mshbasictypes.h"

extern mxArray* mxCreateSharedDataCopy(mxArray*);

#define MSH_MAX_NAME_LEN 62
#define MSH_UPDATE_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT"
#define MSH_LOCK_NAME "/MSH_LOCK"
#define MSH_SEGMENT_NAME "/MSH_SEGMENT%0lx"

#ifdef MSH_WIN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <unistd.h>
#  include <semaphore.h>
#  include <pthread.h>
#  include <fcntl.h>
#endif

typedef enum
{
	msh_SHARETYPE_COPY = 0,               /* always create a new segment */
	msh_SHARETYPE_OVERWRITE = 1          /* reuse the same segment if the new variable is smaller than or the same size as the old one */
} msh_sharetype_t;

typedef struct SegmentMetadata_t
{
	/* use these to link together the memory segments */
	size_t seg_sz;
	unsigned int procs_using;          /* number of processes using this variable */
	unsigned int procs_tracking;          /* number of processes tracking this memory segment */
	msh_segmentnumber_t seg_num;
	msh_segmentnumber_t prev_seg_num;
	msh_segmentnumber_t next_seg_num;
	bool_t is_used;                    /* ensures that this memory segment has been referenced at least once before removing */
	bool_t is_invalid;                    /* set to TRUE if this segment is to be freed by all processes */
} SegmentMetadata_t;

typedef struct SegmentInfo_t
{
#ifdef MSH_WIN
	HANDLE handle;
#else
	handle_t handle;
#endif
	volatile void* shared_memory_ptr;
	size_t seg_sz;
	char_t name[MSH_MAX_NAME_LEN];
	bool_t is_init;
	bool_t is_mapped;
} SegmentInfo_t;

typedef struct VariableList_t
{
	struct VariableNode_t* first;
	struct VariableNode_t* last;
	size_t num_vars;
} VariableList_t;

typedef struct SegmentList_t
{
	struct SegmentNode_t* first;
	struct SegmentNode_t* last;
	size_t num_segs;
} SegmentList_t;

/* structure of shared info about the shared segments */
typedef struct SharedInfo_t
{
	size_t rev_num;
	size_t num_valid_segments;
	msh_segmentnumber_t last_seg_num;          /* caches the predicted next segment number */
	msh_segmentnumber_t first_seg_num;          /* the first segment number in the list */
	unsigned int num_procs;
#ifdef MSH_WIN
	DWORD update_pid;
#else
	pid_t update_pid;
	mode_t security;
#endif
	
	struct user_def_tag
	{
		msh_sharetype_t sharetype;
		bool_t is_thread_safe;
		bool_t will_gc;
	} user_def;
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
	HANDLE proc_lock;
#  else
	handle_t proc_lock;
#  endif
#endif

#ifdef MSH_WIN
	DWORD this_pid;
#else
	pid_t this_pid;
#endif
	
	struct flags_tag
	{
		bool_t is_proc_lock_init;
		bool_t is_proc_locked;
	} flags;
	
} GlobalInfo_t;


extern GlobalInfo_t* g_local_info;
#define g_local_var_list (g_local_info->var_list)
#define g_local_seg_list (g_local_info->seg_list)
#define g_shared_info ((SharedInfo_t*)g_local_info->shm_info_seg.shared_memory_ptr)

#define MshIsUpdated() (g_local_info->rev_num == g_shared_info->rev_num)

#endif /* MATSHARE_MSH_TYPES_H */
