/** matshare_.h
 * Declares top level functions and macros.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef MATSHARE__H
#define MATSHARE__H

#include "mshbasictypes.h"


/* forward declaration to avoid include */
typedef struct mxArray_tag mxArray;

#define MSHSHARE_RETURN_NAME "shared"
#define MSHFETCH_RETURN_NAME "latest"

#define MSH_PARAM_THREADSAFETY "ThreadSafety"
#define MSH_PARAM_THREADSAFETY_L "threadsafety"
#define MSH_PARAM_THREADSAFETY_AB "ts"

#define MSH_PARAM_MAX_VARIABLES "MaxVariables"
#define MSH_PARAM_MAX_VARIABLES_L "maxvariables"
#define MSH_PARAM_MAX_VARIABLES_AB "mv"

#define MSH_PARAM_MAX_SIZE "MaxSize"
#define MSH_PARAM_MAX_SIZE_L "maxsize"
#define MSH_PARAM_MAX_SIZE_AB "ms"

#define MSH_PARAM_GC "GarbageCollection"
#define MSH_PARAM_GC_L "garbagecollection"
#define MSH_PARAM_GC_AB "gc"

#define MSH_PARAM_SECURITY "Security"
#define MSH_PARAM_SECURITY_L "security"
#define MSH_PARAM_SECURITY_AB "sc"

#define MSH_CONFIG_STRING_FORMAT \
"Current matshare configuration: \n\
                                     Current lock count:  %lu\n\
                                     Thread safety:       '%s'\n\
                                     Max variables:       %lu\n\
                                     Max shared size:     "SIZE_FORMAT"\n\
                                     Garbage collection:  '%s'\n"
#ifdef MSH_UNIX
#define MSH_CONFIG_SECURITY_STRING_FORMAT "\
                                     Security:            '%o'\n"
#endif


#define MSH_DEBUG_LOCAL_FORMAT \
"g_local_info: \n" \
"     rev_num: "SIZE_FORMAT"\n" \
"     lock_level: %lu\n" \
"     this_pid: "PID_FORMAT"\n" \
"     shared_info_wrapper (struct): \n" \
"          ptr: "SIZE_FORMAT"\n" \
"          handle: "HANDLE_FORMAT"\n" \
"     process_lock: "HANDLE_FORMAT"\n" \
"     is_initialized: %u\n" \
"     is_deinitialized: %u\n"

#define MSH_DEBUG_LOCAL_ARGS \
g_local_info.rev_num, \
g_local_info.lock_level, \
g_local_info.this_pid, \
g_local_info.shared_info_wrapper.ptr, \
g_local_info.shared_info_wrapper.handle, \
g_local_info.process_lock, \
g_local_info.is_initialized, \
g_local_info.is_deinitialized

#ifdef MSH_WIN
#  define MSH_SECURITY_FORMAT \
""
#  define MSH_NUM_PROCS_FORMAT \
"     num_procs: %li\n"
#  define MSH_SECURITY_ARG
#  define MSH_NUM_PROCS_ARG g_shared_info->num_procs,
#else
#  define MSH_SECURITY_FORMAT \
"          security: %i\n"
#  define MSH_NUM_PROCS_FORMAT \
"     lock_counter (union): \n" \
"          span: %li" \
"          values (struct): \n" \
"               count: %lu \n" \
"               flag: %lu \n" \
"               post: %lu \n"
#  define MSH_SECURITY_ARG g_shared_info->user_defined.security,
#  define MSH_NUM_PROCS_ARG \
g_shared_info->num_procs.span, \
g_shared_info->num_procs.values.count, \
g_shared_info->num_procs.values.flag, \
g_shared_info->num_procs.values.post,
#endif

#define MSH_DEBUG_SHARED_FORMAT \
"g_shared_info: \n" \
"     rev_num: "SIZE_FORMAT"\n" \
"     total_shared_size: "SIZE_FORMAT"\n" \
"     user_defined (struct): \n" \
"          lock_counter (union): \n" \
"               span: %li\n" \
"               values (struct): \n" \
"                    count: %lu \n" \
"                    flag: %lu \n" \
"                    post: %lu \n" \
"          max_shared_segments: %lu\n" \
"          max_shared_size: "SIZE_FORMAT"\n" \
"          will_shared_gc: %lu\n" \
MSH_SECURITY_FORMAT\
"     first_seg_num: "MSH_SEG_NUM_FORMAT"\n" \
"     last_seg_num: "MSH_SEG_NUM_FORMAT"\n" \
"     num_shared_segments: %lu\n" \
"     has_fatal_error: %lu\n" \
"     is_initialized: %lu\n" \
MSH_NUM_PROCS_FORMAT\
"     update_pid: "PID_FORMAT" \n"

#define MSH_DEBUG_SHARED_ARGS \
g_shared_info->rev_num, \
g_shared_info->total_shared_size, \
g_shared_info->user_defined.lock_counter.span, \
g_shared_info->user_defined.lock_counter.values.count, \
g_shared_info->user_defined.lock_counter.values.flag, \
g_shared_info->user_defined.lock_counter.values.post, \
g_shared_info->user_defined.max_shared_segments, \
g_shared_info->user_defined.max_shared_size, \
g_shared_info->user_defined.will_shared_gc, \
MSH_SECURITY_ARG \
g_shared_info->first_seg_num, \
g_shared_info->last_seg_num, \
g_shared_info->num_shared_segments, \
g_shared_info->has_fatal_error, \
g_shared_info->is_initialized, \
MSH_NUM_PROCS_ARG \
g_shared_info->update_pid

typedef enum
{
	msh_SHARE           = 0x0000,  /* share a variable */
	msh_FETCH           = 0x0001,  /* fetch shared variables */
	msh_DETACH          = 0x0002,  /* detach this session from shared memory */
	msh_CONFIG          = 0x0003,  /* change the configuration */
	msh_LOCALCOPY       = 0x0004,  /* create a local deepcopy of the variable */
	msh_DEBUG           = 0x0005,  /* print debug information */
	msh_CLEAR           = 0x0006,  /* clear segments from shared memory */
	msh_RESET           = 0x0007,  /* reset the configuration */
	msh_OVERWRITE       = 0x0008,  /* overwrite the specified variable in-place */
	msh_SAFEOVERWRITE   = 0x0009,  /* threadsafe overwrite of the variable */
	msh_LOCK            = 0x000A,  /* acquire the matshare interprocess lock */
	msh_UNLOCK          = 0x000B,  /* release the interprocess lock */
	msh_CLEAN           = 0x000C,  /* clean invalid and unused segments */
	msh_PERSISTSHARE    = 0x000D   /* share a variable persistently */
} msh_directive_t;


/**
 * Runs sharing operation. Makes a shared copy of the new variable if requested.
 *
 * @param nlhs The number of outputs. If this is 1 then the function will make a shared copy of the new variable.
 * @param plhs An array of output mxArrays
 * @param num_vars The number of variables to be shared.
 * @param in_vars The variables to be shared.
 */
void msh_Share(int num_vars, const mxArray** in_vars, int return_expected, msh_directive_t directive);


/**
 * Fetches variables from shared memory as shared data copies.
 *
 * @note this function will set a maximum of two cell arrays, and put the latest variable directly into the workspace
 * @param nlhs The number of ouputs. If this is 0 then it fetches the most recent variable; if 1 then all variables not already fetched; if 2 then all variables in shared memory.
 * @param plhs An array of output mxArrays.
 */
void msh_Fetch(int nlhs, mxArray** plhs);


/**
 * Fetches variables from shared memory as local deep copies.
 *
 * @param nlhs The number of ouputs. If this is 1 then it fetches the most recent variable; if 2 then all variables not already fetched; if 3 then all variables in shared memory.
 * @param plhs An array of output mxArrays.
 */
void msh_LocalCopy(int nlhs, mxArray** plhs);


/**
 * Clears shared memory segments. If there are no inputs then clears all shared memory segments.
 *
 * @param num_inputs The number of input variables.
 * @param in_vars The variables to be cleared from shared memory.
 */
void msh_Clear(int num_inputs, const mxArray** in_vars);


/**
 * Overwrite a variable in-place. Does this in a safe or unsafe manner
 * depending on the directive.
 *
 * @param dest_var The destination variable.
 * @param in_var The input variable.
 * @param directive The input directive.
 */
void msh_Overwrite(const mxArray* dest_var, const mxArray* in_var, msh_directive_t directive);


/**
 * Changes configuration parameters.
 *
 * @param num_params The number of parameters input.
 * @param in_params An array of the parameters and values.
 */
void msh_Config(int num_params, const mxArray** in_params);

#endif /* MATSHARE__H */
