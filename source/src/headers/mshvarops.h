/** mshvarops.h
 * Declares functions for variable operations and structs
 * for parsing of subscripts passed in by MATLAB.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef MATSHARE_MSHVAROPS_H
#define MATSHARE_MSHVAROPS_H

#include "mshtypes.h"

typedef struct ParsedIndices_t
{
	mwIndex*       start_idxs;
	size_t         num_idxs;
	mwSize*        slice_lens;
	size_t         num_lens;
} ParsedIndices_t;

typedef struct IndexedVariable_t
{
	const mxArray* dest_var;
	ParsedIndices_t indices;
} IndexedVariable_t;

void msh_ParseSubscriptStruct(IndexedVariable_t* indexed_var, const mxArray* in_var, const mxArray* substruct);

ParsedIndices_t msh_ParseIndices(mxArray* subs_arr, const mxArray* dest_var, const mxArray* in_var);

mwIndex msh_ParseSingleIndex(mxArray* subs_arr, const mxArray* dest_var, const mxArray* in_var);

/**
 * Compares the size of the two mxArrays. Returns TRUE if
 * they are the same size, returns FALSE otherwise.
 *
 * @param dest_var The first variable to be compared.
 * @param comp_var The second variable to be compared.
 * @return Whether they are equal in size.
 */
void msh_CompareVariableSize(IndexedVariable_t* indexed_var, const mxArray* comp_var);


/**
 * Overwrites the data in dest_var with the data in in_var.
 *
 * @param dest_var The destination variable.
 * @param in_var The input variable.
 */
void msh_OverwriteVariable(IndexedVariable_t* indexed_var, const mxArray* in_var, int will_sync);

#endif /* MATSHARE_MSHVAROPS_H */
