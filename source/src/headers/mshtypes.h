/** mshtypes.h
 * Defines structs for process local information as well as
 * shared information.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_MSH_TYPES_H
#define MATSHARE_MSH_TYPES_H

#include "mshbasictypes.h"

/* differentiate these in case we have both running at the same time for some reason */
#if MSH_BITNESS == 64
#  define MSH_SHARED_INFO_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT"
#  define MSH_SEGMENT_NAME_FORMAT "/MSH_SEGMENT%0lx"
#  define MSH_CONFIG_FOLDER_NAME "matshare"
#  define MSH_CONFIG_FILE_NAME "mshconfig"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME "/MSH_LOCK"
#  else
#    define HOME_CONFIG_FOLDER ".config"
#  endif
#elif MSH_BITNESS == 32
#  define MSH_SHARED_INFO_SEGMENT_NAME "/MSH_SHARED_INFO_SEGMENT32"
#  define MSH_SEGMENT_NAME_FORMAT "/MSH_32SEGMENT%0lx"
#  define MSH_CONFIG_FOLDER_NAME "matshare"
#  define MSH_ "matshare"
#  define MSH_CONFIG_FILE_NAME "mshconfig32"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME "/MSH_LOCK32"
#  else
#    define HOME_CONFIG_FOLDER ".config"
#  endif
#endif

#define MSH_INITIAL_STATE 0

typedef struct UserConfig_t
{
	/* these are aligned for lockless assignment */
	size_t max_shared_size;
	volatile LockFreeCounter_t lock_counter; /* unused, kept for compatibility */
	unsigned long max_shared_segments;
	alignedbool_t will_shared_gc;
#ifdef MSH_UNIX
	mode_t security;
#endif
	/* this is modified behind a lock */
	char_t fetch_default[MSH_NAME_LEN_MAX];
} UserConfig_t;

/* structure of shared info about the shared segments */
typedef volatile struct SharedInfo_t
{
	size_t rev_num;
	size_t total_shared_size;
	UserConfig_t user_defined;
	segmentnumber_t first_seg_num;     /* the first segment number in the list */
	segmentnumber_t last_seg_num;      /* the last segment number in the list */
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


typedef struct LocalInfo_t
{
	size_t rev_num;
	uint32_T lock_level;
	pid_t this_pid;
	
	struct shared_info_wrapper_tag
	{
		SharedInfo_t* ptr;
		handle_t handle;
	} shared_info_wrapper;
	
	ProcessLock_t process_lock;
	
	bool_t has_fatal_error;
	bool_t is_initialized;
	bool_t is_deinitialized;
} LocalInfo_t;

/**
 * Forward declaration of the library name.
 */
extern char_t* g_msh_library_name;

/**
 * Forward declaration of the error help message.
 */
extern char_t* g_msh_error_help_message;


/**
 * Forward declaration of the warning help message.
 */
extern char_t* g_msh_warning_help_message;

/**
 * Forward declaration of the global information struct.
 */
extern LocalInfo_t g_local_info;

#define g_shared_info (g_local_info.shared_info_wrapper.ptr)

#define g_process_lock (g_local_info.process_lock)

#define g_user_config (g_shared_info->user_defined)

#endif /* MATSHARE_MSH_TYPES_H */
