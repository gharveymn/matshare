/** matshare_.h
 * Declares top level functions and macros.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE__H
#define MATSHARE__H

#include "mshbasictypes.h"

/* forward declaration to avoid include */
typedef struct mxArray_tag mxArray;

#define MSH_DUPLICATE TRUE
#define MSH_SHARED_COPY FALSE

#define MSH_PARAM_THREADSAFETY "ThreadSafety"
#define MSH_PARAM_THREADSAFETY_L "threadsafety"
#define MSH_PARAM_THREADSAFETY_AB "ts"

#define MSH_PARAM_SECURITY "Security"
#define MSH_PARAM_SECURITY_L "security"
#define MSH_PARAM_SECURITY_AB "sc"

#define MSH_PARAM_SHARETYPE "Sharetype"
#define MSH_PARAM_SHARETYPE_L "sharetype"
#define MSH_PARAM_SHARETYPE_AB "st"

#define MSH_PARAM_GC "GarbageCollection"
#define MSH_PARAM_GC_L "garbagecollection"
#define MSH_PARAM_GC_AB "gc"

#define MSH_PARAM_MAX_VARIABLES "MaxVariables"
#define MSH_PARAM_MAX_VARIABLES_L "maxvariables"
#define MSH_PARAM_MAX_VARIABLES_AB "mv"

#define MSH_PARAM_MAX_SIZE "MaxSize"
#define MSH_PARAM_MAX_SIZE_L "maxsize"
#define MSH_PARAM_MAX_SIZE_AB "ms"

#ifdef MSH_WIN
#define MSH_PARAM_INFO \
"Current parameters for matshare: \n\
                                      ThreadSafety:      '%s'\n\
                                      ShareType:       '%s'\n\
                                      GarbageCollection: '%s'\n"
#else
#define MSH_PARAM_INFO \
"Current parameters for matshare: \n\
                                      ThreadSafety:      '%s'\n\
                                      ShareType:       '%s'\n\
                                      GarbageCollection: '%s'\n\
                                      Security:          '%o'\n"
#endif

typedef enum
{
	msh_SHARE          = 0,
	msh_FETCH          = 1,
	msh_DETACH         = 2,
	msh_CONFIG         = 3,
	msh_LOCALCOPY      = 4,
	msh_DEBUG          = 5,
	msh_CLEAR          = 6,
	msh_RESET          = 7
} msh_directive_t;

/**
 * Runs sharing operation. Makes a shared copy of the new variable if requested.
 *
 * @param nlhs The number of outputs. If this is 1 then the function will make a shared copy of the new variable.
 * @param plhs An array of output mxArrays
 * @param num_vars The number of variables to be shared.
 * @param in_vars The variables to be shared.
 */
void msh_Share(int nlhs, mxArray** plhs, int num_vars, const mxArray** in_vars);


/**
 * Fetches variables from shared memory. Either returns shared copies or duplicates to local memory.
 *
 * @param nlhs The number of ouputs. If this is 1 then it fetches the most recent variable; if 2 then all variables not already fetched; if 3 then all variables in shared memory.
 * @param plhs An array of output mxArrays.
 * @param will_duplicate A flag indicating whether to return local copies or shared copies.
 */
void msh_Fetch(int nlhs, mxArray** plhs, bool_t will_duplicate);


/**
 * Changes configuration parameters.
 *
 * @param num_params The number of parameters input.
 * @param in An array of the parameters and values.
 */
void msh_Config(int num_params, const mxArray** in);

/**
 * Clears shared memory segments. If there are no inputs then clears all shared memory segments.
 *
 * @param num_inputs The number of input variables.
 * @param in_vars The variables to be cleared from shared memory.
 */
void msh_Clear(int num_inputs, const mxArray** in_vars);

#endif /* MATSHARE__H */
