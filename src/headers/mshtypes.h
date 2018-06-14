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
#  define MSH_CONFIG_FILE_NAME "mshconfig"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME "/MSH_LOCK"
#  else
#    define HOME_CONFIG_FOLDER ".config"
#  endif
#elif MSH_BITNESS==32
#  define MSH_SHARED_INFO_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT32"
#  define MSH_SEGMENT_NAME "/MSH_32SEGMENT%0lx"
#  define MSH_CONFIG_FOLDER_NAME "matshare"
#  define MSH_CONFIG_FILE_NAME "mshconfig32"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME "/MSH_LOCK32"
#  else
#    define HOME_CONFIG_FOLDER ".config"
#  endif
#endif

#define msh_SHARETYPE_COPY 0
#define msh_SHARETYPE_OVERWRITE 1

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
#ifdef MSH_DEBUG_PERF
	struct debug_perf
	{
		size_t total_time;
		size_t lock_time;
		size_t busy_wait_time;
	} debug_perf;
#endif
	pid_t update_pid;
} SharedInfo_t;


typedef struct GlobalInfo_t
{
	size_t rev_num;
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

extern char_t* g_msh_library_name;
extern char_t* g_msh_error_help_message;
extern char_t* g_msh_warning_help_message;

extern GlobalInfo_t g_local_info;

#define g_shared_info (g_local_info.shared_info_wrapper.ptr)

#ifdef MSH_WIN
#  define g_process_lock (g_local_info.process_lock)
#else
#  define g_process_lock (g_local_info.shared_info_wrapper.handle)
#endif

#define msh_IsUpdated() (g_local_info.rev_num == g_shared_info->rev_num)

#endif /* MATSHARE_MSH_TYPES_H */
