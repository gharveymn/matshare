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

#define MSH_INITIAL_STATE 0
#define MSH_CONFIG_FOLDER_NAME "matshare"

/* differentiate these in case we have both running at the same time for some reason */
#if MSH_BITNESS == 64
#  define MSH_SHARED_INFO_SEGMENT_NAME   "/MSH_SHARED_INFO_SEGMENT"
#  define MSH_SEGMENT_NAME_FORMAT        "/MSH_SEGMENT%0lx"
#  define MSH_CONFIG_FILE_NAME           "mshconfig"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME                "/MSH_LOCK"
#    define MSH_SEGMENT_LOCK_NAME_FORMAT "/MSH_SEGMENT_LOCK%0lx"
#  else
#    define HOME_CONFIG_FOLDER           ".config"
#  endif
#elif MSH_BITNESS == 32
#    define MSH_SHARED_INFO_SEGMENT_NAME "/MSH32_SHARED_INFO_SEGMENT"
#    define MSH_SEGMENT_NAME_FORMAT      "/MSH32_SEGMENT%0lx"
#    define MSH_CONFIG_FILE_NAME         "mshconfig32"
#  ifdef MSH_WIN
#    define MSH_LOCK_NAME                "/MSH32_LOCK"
#    define MSH_SEGMENT_LOCK_NAME_FORMAT "/MSH32_SEGMENT_LOCK%0lx"
#  else
#    define HOME_CONFIG_FOLDER           ".config"
#  endif
#endif

typedef struct UserConfig_T
{
	/* these are aligned for lockless assignment */
	size_t max_shared_size;
	LockFreeCounter_T lock_counter; /* unused, kept for compatibility */
	unsigned long max_shared_segments;
	alignedbool_T will_shared_gc;
#ifdef MSH_UNIX
	mode_t security;
#endif
	/* this is modified behind a lock */
	char_T fetch_default[MSH_NAME_LEN_MAX];
	long varop_opts_default;
	long version;
} UserConfig_T;

/* structure of shared info about the shared segments */
typedef volatile struct SharedInfo_T
{
	size_t rev_num;
	size_t total_shared_size;
	UserConfig_T user_defined;
	segmentnumber_T first_seg_num;     /* the first segment number in the list */
	segmentnumber_T last_seg_num;      /* the last segment number in the list */
	uint32_T num_shared_segments;
	alignedbool_T has_fatal_error;
	alignedbool_T is_initialized;
#ifdef MSH_WIN
	long num_procs;
#else
	LockFreeCounter_T num_procs;
#endif
	pid_T update_pid;
} SharedInfo_T;


typedef struct LocalInfo_T
{
	size_t rev_num;
	uint32_T lock_level;
	pid_T this_pid;
	
	struct shared_info_wrapper_tag
	{
		SharedInfo_T* ptr;
		handle_T handle;
	} shared_info_wrapper;
	
	FileLock_T process_lock;
	
	bool_T has_fatal_error;
	bool_T is_initialized;
	bool_T is_deinitialized;
} LocalInfo_T;

/**
 * Forward declaration of the library name.
 */
extern char_T* g_msh_library_name;

/**
 * Forward declaration of the error help message.
 */
extern char_T* g_msh_error_help_message;


/**
 * Forward declaration of the warning help message.
 */
extern char_T* g_msh_warning_help_message;

/**
 * Forward declaration of the global information struct.
 */
extern LocalInfo_T g_local_info;

#define g_shared_info (g_local_info.shared_info_wrapper.ptr)

#define g_process_lock (g_local_info.process_lock)

#define g_user_config (g_shared_info->user_defined)

#endif /* MATSHARE_MSH_TYPES_H */
