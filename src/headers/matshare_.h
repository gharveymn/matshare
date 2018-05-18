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

void msh_Fetch(int nlhs, mxArray** plhs, bool_t will_duplicate);

void msh_Share(int nlhs, mxArray** plhs, int num_vars, const mxArray** in_vars);

void msh_Clear(int num_inputs, const mxArray** in_vars);

void msh_Param(int num_params, const mxArray** in);



#endif /* MATSHARE__H */
