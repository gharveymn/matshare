#ifndef MATSHARE_EXTERNTYPES_H
#define MATSHARE_EXTERNTYPES_H

#include "mex.h"

/* Credit to Yair M. Altman */
typedef struct InternalMexStruct_t InternalMexStruct_t;

mxArray* msh_GetCrosslink(mxArray* var);

#endif /* MATSHARE_EXTERNTYPES_H */
