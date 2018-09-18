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
struct mxArray_tag;

/* opaque declaration */
typedef struct InternalMexStruct_T InternalMexStruct_T;

/**
 * Fetches the crosslink for the given mxArray.
 *
 * @param var The mxArray for which to find the crosslink.
 * @return The crosslink.
 */
struct mxArray_tag* met_GetCrosslink(const struct mxArray_tag* var);


#endif /* MATSHARE_EXTERNTYPES_H */
