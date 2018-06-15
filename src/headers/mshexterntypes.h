/** mshexterntypes.h
 * Provides declarations for an mxArray emulation struct.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_EXTERNTYPES_H
#define MATSHARE_EXTERNTYPES_H

/* forward declaration to avoid include */
typedef struct mxArray_tag mxArray;

/* Credit to Yair M. Altman */
typedef struct InternalMexStruct_t InternalMexStruct_t;

/**
 * Fetches the crosslink for the given mxArray.
 *
 * @param var The mxArray for which to find the crosslink.
 * @return The crosslink.
 */
mxArray* msh_GetCrosslink(mxArray* var);

#endif /* MATSHARE_EXTERNTYPES_H */
