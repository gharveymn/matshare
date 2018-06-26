/** mshexterntypes.c
 * Provides a partial definition for mxArray. This is a hack.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#include "mex.h"

#include "../mshexterntypes.h"

/* Credit to James Tursa for the definitions */
#define MSH_USE_PARTIAL_DEFINITION 0

/* partial definition */
#if MSH_USE_PARTIAL_DEFINITION
struct InternalMexStruct_t
{
	void* name;             /*   prev - R2008b: Name of variable in workspace
				               R2009a - R2010b: NULL
				               R2011a - later : Reverse crosslink pointer    */
	mxClassID class_id;      /* 0 = unknown     10 = int16
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
	int variable_type;       /*  0 = normal
                                1 = persistent
                                2 = global
                                3 = sub-element (field or cell)
                                4 = temporary
                                5 = (unknown)
                                6 = property of opaque class object
                                7 = (unknown)                                */
	mxArray* crosslink;     /* Address of next shared-data variable          */
	/* the rest is undefined here */
};
#else

/* the full definition */
struct InternalMexStruct_t
{
	void* name;              /*   prev - R2008b: Name of variable in workspace
				               R2009a - R2010b: NULL
				               R2011a - later : Reverse crosslink pointer  */
	mxClassID class_id;      /* 0 = unknown
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
	int variable_type;       /* 0 = normal
                                 1 = persistent
                                 2 = global
                                 3 = sub-element (field or cell)
                                 4 = temporary
                                 5 = (unknown)
                                 6 = property of opaque class object
                                 7 = (unknown)                                 */
	mxArray* crosslink;      /* Address of next shared-data variable          */
	size_t ndim;             /* Number of dimensions                          */
	unsigned int ref_count;  /* Number of extra sub-element copies            */
	unsigned int flags;      /* bit  0 = is scalar double full
                                 bit  2 = is empty double full
                                 bit  4 = is temporary
                                 bit  5 = is sparse
                                 bit  9 = is numeric
                                 bits 24 - 31 = User Bits                      */
	union m_dims_u
	{
		size_t m;           /* Row size for 2D matrices, or                  */
		size_t* dims;       /* Pointer to dims array for nD > 2 arrays       */
	} m_dims;
	size_t n;                /* Product of dims 2:end                         */
	void* data;              /* Real Data Pointer (or cell/field elements)    */
	void* imag_data;         /* Imag Data Pointer (or field information)      */
	union ir_data_u
	{
		mwIndex* ir;        /* Pointer to row values for sparse arrays       */
		mxClassID class_id; /* New User Defined Class ID (classdef)          */
		char* class_name;   /* Pointer to Old User Defined Class Name        */
	} ir_data;
	union jc_data_u
	{
		mwIndex* jc;        /* Pointer to column values for sparse arrays    */
		mxClassID class_id; /* Old User Defined Class ID                     */
	} jc_data;
	size_t nzmax;            /* Number of elements allocated for sparse        */
};
#endif

void met_SetName(mxArray* var, mxArray* name)
{
	((InternalMexStruct_t*)var)->name = name;
}

void met_SetCrosslink(mxArray* var, mxArray* crosslink)
{
	((InternalMexStruct_t*)var)->crosslink = crosslink;
}

void* met_GetName(mxArray* var)
{
	return ((InternalMexStruct_t*)var)->name;
}

mxArray* met_GetCrosslink(mxArray* var)
{
	return ((InternalMexStruct_t*)var)->crosslink;
}

void met_SwapStruct(mxArray* dest, mxArray* src)
{
	InternalMexStruct_t temp = *((InternalMexStruct_t*)dest);
	*((InternalMexStruct_t*)dest) = *((InternalMexStruct_t*)src);
	*((InternalMexStruct_t*)src) = temp;
}


int met_PutSharedCopy(const char* workspace_name, const char* variable_name, mxArray* shared_data_copy)
{
	InternalMexStruct_t workspace_variable_copy;
	mxArray* workspace_variable, * dummy_variable,* curr_var;
	int is_doubly_linked = (met_GetName(met_GetCrosslink(shared_data_copy)) == shared_data_copy);
	
	dummy_variable = mxCreateNumericArray(0, NULL, mxDOUBLE_CLASS, mxREAL);
	
	if(mexPutVariable(workspace_name, variable_name, dummy_variable))
	{
		return 1;
	}
	mxDestroyArray(dummy_variable);
	
	/* note: abusing const */
	workspace_variable = (mxArray*)mexGetVariablePtr(workspace_name, variable_name);
	workspace_variable_copy = *((InternalMexStruct_t*)workspace_variable);
	*((InternalMexStruct_t*)workspace_variable) = *((InternalMexStruct_t*)shared_data_copy);
	*((InternalMexStruct_t*)shared_data_copy) = workspace_variable_copy;
	
	if(is_doubly_linked)
	{
		met_SetName(met_GetCrosslink(workspace_variable), workspace_variable);
		met_SetCrosslink(met_GetName(workspace_variable), workspace_variable);
	}
	else
	{
		/* swap back name */
		met_SetName(shared_data_copy, met_GetName(workspace_variable));
		met_SetName(workspace_variable, workspace_variable_copy.name);
		
		/* find the previous crosslink variable */
		curr_var = met_GetCrosslink(workspace_variable);
		while(met_GetCrosslink(curr_var) != shared_data_copy)
		{
			curr_var = met_GetCrosslink(curr_var);
		}
		met_SetCrosslink(curr_var, workspace_variable);
	}
	
	/* swap the variable type back */
	((InternalMexStruct_t*)shared_data_copy)->variable_type = ((InternalMexStruct_t*)workspace_variable)->variable_type;
	((InternalMexStruct_t*)workspace_variable)->variable_type = workspace_variable_copy.variable_type;
	
	/* reset the temp flag */
	((InternalMexStruct_t*)workspace_variable)->flags &= ~0x10;
	
	mxDestroyArray(shared_data_copy);
	
}
