/** mshexterntypes.h
 * Provides declarations for an mxArray emulation struct.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef MATSHARE_EXTERNTYPES_H
#define MATSHARE_EXTERNTYPES_H

/* forward declaration to avoid include */
typedef struct mxArray_tag mxArray;

/* opaque declaration */
typedef struct InternalMexStruct_t InternalMexStruct_t;

void met_SetName(mxArray* var, mxArray* name);

void met_SetCrosslink(mxArray* var, mxArray* crosslink);

void* met_GetName(mxArray* var);

/**
 * Fetches the crosslink for the given mxArray.
 *
 * @param var The mxArray for which to find the crosslink.
 * @return The crosslink.
 */
mxArray* met_GetCrosslink(mxArray* var);

int met_PutSharedCopy(const char* workspace_name, const char* variable_name, mxArray* shared_data_copy);


#endif /* MATSHARE_EXTERNTYPES_H */
