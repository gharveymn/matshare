#ifndef MATSHARE_EXTERNTYPES_H
#define MATSHARE_EXTERNTYPES_H

#include "mex.h"


/* Credit to Yair M. Altman for this */
typedef struct
{
	void* name;             /*   prev - R2008b: Name of variable in workspace
				               R2009a - R2010b: NULL
				               R2011a - later : Reverse CrossLink pointer    */
	mxClassID ClassID;      /*  0 = unknown     10 = int16
                                1 = cell        11 = uint16
                                2 = struct      12 = int32
                                3 = logical     13 = uint32
                                4 = char        14 = int64
                                5 = void        15 = uint64
                                6 = double      16 = function_handle
                                7 = single      17 = opaque (classdef)
                                8 = int8        18 = object (old style)
                                9 = uint8       19 = index (deprecated)
                               10 = int16       20 = sparse (deprecated)     */
	int VariableType;       /*  0 = normal
                                1 = persistent
                                2 = global
                                3 = sub-element (field or cell)
                                4 = temporary
                                5 = (unknown)
                                6 = property of opaque class object
                                7 = (unknown)                                */
	mxArray* CrossLink;     /* Address of next shared-data variable          */
	size_t ndim;            /* Number of dimensions                          */
	unsigned int RefCount;  /* Number of extra sub-element copies            */
	unsigned int flags;     /*  bit  0 = is scalar double full
                                bit  2 = is empty double full
                                bit  4 = is temporary
                                bit  5 = is sparse
                                bit  9 = is numeric
                                bits 24 - 31 = User Bits                     */
	union mdim_data
	{
		size_t M;           /* Row size for 2D matrices, or                  */
		size_t* dims;       /* Pointer to dims array for nD > 2 arrays       */
	} Mdims;
	size_t N;               /* Product of dims 2:end                         */
	void* pr;               /* Real Data Pointer (or cell/field elements)    */
	void* pi;               /* Imag Data Pointer (or field information)      */
	union ir_data
	{
		mwIndex* ir;        /* Pointer to row values for sparse arrays       */
		mxClassID ClassID;  /* New User Defined Class ID (classdef)          */
		char* ClassName;    /* Pointer to Old User Defined Class Name        */
	} irClassNameID;
	union jc_data
	{
		mwIndex* jc;        /* Pointer to column values for sparse arrays    */
		mxClassID ClassID;  /* Old User Defined Class ID                     */
	} jcClassID;
	size_t nzmax;           /* Number of elements allocated for sparse       */
/*  size_t reserved;           Don't believe this! It is not really there!   */
} mxArrayStruct;

#endif //MATSHARE_EXTERNTYPES_H
