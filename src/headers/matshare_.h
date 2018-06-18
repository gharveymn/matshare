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

#define MSH_PARAM_SHARETYPE "Sharetype"
#define MSH_PARAM_SHARETYPE_L "sharetype"
#define MSH_PARAM_SHARETYPE_AB "st"

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
	msh_SHARE         = 0x00,
	msh_FETCH         = 0x01,
	msh_DETACH        = 0x02,
	msh_CONFIG        = 0x03,
	msh_LOCALCOPY     = 0x04,
	msh_DEBUG         = 0x05,
	msh_CLEAR         = 0x06,
	msh_RESET         = 0x07,
	msh_OVERWRITE     = 0x08,
	msh_SAFEOVERWRITE = 0x09,
	msh_LOCK          = 0x0A,
	msh_UNLOCK        = 0x0B
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
 * @param directive A flag indicating whether to return local copies or shared copies.
 */
void msh_Fetch(int nlhs, mxArray** plhs, msh_directive_t directive);


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
 * @param in An array of the parameters and values.
 */
void msh_Config(int num_params, const mxArray** in);

#endif /* MATSHARE__H */
