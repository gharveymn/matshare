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

#ifndef INT8_MAX
#  define INT8_MAX  0x7F
#endif
#ifndef INT16_MAX
#  define INT16_MAX 0x7FFF
#endif
#ifndef INT32_MAX
#  define INT32_MAX 0x7FFFFFFF
#endif
#ifndef INT64_MAX
#  define INT64_MAX 0x7FFFFFFFFFFFFFFF
#endif
#ifndef INT8_MIN
#  define INT8_MIN  (-INT8_MAX-1)
#endif
#ifndef INT16_MIN
#  define INT16_MIN (-INT16_MAX-1)
#endif
#ifndef INT32_MIN
#  define INT32_MIN (-INT32_MAX-1)
#endif
#ifndef INT64_MIN
#  define INT64_MIN (-INT64_MAX-1)
#endif

#ifndef UINT8_MAX
#  define UINT8_MAX  0xFF
#endif
#ifndef UINT16_MAX
#  define UINT16_MAX 0xFFFF
#endif
#ifndef UINT32_MAX
#  define UINT32_MAX 0xFFFFFFFF
#endif
#ifndef UINT64_MAX
#  define UINT64_MAX 0xFFFFFFFFFFFFFFFF
#endif

typedef enum
{
	ABS = 0,
	ADD = 1,
	SUB = 2,
	MUL = 3,
	DIV = 4,
	REM = 5,
	MOD = 6,
	NEG = 7,
	SRA = 8,
	SLA = 9
} msh_varop_T;

typedef void (*binaryvaropfcn_T)(void*, void*, size_t, int);

typedef struct ParsedIndices_T
{
	mwIndex*       start_idxs;
	size_t         num_idxs;
	mwSize*        slice_lens;
	size_t         num_lens;
} ParsedIndices_T;

typedef struct IndexedVariable_T
{
	const mxArray* dest_var;
	ParsedIndices_T indices;
} IndexedVariable_T;

void msh_ParseSubscriptStruct(IndexedVariable_T* indexed_var, const mxArray* in_var, const mxArray* substruct);

ParsedIndices_T msh_ParseIndices(mxArray* subs_arr, const mxArray* dest_var, const mxArray* in_var);

mwIndex msh_ParseSingleIndex(mxArray* subs_arr, const mxArray* dest_var, const mxArray* in_var);

/**
 * Compares the size of the two mxArrays. Returns TRUE if
 * they are the same size, returns FALSE otherwise.
 *
 * @param dest_var The first variable to be compared.
 * @param comp_var The second variable to be compared.
 * @return Whether they are equal in size.
 */
void msh_CompareVariableSize(IndexedVariable_T* indexed_var, const mxArray* comp_var);


/**
 * Overwrites the data in dest_var with the data in in_var.
 *
 * @param dest_var The destination variable.
 * @param in_var The input variable.
 */
void msh_OverwriteVariable(IndexedVariable_T* indexed_var, const mxArray* in_var, int will_sync);

#endif /* MATSHARE_MSHVAROPS_H */
