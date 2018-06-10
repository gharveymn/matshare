#ifndef MATSHARE__H
#define MATSHARE__H

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"
#include "mshvariables.h"
#include "mshsegments.h"
#include "mshinit.h"

#define MSH_DUPLICATE TRUE
#define MSH_SHARED_COPY FALSE

#define MSH_PARAM_THREADSAFETY "ThreadSafety"
#define MSH_PARAM_THREADSAFETY_L "threadsafety"

#define MSH_PARAM_SECURITY "Security"
#define MSH_PARAM_SECURITY_L "security"

#define MSH_PARAM_SHARETYPE "Sharetype"
#define MSH_PARAM_SHARETYPE_L "sharetype"

#define MSH_PARAM_GC "GC"
#define MSH_PARAM_GC_L "gc"

#define MSH_PARAM_MAX_VARIABLES "MaxVariables"
#define MSH_PARAM_MAX_VARIABLES_L "maxvariables"

#define MSH_PARAM_MAX_SIZE "MaxSize"
#define MSH_PARAM_MAX_SIZE_L "maxsize"

#ifdef MSH_WIN
#define MSH_PARAM_INFO \
"Current parameters for matshare: \n\
                                      ThreadSafety: '%s'\n\
                                      CopyOnWrite:  '%s'\n\
                                      GC:           '%s'\n"
#else
#define MSH_PARAM_INFO \
"Current parameters for matshare: \n\
                                      ThreadSafety: '%s'\n\
                                      CopyOnWrite:  '%s'\n\
                                      GC:           '%s'\n\
                                      Security:     '%o'\n"
#endif

typedef enum
{
	msh_SHARE          = 0,
	msh_FETCH          = 1,
	msh_DETACH         = 2,
	msh_PARAM          = 3,
	msh_DEEPCOPY       = 4,
	msh_DEBUG          = 5,
	msh_OBJ_REGISTER   = 6,
	msh_OBJ_DEREGISTER = 7,
	msh_CLEAR          = 8,
	msh_RESET          = 9
} msh_directive_t;

void msh_Fetch(int nlhs, mxArray** plhs, bool_t will_duplicate);

void msh_Share(int nlhs, mxArray** plhs, int num_vars, const mxArray** in_vars);

void msh_Clear(int num_inputs, const mxArray** in_vars);

void msh_Config(int num_params, const mxArray** in);

msh_directive_t msh_ParseDirective(const mxArray* in);

#endif /* MATSHARE__H */
