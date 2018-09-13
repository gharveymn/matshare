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

#define MSH_IS_SYNCHRONOUS 0x0001
#define MSH_USE_ATOMIC_OPS 0x0002

typedef enum
{
	VAROP_ABS = 0x0000,
	VAROP_ADD = 0x0001,
	VAROP_SUB = 0x0002,
	VAROP_MUL = 0x0003,
	VAROP_DIV = 0x0004,
	VAROP_REM = 0x0005,
	VAROP_MOD = 0x0006,
	VAROP_NEG = 0x0007,
	VAROP_ARS = 0x0008,
	VAROP_ALS = 0x0009,
	VAROP_CPY = 0x000A
} msh_varop_T;

typedef struct WideInput_T
{
	union
	{
		void*     raw;
		int8_T*   Int8;
		int16_T*  Int16;
		int32_T*  Int32;
		uint8_T*  UInt8;
		uint16_T* UInt16;
		uint32_T* UInt32;
#if MSH_BITNESS==64
		int64_T*  Int64;
		uint64_T* UInt64;
#endif
		single* Single;
		double* Double;
		mwIndex*  Index;
		mwSize*   Size;
	} input;
	size_t num_elems;
	mxClassID mxtype;
} WideInput_T;

#define WideInputFetch_(S_, TYPEN_) S_->input.TYPEN_
#define WideInputFetch(S, TYPEN) WideInputFetch_(S, TYPEN)

typedef void (*unaryvaropfcn_T)(WideInput_T*,long);
typedef void (*binaryvaropfcn_T)(WideInput_T*,WideInput_T*,long);

typedef struct ParsedIndices_T
{
	const mxArray* subs_struct;
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

IndexedVariable_T msh_ParseSubscriptStruct(const mxArray* parent_var, const mxArray* subs_struct);

ParsedIndices_T msh_ParseIndices(mxArray* subs_arr, const mxArray* dest_var);

mwIndex msh_ParseSingleIndex(mxArray* subs_arr, const mxArray* dest_var);

void msh_CheckValidInput(IndexedVariable_T* indexed_var, const mxArray* in_var, msh_varop_T varop);

/**
 * Compares the size of the two mxArrays. Returns TRUE if
 * they are the same size, returns FALSE otherwise.
 *
 * @param dest_var The first variable to be compared.
 * @param in_var The second variable to be compared.
 * @return Whether they are equal in size.
 */
void msh_CompareVariableSize(IndexedVariable_T* indexed_var, const mxArray* in_var, msh_varop_T varop);


int msh_GetNumVarOpArgs(msh_varop_T varop);

void msh_UnaryVariableOperation(IndexedVariable_T* indexed_var, msh_varop_T varop, long opts, mxArray** output);
void msh_BinaryVariableOperation(IndexedVariable_T* indexed_var, const mxArray* in_var, msh_varop_T varop, long opts, mxArray** output);

void msh_VariableOperation(const mxArray* parent_var, const mxArray* subs_struct, const mxArray* in_vars, size_t num_in_vars, msh_varop_T varop, long opts, FileLock_T filelock, mxArray** output);

#endif /* MATSHARE_MSHVAROPS_H */
