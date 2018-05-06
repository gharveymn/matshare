#ifndef MATSHARE__H
#define MATSHARE__H

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"
#include "mshlists.h"
#include "mshinit.h"

#define MSH_DUPLICATE TRUE
#define MSH_SHARED_COPY FALSE

void msh_Fetch(int nlhs, mxArray** plhs, bool_t will_duplicate);

void msh_Share(int nlhs, mxArray** plhs, int num_vars, const mxArray** in_vars);

void msh_Clear(int num_inputs, const mxArray** in_vars);

void msh_Param(int num_params, const mxArray** in);



#endif /* MATSHARE__H */
