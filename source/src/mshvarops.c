/** mshvarops.c
 * Defines functions for variable operations.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#include <math.h>
#include <string.h>

#include "mshvarops.h"
#include "mshutils.h"
#include "mshlockfree.h"
#include "mlerrorutils.h"

#define WideInput_T(TYPE) \
struct                    \
{                         \
	TYPE*     input;     \
	size_t    num_elems; \
	mxClassID mxtype;    \
}


static size_t msh_ParseIndicesWorker(mxArray*      subs_arr,
                                     const mwSize* dest_dims,
                                     size_t        dest_num_dims,
                                     size_t dest_num_elems,
                                     mwIndex*      start_idxs,
                                     const mwSize* slice_lens,
                                     size_t        num_parsed,
                                     size_t        curr_dim,
                                     size_t        base_dim,
                                     size_t        curr_mult,
                                     size_t        anchor_idx);

static void msh_CheckInputSize(mxArray* subs_arr, const mxArray* in_var, const size_t* dest_dims, size_t dest_num_dims);

int msh_GetNumVarOpArgs(msh_varop_T varop)
{
	switch(varop)
	{
		case(VAROP_ABS):
		case(VAROP_NEG): return 1;
		case(VAROP_ADD):
		case(VAROP_SUB):
		case(VAROP_MUL):
		case(VAROP_DIV):
		case(VAROP_REM):
		case(VAROP_MOD):
		case(VAROP_ARS):
		case(VAROP_ALS):
		case(VAROP_CPY): return 2;
		default:   meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "UnrecognizedVarOpError", "Unrecognized variable operation.");
	}
	return 0;
}


IndexedVariable_T msh_ParseSubscriptStruct(const mxArray* parent_var, const mxArray* subs_struct)
{
	size_t            i, num_idxs;
	mwIndex           index;
	struct
	{
		mxArray* type;
		mxChar* type_wstr;
		mxArray* subs;
		char subs_str[MSH_NAME_LEN_MAX];
	} idxstruct;
	
	IndexedVariable_T indexed_var = {parent_var, {subs_struct, NULL, 0, NULL, 0}};
	
	for(i = 0, num_idxs = mxGetNumberOfElements(subs_struct); i < num_idxs; i++)
	{
		idxstruct.type = mxGetField(subs_struct, i, "type");
		idxstruct.subs = mxGetField(subs_struct, i, "subs");
		
		if(idxstruct.type == NULL || mxIsEmpty(idxstruct.type) || !mxIsChar(idxstruct.type))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidOptionError", "The struct must have non-empty field 'type' of class 'char'. Use the 'substruct' function to create proper input.");
		}
		
		if(idxstruct.subs == NULL || mxIsEmpty(idxstruct.subs))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidOptionError", "The struct must have non-empty field 'subs'. Use the 'substruct' function to create proper input.");
		}
		
		idxstruct.type_wstr = mxGetChars(idxstruct.type);
		switch(idxstruct.type_wstr[0])
		{
			case ('.'):
			{
				if(!mxIsStruct(indexed_var.dest_var))
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndexError", "Variable class 'struct' required for this assignment.");
				}
				
				if(mxGetNumberOfElements(indexed_var.dest_var) != 1)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndexError", "Scalar struct required for this assignment.");
				}
				
				mxGetString(idxstruct.subs, idxstruct.subs_str, MSH_NAME_LEN_MAX);
				
				if((indexed_var.dest_var = mxGetField(indexed_var.dest_var, 0, idxstruct.subs_str)) == NULL)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "FieldNotFoundError", "Could not find field '%s'.", idxstruct.subs_str);
				}
				
				break;
			}
			case ('{'):
			{
				/* this should be a cell array */
				if(!mxIsCell(indexed_var.dest_var))
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndexError", "Variable class 'cell' required for this assignment.");
				}
				
				/* only proceed if this is a singular index */
				
				index = msh_ParseSingleIndex(idxstruct.subs, indexed_var.dest_var);
				indexed_var.dest_var = mxGetCell(indexed_var.dest_var, index);
				
				break;
			}
			case ('('):
			{
				/* if this is not the last indexing then can only proceed if this is a singular index to a struct */
				if(i + 1 < num_idxs)
				{
					if(mxIsStruct(indexed_var.dest_var))
					{
						index = msh_ParseSingleIndex(idxstruct.subs, indexed_var.dest_var);
						
						/* to to next subscript */
						i++;
						
						idxstruct.type = mxGetField(subs_struct, i, "type");
						idxstruct.subs = mxGetField(subs_struct, i, "subs");
						
						if(idxstruct.type == NULL || mxIsEmpty(idxstruct.type) || !mxIsChar(idxstruct.type))
						{
							meu_PrintMexError(MEU_FL,
							                  MEU_SEVERITY_USER,
							                  "InvalidOptionError",
							                  "The struct must have non-empty field 'type' of class 'char'. Use the 'substruct' function to create proper input.");
						}
						
						if(idxstruct.subs == NULL || mxIsEmpty(idxstruct.subs))
						{
							meu_PrintMexError(MEU_FL,
							                  MEU_SEVERITY_USER,
							                  "InvalidOptionError",
							                  "The struct must have non-empty field 'subs'. Use the 'substruct' function to create proper input.");
						}
						
						idxstruct.type_wstr = mxGetChars(idxstruct.type);
						if(mxGetChars(idxstruct.type)[0] != '.')
						{
							meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Expected dot-expression after '()' for indexing struct.");
						}
						
						mxGetString(idxstruct.subs, idxstruct.subs_str, MSH_NAME_LEN_MAX);
						
						if((indexed_var.dest_var = mxGetField(indexed_var.dest_var, index, idxstruct.subs_str)) == NULL)
						{
							meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "FieldNotFoundError", "Could not find field '%s'.", idxstruct.subs_str);
						}
						
					}
					else
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Cannot index where '()' is not the last argument for non-struct array.");
					}
				}
				else
				{
					indexed_var.indices = msh_ParseIndices(idxstruct.subs, indexed_var.dest_var);
				}
				break;
			}
			default:
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedIndexTypeError", "Unrecognized type '%s' for indexing", mxArrayToString(idxstruct.type));
			}
		}
	}
	
	return indexed_var;
	
}


ParsedIndices_T msh_ParseIndices(mxArray* subs_arr, const mxArray* dest_var)
{
	size_t          i, j, num_subs, base_dim, max_mult;
	double          dbl_iter;
	mxArray*        curr_subs;
	
	size_t          dest_num_elems = mxGetNumberOfElements(dest_var);
	size_t          num_subs_elems = 0;
	size_t          base_mult     = 1;
	mwSize          dest_num_dims  = mxGetNumberOfDimensions(dest_var);
	ParsedIndices_T parsed_indices = {0};
	double*         indices        = NULL;
	const size_t*   dest_dims      = mxGetDimensions(dest_var);
	
	if(!mxIsCell(subs_arr))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Indexing subscripts be stored in a cell array.");
	}
	
	if((num_subs = mxGetNumberOfElements(subs_arr)) < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Index cell array must have more than one element.");
	}
	
	if(dest_num_dims < 2)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "NotEnoughDimensionsError", "Too few dimensions. Need at least 2 dimensions for assignment.");
	}
	
	/* parse the sections where copies will be contiguous
	 * double ptr to index array
	 * slice length array for each of the index arrays
	 */
	
	/* see if we can increase the slice size */
	for(base_dim = 0; base_dim < dest_num_dims && base_dim < num_subs; base_mult *= dest_dims[base_dim++])
	{
		curr_subs = mxGetCell(subs_arr, base_dim);
		num_subs_elems = mxGetNumberOfElements(curr_subs);
		
		if(mxIsChar(curr_subs))
		{
			/* if all then keep going */
			if(base_dim + 1 == num_subs)
			{
				do
				{
					base_mult *= dest_dims[base_dim++];
				} while(base_dim < dest_num_dims);
				break;
			}
		}
		else/*if(mxIsDouble(curr_subs))*/
		{
			indices = mxGetData(curr_subs);
			if(num_subs_elems != dest_dims[base_dim])
			{
				break;
			}
			
			for(j = 0; j < num_subs_elems; j++)
			{
				if(j != (size_t)indices[j] - 1)
				{
					goto BREAK_OUTER;
				}
			}
		}
	}

BREAK_OUTER:
	
	/* these are set in the loop above which is guaranteed to run at least once
	curr_subs = mxGetCell(subs_arr, base_dim);
	num_subs_elems = mxGetNumberOfElements(curr_subs);
	*/
	
	if(base_dim < dest_num_dims)
	{
		/* get total lengths from the first non-continuous dimension */
		
		/* note: this is an oversized allocation */
		parsed_indices.slice_lens = mxMalloc(num_subs_elems*sizeof(mwSize));
		for(i = 0, parsed_indices.num_lens = 0; i < num_subs_elems; parsed_indices.num_lens++)
		{
			dbl_iter = indices[i];
			parsed_indices.slice_lens[parsed_indices.num_lens] = 0;
			do
			{
				i += 1;
				dbl_iter += 1.0;
				parsed_indices.slice_lens[parsed_indices.num_lens]++;
			} while(i < num_subs_elems && dbl_iter == indices[i]);
			parsed_indices.slice_lens[parsed_indices.num_lens] *= base_mult;
		}
	}
	else
	{
		parsed_indices.num_lens = 1;
		parsed_indices.slice_lens = mxMalloc(parsed_indices.num_lens*sizeof(mwSize));
		parsed_indices.slice_lens[0] = dest_num_elems;
	}
	
	/* parse the indices */
	parsed_indices.num_idxs = parsed_indices.num_lens;
	for(i = base_dim + 1; i < num_subs; i++)
	{
		curr_subs = mxGetCell(subs_arr, i);
		if(mxIsChar(curr_subs))
		{
			/* if all then keep going */
			if(i < dest_num_dims)
			{
				parsed_indices.num_idxs *= dest_dims[i];
			}
		}
		else/*if(mxIsDouble(curr_subs))*/
		{
			parsed_indices.num_idxs *= mxGetNumberOfElements(curr_subs);
		}
	}
	
	parsed_indices.start_idxs = mxMalloc(parsed_indices.num_idxs * sizeof(mwIndex));
	
	for(i = 0, max_mult = 1; i < MIN(num_subs, dest_num_dims)-1; i++)
	{
		max_mult *= dest_dims[i];
	}
	
	msh_ParseIndicesWorker(subs_arr, dest_dims, dest_num_dims, dest_num_elems, parsed_indices.start_idxs, parsed_indices.slice_lens, 0, num_subs - 1, base_dim, max_mult, 0);
	
	return parsed_indices;
	
}


mwIndex msh_ParseSingleIndex(mxArray* subs_arr, const mxArray* dest_var)
{
	size_t          i, num_subs, max_mult;
	mwIndex         ret_idx;
	mxArray*        curr_subs;
	
	size_t          num_subs_elems   = 0;
	mwSize          dummy_slice_lens = 1;
	size_t          dest_num_elems   = mxGetNumberOfElements(dest_var);
	mwSize          dest_num_dims    = mxGetNumberOfDimensions(dest_var);
	const size_t*   dest_dims        = mxGetDimensions(dest_var);
	
	if(!mxIsCell(subs_arr))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Indexing subscripts be stored in a cell array.");
	}
	
	if((num_subs = mxGetNumberOfElements(subs_arr)) < 1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Index cell array must have more than one element.");
	}
	
	if(dest_num_dims < 2)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "NotEnoughDimensionsError", "Too few dimensions. Need at least 2 dimensions for assignment.");
	}
	
	for(i = 0; i < num_subs; i++)
	{
		
		curr_subs = mxGetCell(subs_arr, i);
		num_subs_elems = mxGetNumberOfElements(curr_subs);
		
		if(num_subs_elems < 1)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Subscript must have at least one element.");
		}
		
		if(num_subs_elems > 1)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Too many outputs from from cell or struct indexing.");
		}
		
		if(mxIsChar(curr_subs))
		{
			if(mxGetChars(curr_subs)[0] != ':')
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown index value '%s'.", mxArrayToString(curr_subs));
			}
			
			if(i < dest_num_dims && dest_dims[i] > 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Too many outputs from from cell or struct indexing.");
			}
		}
		else if(!mxIsDouble(curr_subs))
		{
			/* no logical handling since that will be handled switched to numeric by the entry function */
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown indexing method.");
		}
	}
	
	for(i = 0, max_mult = 1; i < MIN(num_subs, dest_num_dims)-1; i++)
	{
		max_mult *= dest_dims[i];
	}
	
	msh_ParseIndicesWorker(subs_arr, dest_dims, dest_num_dims, dest_num_elems, &ret_idx, &dummy_slice_lens, 0, num_subs - 1, 0, max_mult, 0);
	
	return ret_idx;
}


static size_t msh_ParseIndicesWorker(mxArray*      subs_arr,
                                     const mwSize* dest_dims,
                                     size_t dest_num_dims,
                                     size_t dest_num_elems,
                                     mwIndex* start_idxs,
                                     const mwSize* slice_lens,
                                     size_t num_parsed,
                                     size_t curr_dim,
                                     size_t base_dim,
                                     size_t curr_mult,
                                     size_t anchor_idx)
{
	size_t   i, j, num_elems;
	double*  indices;
	
	mxArray* curr_sub = mxGetCell(subs_arr, curr_dim);
	
	if(curr_dim > base_dim)
	{
		if(mxIsChar(curr_sub))
		{
			if(curr_dim < dest_num_dims)
			{
				for(i = 0; i < dest_dims[curr_dim]; i++)
				{
					num_parsed = msh_ParseIndicesWorker(subs_arr,
					                                    dest_dims,
					                                    dest_num_dims,
					                                    dest_num_elems,
					                                    start_idxs,
					                                    slice_lens,
					                                    num_parsed,
					                                    curr_dim - 1,
					                                    base_dim,
					                                    curr_mult/dest_dims[curr_dim - 1],
					                                    anchor_idx + i*curr_mult);
				}
			}
			else
			{
				num_parsed = msh_ParseIndicesWorker(subs_arr,
				                                    dest_dims,
				                                    dest_num_dims,
				                                    dest_num_elems,
				                                    start_idxs,
				                                    slice_lens,
				                                    num_parsed,
				                                    curr_dim - 1,
				                                    base_dim,
				                                    curr_mult,
				                                    anchor_idx);
			}
		}
		else/*if(mxIsDouble(curr_sub))*/
		{
			indices = mxGetData(curr_sub);
			
			if(curr_dim < dest_num_dims)
			{
				for(i = 0, num_elems = mxGetNumberOfElements(curr_sub); i < num_elems; i++)
				{
					num_parsed = msh_ParseIndicesWorker(subs_arr,
					                                    dest_dims,
					                                    dest_num_dims,
					                                    dest_num_elems,
					                                    start_idxs,
					                                    slice_lens,
					                                    num_parsed,
					                                    curr_dim - 1,
					                                    base_dim,
					                                    curr_mult/dest_dims[curr_dim - 1],
					                                    anchor_idx + ((mwIndex)indices[i] - 1)*curr_mult);
				}
			}
			else
			{
				for(i = 0, num_elems = mxGetNumberOfElements(curr_sub); i < num_elems; i++)
				{
					num_parsed = msh_ParseIndicesWorker(subs_arr,
					                                    dest_dims,
					                                    dest_num_dims,
					                                    dest_num_elems,
					                                    start_idxs,
					                                    slice_lens,
					                                    num_parsed,
					                                    curr_dim - 1,
					                                    base_dim,
					                                    curr_mult,
					                                    anchor_idx);
				}
			}
		}
	}
	else
	{
		if(mxIsChar(curr_sub))
		{
			start_idxs[num_parsed] = anchor_idx;
			if(dest_num_elems < start_idxs[num_parsed] + slice_lens[0])
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Indexing exceeds the bounds of the destination variable.");
			}
			num_parsed += 1;
		}
		else/*if(mxIsDouble(curr_sub))*/
		{
			indices = mxGetData(curr_sub);
			for(i = 0, j = 0, num_elems = mxGetNumberOfElements(curr_sub); i < num_elems; i += slice_lens[j++])
			{
				start_idxs[num_parsed] = anchor_idx + ((mwIndex)indices[i]-1)*curr_mult;
				if(dest_num_elems < start_idxs[num_parsed] + slice_lens[j])
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Indexing exceeds the bounds of the destination variable.");
				}
				num_parsed += 1;
			}
		}
	}
	
	return num_parsed;
	
}


void msh_CheckValidInput(IndexedVariable_T* indexed_var, const mxArray* in_var, msh_varop_T varop)
{
	size_t num_idxs;
	mxArray* type;
	mxArray* subs;
	
	if(indexed_var->indices.subs_struct != NULL)
	{
		if((num_idxs = mxGetNumberOfElements(in_var)) < 1)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidOptionError", "The struct must have non-empty. Use the 'substruct' function to create proper input.");
		}
		
		type = mxGetField(indexed_var->indices.subs_struct, num_idxs - 1, "type");
		subs = mxGetField(indexed_var->indices.subs_struct, num_idxs - 1, "subs");
		
		if(type == NULL || mxIsEmpty(type) || !mxIsChar(type))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidOptionError", "The struct must have non-empty field 'type' of class 'char'. Use the 'substruct' function to create proper input.");
		}
		
		if(subs == NULL || mxIsEmpty(subs))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidOptionError", "The struct must have non-empty field 'subs'. Use the 'substruct' function to create proper input.");
		}
		
		switch(mxGetChars(type)[0])
		{
			case ('('):
			{
				msh_CheckInputSize(subs, in_var, mxGetDimensions(indexed_var->dest_var), mxGetNumberOfDimensions(indexed_var->dest_var));
				break;
			}
			case ('.'):
			case ('{'):
				break;
			default:
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedIndexTypeError", "Unrecognized type '%s' for indexing", mxArrayToString(type));
			}
		}
	}
	
	msh_CompareVariableSize(indexed_var, in_var, varop);
	
}

static void msh_CheckInputSize(mxArray* subs_arr, const mxArray* in_var, const size_t* dest_dims, size_t dest_num_dims)
{
	
	/* this checks for validity of subscripted input */
	
	size_t          i, j, k, exp_num_elems;
	size_t          first_sig_dim, last_sig_dim, num_sig_dims;
	mxArray*        curr_subs;
	
	size_t          num_subs_elems = 0;
	size_t          num_subs       = mxGetNumberOfElements(subs_arr);
	mwSize          in_num_dims    = mxGetNumberOfDimensions(in_var);
	const size_t*   in_dims        = mxGetDimensions(in_var);
	
	if(mxIsEmpty(in_var))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "EmptyAssignmentError", "Assignment input must be non-empty.");
	}
	
	if(mxGetNumberOfElements(in_var) > 1)
	{
		/* find the first significant dimension */
		for(first_sig_dim = 0; first_sig_dim < num_subs; first_sig_dim++)
		{
			curr_subs = mxGetCell(subs_arr, first_sig_dim);
			num_subs_elems = mxGetNumberOfElements(curr_subs);
			
			if(num_subs_elems < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Subscript must have at least one element.");
			}
			
			if(mxIsChar(curr_subs))
			{
				if(num_subs_elems != 1 || mxGetChars(curr_subs)[0] != ':')
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown index value '%s'.", mxArrayToString(curr_subs));
				}
				
				if(first_sig_dim < dest_num_dims && dest_dims[first_sig_dim] > 1)
				{
					break;
				}
			}
			else if(mxIsDouble(curr_subs))
			{
				if(num_subs_elems > 1)
				{
					break;
				}
			}
			else
			{
				/* no logical handling since that will be handled switched to numeric by the entry function */
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown indexing method.");
			}
		}
		
		for(last_sig_dim = num_subs - 1; last_sig_dim > first_sig_dim; last_sig_dim--)
		{
			curr_subs = mxGetCell(subs_arr, last_sig_dim);
			num_subs_elems = mxGetNumberOfElements(curr_subs);
			
			if(num_subs_elems < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Subscript must have at least one element.");
			}
			
			if(mxIsChar(curr_subs))
			{
				if(num_subs_elems != 1 || mxGetChars(curr_subs)[0] != ':')
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown index value '%s'.", mxArrayToString(curr_subs));
				}
				
				if(last_sig_dim < dest_num_dims && dest_dims[last_sig_dim] > 1)
				{
					break;
				}
			}
			else if(mxIsDouble(curr_subs))
			{
				if(num_subs_elems > 1)
				{
					break;
				}
			}
			else
			{
				/* no logical handling since that will be handled switched to numeric by the entry function */
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown indexing method.");
			}
		}
		
		num_sig_dims = last_sig_dim - first_sig_dim + 1;
		
		/* check input validity in between the significant dim ends */
		for(i = first_sig_dim + 1; i < last_sig_dim; i++)
		{
			curr_subs = mxGetCell(subs_arr, first_sig_dim);
			num_subs_elems = mxGetNumberOfElements(curr_subs);
			
			if(num_subs_elems < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexingError", "Subscript must have at least one element.");
			}
			
			if(mxIsChar(curr_subs) && (num_subs_elems != 1 || mxGetChars(curr_subs)[0] != ':'))
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown index value '%s'.", mxArrayToString(curr_subs));
			}
			else if(!mxIsDouble(curr_subs))
			{
				/* no logical handling since that will be handled switched to numeric by the entry function */
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnknownIndexTypeError", "Unknown indexing method.");
			}
		}
		
		/* check assignment sizing */
		for(i = 0; i < in_num_dims; i++)
		{
			if(in_dims[i] > 1)
			{
				
				if(in_num_dims - i < num_sig_dims)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "AssignmentMismatchError", "Assignment input dimension mismatch.");
				}
				
				for(j = first_sig_dim; j <= last_sig_dim; j++, i++)
				{
					curr_subs = mxGetCell(subs_arr, j);
					
					if(mxIsChar(curr_subs))
					{
						if(j < dest_num_dims)
						{
							/* if this is the last subscript then this dim should hold all the rest */
							if(j + 1 == num_subs)
							{
								for(k = j, exp_num_elems = 1; k < dest_num_dims; k++)
								{
									exp_num_elems *= dest_dims[k];
								}
							}
							else
							{
								exp_num_elems = dest_dims[j];
							}
						}
						else
						{
							exp_num_elems = 1;
						}
						
						if(in_dims[i] != exp_num_elems)
						{
							meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "AssignmentMismatchError", "Assignment input dimension mismatch.");
						}
						
					}
					else
					{
						if(in_dims[i] != mxGetNumberOfElements(curr_subs))
						{
							meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "AssignmentMismatchError", "Assignment input dimension mismatch.");
						}
					}
				}
				
				/* rest of the dimensions must be singleton */
				for(; i < in_num_dims; i++)
				{
					if(in_dims[i] != 1)
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "AssignmentMismatchError", "Assignment input dimension mismatch.");
					}
				}
				
			}
		}
	}
	else
	{
		/* just check the indexing validity */
		for(i = 0; i < num_subs; i++)
		{
			curr_subs = mxGetCell(subs_arr, i);
			num_subs_elems = mxGetNumberOfElements(curr_subs);
			
			if(num_subs_elems < 1)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Subscript must have at least one element.");
			}
			
			if(mxIsChar(curr_subs) && (num_subs_elems != 1 || mxGetChars(curr_subs)[0] != ':'))
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Unknown index value '%s'.", mxArrayToString(curr_subs));
			}
			else if(!mxIsDouble(curr_subs))
			{
				/* no logical handling since that will be handled switched to numeric by the entry function */
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IndexingError", "Unknown indexing method.");
			}
		}
	}
	
}


void msh_CompareVariableSize(IndexedVariable_T* indexed_var, const mxArray* in_var, msh_varop_T varop)
{
	int               field_num, dest_num_fields;
	size_t            i, j, dest_idx, comp_idx, dest_num_elems;
	const char_T*     curr_field_name;
	
	size_t            in_num_elems  = mxGetNumberOfElements(in_var);
	mxClassID         dest_class_id = mxGetClassID(indexed_var->dest_var);
	mxClassID         in_class_id   = mxGetClassID(in_var);
	IndexedVariable_T sub_variable  = {0};
	
	/*
	if(dest_class_id != mxGetClassID(in_var))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(in_var), mxGetClassName(indexed_var->dest_var));
	}
	*/
	
	/* if this is false, in the other case, the sizes will already be checked unless it is sparse */
	if(indexed_var->indices.start_idxs == NULL && in_num_elems != 1)
	{
		/* can't allow differing dimensions because matlab doesn't use shared pointers to dimensions in mxArrays */
		if(mxGetNumberOfDimensions(indexed_var->dest_var) != mxGetNumberOfDimensions(in_var) ||
		   memcmp(mxGetDimensions(indexed_var->dest_var), mxGetDimensions(in_var), mxGetNumberOfDimensions(indexed_var->dest_var)*sizeof(mwSize)) != 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleSizeError", "The input variable was of differing size in comparison to the destination.");
		}
	}
	
	if(dest_class_id == mxSTRUCT_CLASS)
	{
		/* must be struct */
		if(dest_class_id != mxGetClassID(in_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(in_var), mxGetClassName(indexed_var->dest_var));
		}
		
		dest_num_elems = mxGetNumberOfElements(indexed_var->dest_var);
		dest_num_fields = mxGetNumberOfFields(indexed_var->dest_var);
		
		if(dest_num_fields != mxGetNumberOfFields(in_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumFieldsError", "The number of fields in the input variable (%d) differs from that of the destination (%d).", mxGetNumberOfFields(in_var), dest_num_fields);
		}
		
		/* search for each field name */
		for(field_num = 0; field_num < dest_num_fields; field_num++)
		{
			if(mxGetField(in_var, 0, mxGetFieldNameByNumber(indexed_var->dest_var, field_num)) == NULL)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "MissingFieldError", "The input variable is missing the field '%s'.", mxGetFieldNameByNumber(indexed_var->dest_var, field_num));
			}
		}
		
		if(indexed_var->indices.start_idxs == NULL)
		{
			for(field_num = 0; field_num < dest_num_fields; field_num++)     /* each field */
			{
				curr_field_name = mxGetFieldNameByNumber(indexed_var->dest_var, field_num);
				for(dest_idx = 0; dest_idx < dest_num_elems; dest_idx++)
				{
					sub_variable.dest_var = mxGetField(indexed_var->dest_var, dest_idx, curr_field_name);
					msh_CompareVariableSize(&sub_variable, mxGetField(in_var, dest_idx, curr_field_name), VAROP_MUL);
				}
			}
		}
		else
		{
			for(field_num = 0; field_num < dest_num_fields; field_num++)     /* each field */
			{
				curr_field_name = mxGetFieldNameByNumber(indexed_var->dest_var, field_num);
				for(i = 0, comp_idx = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
				{
					for(j = 0; j < indexed_var->indices.num_lens; j++)
					{
						for(dest_idx = indexed_var->indices.start_idxs[i+j]; dest_idx < indexed_var->indices.start_idxs[i+j] + indexed_var->indices.slice_lens[j]; dest_idx++, comp_idx++)
						{
							sub_variable.dest_var = mxGetField(indexed_var->dest_var, dest_idx, curr_field_name);
							msh_CompareVariableSize(&sub_variable, mxGetField(in_var, (in_num_elems == 1)? 0 : comp_idx, curr_field_name), VAROP_MUL);
						}
					}
				}
			}
		}
	}
	else if(dest_class_id == mxCELL_CLASS)
	{
		/* must be cell */
		if(dest_class_id != mxGetClassID(in_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(in_var), mxGetClassName(indexed_var->dest_var));
		}
		
		dest_num_elems = mxGetNumberOfElements(indexed_var->dest_var);
		
		if(indexed_var->indices.start_idxs == NULL)
		{
			for(dest_idx = 0; dest_idx < dest_num_elems; dest_idx++)
			{
				sub_variable.dest_var = mxGetCell(indexed_var->dest_var, dest_idx);
				msh_CompareVariableSize(&sub_variable, mxGetCell(in_var, dest_idx), VAROP_MUL);
			}
		}
		else
		{
			for(i = 0, comp_idx = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
			{
				for(j = 0; j < indexed_var->indices.num_lens; j++)
				{
					for(dest_idx = indexed_var->indices.start_idxs[i+j]; dest_idx < indexed_var->indices.start_idxs[i+j] + indexed_var->indices.slice_lens[j]; dest_idx++, comp_idx++)
					{
						sub_variable.dest_var = mxGetCell(indexed_var->dest_var, dest_idx);
						msh_CompareVariableSize(&sub_variable, mxGetCell(in_var, (in_num_elems == 1)? 0 : comp_idx), VAROP_MUL);
					}
				}
			}
		}
	}
	else if(mxIsNumeric(indexed_var->dest_var) || dest_class_id == mxLOGICAL_CLASS || dest_class_id == mxCHAR_CLASS)
	{
		
		/* must be base level variable */
		if(!mxIsNumeric(in_var) && !mxIsLogical(in_var) && !mxIsChar(in_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(in_var), mxGetClassName(indexed_var->dest_var));
		}
		
		if(mxIsSparse(indexed_var->dest_var))
		{
			if(varop != VAROP_CPY)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "SparseVarOpError", "Non-copying variable operations are not available for shared sparse matrices.");
			}
			
			if(!mxIsSparse(in_var))
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NonSparseInputError", "Input for sparse copying must be sparse.");
			}
			
			/* sparse input must be the same size */
			if(indexed_var->indices.start_idxs != NULL || in_num_elems == 1)
			{
				/* can't allow differing dimensions because matlab doesn't use shared pointers to dimensions in mxArrays */
				if(mxGetM(indexed_var->dest_var) != mxGetM(in_var) || mxGetN(indexed_var->dest_var) != mxGetN(in_var))
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleSizeError", "The input variable was of differing size in comparison to the destination. "
															"Sparse input must have the same size as the destination");
				}
			}
			
			if(mxGetNzmax(indexed_var->dest_var) < mxGetNzmax(in_var))
			{
				meu_PrintMexError(MEU_FL,
				                  MEU_SEVERITY_USER,
				                  "NzmaxError",
				                  "The number of non-zero entries in the input variable ("SIZE_FORMAT") is larger than maximum number \n"
										   "allocated for destination ("SIZE_FORMAT"). This is an error because doing so would change the size \n"
										   "of shared memory.",
				                  mxGetNzmax(in_var),
				                  mxGetNzmax(indexed_var->dest_var));
			}
		}
		else
		{
			
			if(varop != VAROP_CPY)
			{
				
				if(dest_class_id == mxCHAR_CLASS)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "DestCharError", "Destination class cannot be 'char' for variable operations other than overwriting.");
				}
				
				/*
				 * Rules:
				 * floats may operate with floats
				 * integers may only operate with integers of the same class, or scalar doubles
				 */
				
				/* check if dest class is int */
				if(dest_class_id != mxDOUBLE_CLASS && dest_class_id != mxSINGLE_CLASS)
				{
					/* check if same int class */
					if(dest_class_id != in_class_id)
					{
						/* if not then must be scalar double */
						if((in_class_id != mxDOUBLE_CLASS && in_class_id != mxCHAR_CLASS) || in_num_elems != 1)
						{
							meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IntegerOpError", "Fixed width integer operations may only be performed with both operands of the same type "
							                                                               "or with one of the operands in scalar form of class 'double' or 'char'.");
						}
					}
					
				}
				else if(in_class_id != mxDOUBLE_CLASS && in_class_id != mxSINGLE_CLASS && in_class_id != mxCHAR_CLASS)
				{
					/* error if dest is single or not scalar */
					if(dest_class_id == mxSINGLE_CLASS || mxGetNumberOfElements(indexed_var->dest_var) != 1)
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IntegerOpError", "Fixed width integer operations may only be performed with both operands of the same type "
						                                                               "or with one of the operands in scalar form of class 'double' or 'char'.");
					}
				}
				
			}
			
			if(mxIsSparse(in_var))
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "SparseInputError", "Cannot convert sparse matrix to full.");
			}
		}
		
		if(mxIsComplex(indexed_var->dest_var) != mxIsComplex(in_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "ComplexityError", "The complexity of the input variable does not match with that of the destination.");
		}
	}
	else
	{
		/* this may occur if the user passes a destination variable which is not in shared memory */
		meu_PrintMexError(MEU_FL,
		                  MEU_SEVERITY_INTERNAL,
		                  "InvalidTypeError",
		                  "Unexpected destination type. All elements of the shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'. Check that your destination is a matshare object.");
	}
	
}


/** meta routines **/

#define VO_FCN_FNAME_(OP, TYPEN) msh_##OP##TYPEN
#define VO_FCN_RNAME_(OP, TYPEN) msh_##OP##TYPEN##Runner
#define VO_FCN_CTCNAME_(TYPEN) msh_Choose##TYPEN##Converter
#define VO_FCN_TCNAME_(TYPEN1, TYPEN2) msh_TC##TYPEN1##From##TYPEN2

#define VO_FCN_FNAME(OP, TYPEN) VO_FCN_FNAME_(OP, TYPEN)
#define VO_FCN_RNAME(OP, TYPEN) VO_FCN_RNAME_(OP, TYPEN)
#define VO_FCN_CTCNAME(TYPEN) VO_FCN_CTCNAME_(TYPEN)
#define VO_FCN_TCNAME(TYPEN1, TYPEN2) VO_FCN_TCNAME_(TYPEN1, TYPEN2)

#define FW_INT_TYPEC(SIZE) int##SIZE##conv_T
#define FW_INT_TYPEN(SIZE) Int##SIZE
#define FW_INT_TYPE(SIZE) int##SIZE##_T
#define FW_INT_MAX(SIZE) INT##SIZE##_MAX
#define FW_INT_MIN(SIZE) INT##SIZE##_MIN

#define FW_INT_FCN_RNAME(OP, SIZE) VO_FCN_RNAME(OP, FW_INT_TYPEN(SIZE))
#define FW_INT_FCN_FNAME(OP, SIZE) VO_FCN_FNAME(OP, FW_INT_TYPEN(SIZE))
#define FW_INT_FCN_CTCNAME(SIZE) VO_FCN_CTCNAME(FW_INT_TYPEN(SIZE))
#define FW_II_FCN_TCNAME(SIZE1,SIZE2) VO_FCN_TCNAME(FW_INT_TYPEN(SIZE1), FW_INT_TYPEN(SIZE2))
#define FW_IU_FCN_TCNAME(SIZE1,SIZE2) VO_FCN_TCNAME(FW_INT_TYPEN(SIZE1), FW_UINT_TYPEN(SIZE2))

#define FW_UINT_TYPEC(SIZE) uint##SIZE##conv_T
#define FW_UINT_TYPEN(SIZE) UInt##SIZE
#define FW_UINT_TYPE(SIZE) uint##SIZE##_T
#define FW_UINT_MAX(SIZE) UINT##SIZE##_MAX

#define FW_UINT_FCN_RNAME(OP, SIZE) VO_FCN_RNAME(OP, FW_UINT_TYPEN(SIZE))
#define FW_UINT_FCN_FNAME(OP, SIZE) VO_FCN_FNAME(OP, FW_UINT_TYPEN(SIZE))
#define FW_UINT_FCN_CTCNAME(SIZE) VO_FCN_CTCNAME(FW_UINT_TYPEN(SIZE))
#define FW_UU_FCN_TCNAME(SIZE1,SIZE2) VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE1), FW_UINT_TYPEN(SIZE2))
#define FW_UI_FCN_TCNAME(SIZE1,SIZE2) VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE1), FW_INT_TYPEN(SIZE2))

/** function declarations (need these here for the metadef) **/
static mxDouble VO_FCN_FNAME(Add, Double)(mxDouble accum, mxDouble in);
static mxDouble VO_FCN_FNAME(Sub, Double)(mxDouble accum, mxDouble in);
static mxDouble VO_FCN_FNAME(Mul, Double)(mxDouble accum, mxDouble in);
static mxDouble VO_FCN_FNAME(Div, Double)(mxDouble accum, mxDouble in);
static mxDouble VO_FCN_FNAME(Rem, Double)(mxDouble accum, mxDouble in);
static mxDouble VO_FCN_FNAME(Mod, Double)(mxDouble accum, mxDouble in);
static mxDouble VO_FCN_FNAME(ARS, Double)(mxDouble accum, mxDouble in);
static mxDouble VO_FCN_FNAME(ALS, Double)(mxDouble accum, mxDouble in);

/** Type Conversion routines **/
#define TC_II_L_DEF(TYPE1, TYPE2, TYPE1_MAX, TYPE1_MIN, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	TYPE2 in_val = *((TYPE2*)v_in); \
	if(in_val > (TYPE1_MAX)) \
	{ \
		return TYPE1_MAX; \
	} \
	else if(in_val < (TYPE1_MIN)) \
	{ \
		return TYPE1_MIN; \
	} \
	else \
	{ \
		return (TYPE1)in_val; \
	} \
}

#define TC_IU_L_DEF(TYPE1, TYPE2, TYPE1_MAX, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	TYPE2 in_val = *((TYPE2*)v_in); \
	if(in_val > (TYPE1_MAX)) \
	{ \
		return TYPE1_MAX; \
	} \
	else \
	{ \
		return (TYPE1)in_val; \
	} \
}

#define TC_INT_F_DEF(TYPE1, TYPE2, TYPE1_MAX, TYPE1_MIN, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	TYPE2 in_val = *((TYPE2*)v_in); \
	if(in_val > 0) \
	{ \
		if(in_val > (TYPE1_MAX)) \
		{ \
			return TYPE1_MAX; \
		} \
		return (TYPE1)(in_val + 0.5); \
	} \
	else if(in_val < 0) \
	{ \
		if(in_val < (TYPE1_MIN)) \
		{ \
			return TYPE1_MIN; \
		} \
		return (TYPE1)(in_val - 0.5); \
	} \
	return 0; \
}

#define TC_UU_L_DEF(TYPE1, TYPE2, TYPE1_MAX, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	TYPE2 in_val = *((TYPE2*)v_in); \
	if(in_val > (TYPE1_MAX)) \
     { \
          return TYPE1_MAX; \
     } \
     else \
     { \
		return (TYPE1)in_val; \
	} \
}

#define TC_UI_L_DEF(TYPE1, TYPE2, TYPE1_MAX, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	TYPE2 in_val = *((TYPE2*)v_in); \
	if(in_val > (TYPE1_MAX)) \
     { \
          return (TYPE1_MAX); \
     } \
	else if(in_val < 0) \
     { \
          return 0; \
     } \
     else \
     { \
		return (TYPE1)in_val; \
	} \
}

#define TC_INT_U_DEF(TYPE1, TYPE2, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	return (TYPE1)*((TYPE2*)v_in + offset); \
}


#define TC_UINT_U_DEF(TYPE1, TYPE2, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	TYPE2 in_val = *((TYPE2*)v_in); \
	if(in_val < 0) \
	{ \
		return 0; \
	} \
	else \
	{ \
		return (TYPE1)*((TYPE2*)v_in + offset); \
	} \
}

#define FW_TC_INT_L_METADEF(SIZE1, SIZE2) \
TC_II_L_DEF(FW_INT_TYPE(SIZE1), FW_INT_TYPE(SIZE2), FW_INT_MAX(SIZE1), FW_INT_MIN(SIZE1), FW_II_FCN_TCNAME(SIZE1, SIZE2)); \
TC_IU_L_DEF(FW_INT_TYPE(SIZE1), FW_UINT_TYPE(SIZE2), FW_INT_MAX(SIZE1), FW_IU_FCN_TCNAME(SIZE1, SIZE2));

#define FW_TC_INT_U_METADEF(SIZE1, SIZE2) \
TC_INT_U_DEF(FW_INT_TYPE(SIZE1), FW_INT_TYPE(SIZE2), FW_II_FCN_TCNAME(SIZE1, SIZE2)); \
TC_INT_U_DEF(FW_INT_TYPE(SIZE1), FW_UINT_TYPE(SIZE2), FW_IU_FCN_TCNAME(SIZE1, SIZE2));

#define FW_TC_INT_F_METADEF(SIZE) \
TC_INT_F_DEF(FW_INT_TYPE(SIZE), mxSingle, FW_INT_MAX(SIZE), FW_INT_MIN(SIZE), VO_FCN_TCNAME(FW_INT_TYPEN(SIZE), Single)); \
TC_INT_F_DEF(FW_INT_TYPE(SIZE), mxDouble, FW_INT_MAX(SIZE), FW_INT_MIN(SIZE), VO_FCN_TCNAME(FW_INT_TYPEN(SIZE), Double)); \


#if MSH_BITNESS==64
#define FW_TC_INT_CONV_DEF(SIZE) \
typedef FW_INT_TYPE(SIZE) (*FW_INT_TYPEC(SIZE))(void*, size_t); \
static FW_INT_TYPEC(SIZE) FW_INT_FCN_CTCNAME(SIZE)(mxClassID cid) \
{ \
	switch(cid) \
	{ \
		case(mxINT8_CLASS):   return FW_II_FCN_TCNAME(SIZE, 8); \
		case(mxUINT8_CLASS):  return FW_IU_FCN_TCNAME(SIZE, 8); \
		case(mxINT16_CLASS):  return FW_II_FCN_TCNAME(SIZE, 16); \
		case(mxUINT16_CLASS): return FW_IU_FCN_TCNAME(SIZE, 16); \
		case(mxINT32_CLASS):  return FW_II_FCN_TCNAME(SIZE, 32); \
		case(mxUINT32_CLASS): return FW_IU_FCN_TCNAME(SIZE, 32); \
		case(mxINT64_CLASS):  return FW_II_FCN_TCNAME(SIZE, 64); \
		case(mxUINT64_CLASS): return FW_IU_FCN_TCNAME(SIZE, 64); \
		case(mxSINGLE_CLASS): return VO_FCN_TCNAME(FW_INT_TYPEN(SIZE), Single); \
		case(mxDOUBLE_CLASS): return VO_FCN_TCNAME(FW_INT_TYPEN(SIZE), Double); \
		default: meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "TypeConversionError", "Invalid variable conversion."); \
	} \
	return 0; \
}
#else
#define FW_TC_INT_CONV_DEF(SIZE) \
typedef FW_INT_TYPE(SIZE) (*FW_INT_TYPEC(SIZE))(void*,size_t); \
static FW_INT_TYPEC(SIZE) FW_INT_FCN_CTCNAME(SIZE)(mxClassID cid) \
{ \
	switch(cid) \
	{ \
		case(mxINT8_CLASS):   return FW_II_FCN_TCNAME(SIZE, 8); \
		case(mxUINT8_CLASS):  return FW_IU_FCN_TCNAME(SIZE, 8); \
		case(mxINT16_CLASS):  return FW_II_FCN_TCNAME(SIZE, 16); \
		case(mxUINT16_CLASS): return FW_IU_FCN_TCNAME(SIZE, 16); \
		case(mxINT32_CLASS):  return FW_II_FCN_TCNAME(SIZE, 32); \
		case(mxUINT32_CLASS): return FW_IU_FCN_TCNAME(SIZE, 32); \
		case(mxSINGLE_CLASS): return VO_FCN_TCNAME(FW_INT_TYPEN(SIZE), Single); \
		case(mxDOUBLE_CLASS): return VO_FCN_TCNAME(FW_INT_TYPEN(SIZE), Double); \
		default: meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "TypeConversionError", "Invalid variable conversion."); \
	} \
	return 0; \
}
#endif

FW_TC_INT_U_METADEF(8, 8);
FW_TC_INT_L_METADEF(8, 16);
FW_TC_INT_L_METADEF(8, 32);
#if MSH_BITNESS==64
FW_TC_INT_L_METADEF(8, 64);
#endif
FW_TC_INT_F_METADEF(8);
FW_TC_INT_CONV_DEF(8);

FW_TC_INT_U_METADEF(16, 8);
FW_TC_INT_U_METADEF(16, 16);
FW_TC_INT_L_METADEF(16, 32);
#if MSH_BITNESS==64
FW_TC_INT_L_METADEF(16, 64);
#endif
FW_TC_INT_F_METADEF(16);
FW_TC_INT_CONV_DEF(16);

FW_TC_INT_U_METADEF(32, 8);
FW_TC_INT_U_METADEF(32, 16);
FW_TC_INT_U_METADEF(32, 32);
#if MSH_BITNESS==64
FW_TC_INT_L_METADEF(32, 64);
#endif
FW_TC_INT_F_METADEF(32);
FW_TC_INT_CONV_DEF(32);

#if MSH_BITNESS==64
FW_TC_INT_U_METADEF(64, 8);
FW_TC_INT_U_METADEF(64, 16);
FW_TC_INT_U_METADEF(64, 32);
FW_TC_INT_U_METADEF(64, 64);
FW_TC_INT_F_METADEF(64);
FW_TC_INT_CONV_DEF(64);
#endif

#define FW_UINT_TC_L_METADEF(SIZE1, SIZE2) \
TC_UU_L_DEF(FW_UINT_TYPE(SIZE1), FW_UINT_TYPE(SIZE2), FW_UINT_MAX(SIZE1), FW_UU_FCN_TCNAME(SIZE1, SIZE2)); \
TC_UI_L_DEF(FW_UINT_TYPE(SIZE1), FW_INT_TYPE(SIZE2), FW_UINT_MAX(SIZE1), FW_UI_FCN_TCNAME(SIZE1, SIZE2));

#define FW_UINT_TC_U_METADEF(SIZE1, SIZE2) \
TC_INT_U_DEF(FW_UINT_TYPE(SIZE1), FW_UINT_TYPE(SIZE2), FW_UU_FCN_TCNAME(SIZE1, SIZE2)); \
TC_UINT_U_DEF(FW_UINT_TYPE(SIZE1), FW_INT_TYPE(SIZE2), FW_UI_FCN_TCNAME(SIZE1, SIZE2)); \

#define FW_TC_UINT_F_METADEF(SIZE) \
TC_INT_F_DEF(FW_UINT_TYPE(SIZE), mxSingle, FW_UINT_MAX(SIZE), 0, VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE), Single)); \
TC_INT_F_DEF(FW_UINT_TYPE(SIZE), mxDouble, FW_UINT_MAX(SIZE), 0, VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE), Double)); \

#if MSH_BITNESS==64
#define FW_TC_UINT_CONV_DEF(SIZE) \
typedef FW_UINT_TYPE(SIZE) (*FW_UINT_TYPEC(SIZE))(void*, size_t); \
static FW_UINT_TYPEC(SIZE) FW_UINT_FCN_CTCNAME(SIZE)(mxClassID cid) \
{ \
	switch(cid) \
	{ \
		case(mxINT8_CLASS):   return FW_UI_FCN_TCNAME(SIZE, 8); \
		case(mxUINT8_CLASS):  return FW_UU_FCN_TCNAME(SIZE, 8); \
		case(mxINT16_CLASS):  return FW_UI_FCN_TCNAME(SIZE, 16); \
		case(mxUINT16_CLASS): return FW_UU_FCN_TCNAME(SIZE, 16); \
		case(mxINT32_CLASS):  return FW_UI_FCN_TCNAME(SIZE, 32); \
		case(mxUINT32_CLASS): return FW_UU_FCN_TCNAME(SIZE, 32); \
		case(mxINT64_CLASS):  return FW_UI_FCN_TCNAME(SIZE, 64); \
		case(mxUINT64_CLASS): return FW_UU_FCN_TCNAME(SIZE, 64); \
		case(mxSINGLE_CLASS): return VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE), Single); \
		case(mxDOUBLE_CLASS): return VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE), Double); \
		default: meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "TypeConversionError", "Invalid variable conversion."); \
	} \
	return 0; \
}
#else
#define FW_TC_INT_CONV_DEF(SIZE) \
typedef FW_UINT_TYPE(SIZE) (*FW_UINT_TYPEC(SIZE))(void*,size_t); \
static FW_UINT_TYPEC(SIZE) FW_UINT_FCN_CTCNAME(SIZE)(mxClassID cid) \
{ \
	switch(cid) \
	{ \
		case(mxINT8_CLASS):   return FW_UI_FCN_TCNAME(SIZE, 8); \
		case(mxUINT8_CLASS):  return FW_UU_FCN_TCNAME(SIZE, 8); \
		case(mxINT16_CLASS):  return FW_UI_FCN_TCNAME(SIZE, 16); \
		case(mxUINT16_CLASS): return FW_UU_FCN_TCNAME(SIZE, 16); \
		case(mxINT32_CLASS):  return FW_UI_FCN_TCNAME(SIZE, 32); \
		case(mxUINT32_CLASS): return FW_UU_FCN_TCNAME(SIZE, 32); \
		case(mxSINGLE_CLASS): return VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE), Single); \
		case(mxDOUBLE_CLASS): return VO_FCN_TCNAME(FW_UINT_TYPEN(SIZE), Double); \
		default: meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "TypeConversionError", "Invalid variable conversion."); \
	} \
	return 0; \
}
#endif

FW_UINT_TC_U_METADEF(8, 8);
FW_UINT_TC_L_METADEF(8, 16);
FW_UINT_TC_L_METADEF(8, 32);
#if MSH_BITNESS==64
FW_UINT_TC_L_METADEF(8, 64);
#endif
FW_TC_UINT_F_METADEF(8);
FW_TC_UINT_CONV_DEF(8);

FW_UINT_TC_U_METADEF(16, 8);
FW_UINT_TC_U_METADEF(16, 16);
FW_UINT_TC_L_METADEF(16, 32);
#if MSH_BITNESS==64
FW_UINT_TC_L_METADEF(16, 64);
#endif
FW_TC_UINT_F_METADEF(16);
FW_TC_UINT_CONV_DEF(16);

FW_UINT_TC_U_METADEF(32, 8);
FW_UINT_TC_U_METADEF(32, 16);
FW_UINT_TC_U_METADEF(32, 32);
#if MSH_BITNESS==64
FW_UINT_TC_L_METADEF(32, 64);
#endif
FW_TC_UINT_F_METADEF(32);
FW_TC_UINT_CONV_DEF(32);

#if MSH_BITNESS==64
FW_UINT_TC_U_METADEF(64, 8);
FW_UINT_TC_U_METADEF(64, 16);
FW_UINT_TC_U_METADEF(64, 32);
FW_UINT_TC_U_METADEF(64, 64);
FW_TC_UINT_F_METADEF(64);
FW_TC_UINT_CONV_DEF(64);
#endif

#define TC_FL_DEF(TYPE1, TYPE2, TCNAME) \
static TYPE1 TCNAME(void* v_in, size_t offset) \
{ \
	return (TYPE1)*((TYPE2*)v_in + offset); \
}

#define TC_FL_INT_METADEF(SIZE) \
TC_FL_DEF(mxSingle, FW_INT_TYPE(SIZE), VO_FCN_TCNAME(Single, FW_INT_TYPEN(SIZE))); \
TC_FL_DEF(mxSingle, FW_UINT_TYPE(SIZE), VO_FCN_TCNAME(Single, FW_UINT_TYPEN(SIZE))); \
TC_FL_DEF(mxDouble, FW_INT_TYPE(SIZE), VO_FCN_TCNAME(Double, FW_INT_TYPEN(SIZE))); \
TC_FL_DEF(mxDouble, FW_UINT_TYPE(SIZE), VO_FCN_TCNAME(Double, FW_UINT_TYPEN(SIZE)));

TC_FL_INT_METADEF(8);
TC_FL_INT_METADEF(16);
TC_FL_INT_METADEF(32);
#if MSH_BITNESS==64
TC_FL_INT_METADEF(64);
#endif

TC_FL_DEF(mxSingle, mxSingle, VO_FCN_TCNAME(Single, Single));
TC_FL_DEF(mxSingle, mxDouble, VO_FCN_TCNAME(Single, Double));
TC_FL_DEF(mxDouble, mxDouble, VO_FCN_TCNAME(Double, Double));
TC_FL_DEF(mxDouble, mxSingle, VO_FCN_TCNAME(Double, Single));

typedef mxSingle (*singleconv_T)(void*,size_t);
static singleconv_T VO_FCN_CTCNAME(Single)(mxClassID cid)
{
	switch(cid)
	{
		case(mxINT8_CLASS):   return VO_FCN_TCNAME(Single, FW_INT_TYPEN(  8  ));
		case(mxUINT8_CLASS):  return VO_FCN_TCNAME(Single, FW_UINT_TYPEN( 8  ));
		case(mxINT16_CLASS):  return VO_FCN_TCNAME(Single, FW_INT_TYPEN(  16 ));
		case(mxUINT16_CLASS): return VO_FCN_TCNAME(Single, FW_UINT_TYPEN( 16 ));
		case(mxINT32_CLASS):  return VO_FCN_TCNAME(Single, FW_INT_TYPEN(  32 ));
		case(mxUINT32_CLASS): return VO_FCN_TCNAME(Single, FW_UINT_TYPEN( 32 ));
#if MSH_BITNESS==64
		case(mxINT64_CLASS):  return VO_FCN_TCNAME(Single, FW_INT_TYPEN(  64 ));
		case(mxUINT64_CLASS): return VO_FCN_TCNAME(Single, FW_UINT_TYPEN( 64 ));

#endif
		case(mxSINGLE_CLASS): return VO_FCN_TCNAME(Single, Single);
		case(mxDOUBLE_CLASS): return VO_FCN_TCNAME(Single, Double);
		default: meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "TypeConversionError", "Invalid variable conversion.");
	}
	return 0;
}


typedef mxDouble (*doubleconv_T)(void*,size_t);
static doubleconv_T VO_FCN_CTCNAME(Double)(mxClassID cid)
{
	switch(cid)
	{
		case(mxINT8_CLASS):   return VO_FCN_TCNAME(Double, FW_INT_TYPEN(  8  ));
		case(mxUINT8_CLASS):  return VO_FCN_TCNAME(Double, FW_UINT_TYPEN( 8  ));
		case(mxINT16_CLASS):  return VO_FCN_TCNAME(Double, FW_INT_TYPEN(  16 ));
		case(mxUINT16_CLASS): return VO_FCN_TCNAME(Double, FW_UINT_TYPEN( 16 ));
		case(mxINT32_CLASS):  return VO_FCN_TCNAME(Double, FW_INT_TYPEN(  32 ));
		case(mxUINT32_CLASS): return VO_FCN_TCNAME(Double, FW_UINT_TYPEN( 32 ));
#if MSH_BITNESS==64
		case(mxINT64_CLASS):  return VO_FCN_TCNAME(Double, FW_INT_TYPEN(  64 ));
		case(mxUINT64_CLASS): return VO_FCN_TCNAME(Double, FW_UINT_TYPEN( 64 ));

#endif
		case(mxSINGLE_CLASS): return VO_FCN_TCNAME(Double, Single);
		case(mxDOUBLE_CLASS): return VO_FCN_TCNAME(Double, Double);
		default: meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "TypeConversionError", "Invalid variable conversion.");
	}
	return 0;
}



#define UNARY_OP_RUNNER(RNAME, NAME, TYPE_ACCUM, CASNAME)   \
static void RNAME(void* v_accum, long opts) \
{                                                  \
	size_t i;                                     \
	TYPE_ACCUM new_val, old_val;						                \
	WideInput_T(TYPE_ACCUM)* wide_accum = v_accum;					 \
	if(opts & MSH_USE_ATOMIC_OPS) \
	{ \
		for(i = 0; i < wide_accum->num_elems; i++)                \
		{                                             \
			do                                                      \
			{                                                       \
				old_val = wide_accum->input[i];                    \
				new_val = NAME(old_val);          \
			} while(CASNAME(wide_accum->input + i, old_val, new_val) != old_val); \
		}                                             \
	} \
	else \
	{ \
		for(i = 0; i < wide_accum->num_elems; i++)                \
		{                                             \
			wide_accum->input[i] = NAME(wide_accum->input[i]);               \
		}                                             \
	} \
}

#define UNARY_OP_RUNNER_METADEF(OP, TYPE, TYPEN) \
UNARY_OP_RUNNER(VO_FCN_RNAME(OP, TYPEN), VO_FCN_FNAME(OP, TYPEN), TYPE, VO_FCN_CASNAME(TYPEN))

#define BINARY_OP_RUNNER(RNAME, FNAME, CASNAME, CTCNAME, OP, TYPE, TYPEN, TYPEC)           \
static void RNAME(void* v_accum, void* v_in, long opts) \
{ \
	size_t i; \
	TYPEC in_conv; \
	TYPE new_val, old_val, static_val; \
	double static_val_dbl, tmp; \
	WideInput_T(TYPE)* wide_accum = v_accum; \
	WideInput_T(TYPE)* wide_in    = v_in; \
	if(wide_in->num_elems == 1) \
	{ \
		if(wide_accum->mxtype != wide_in->mxtype) \
		{ \
			if(wide_in->mxtype == mxDOUBLE_CLASS) \
			{ \
				static_val_dbl = *(double*)wide_in->input; \
				if(opts & MSH_USE_ATOMIC_OPS) \
				{ \
					for(i = 0; i < wide_accum->num_elems; i++) \
					{ \
						do \
						{ \
							old_val = wide_accum->input[i]; \
							tmp = VO_FCN_FNAME(OP, Double)((double)old_val, static_val_dbl); \
							new_val = VO_FCN_TCNAME(TYPEN, Double)(&tmp, 0); \
						} while(CASNAME(wide_accum->input + i, old_val, new_val) != old_val); \
					} \
				} \
				else \
				{ \
					for(i = 0; i < wide_accum->num_elems; i++) \
					{ \
						tmp = VO_FCN_FNAME(OP, Double)((double)wide_accum->input[i], static_val_dbl); \
						wide_accum->input[i] = VO_FCN_TCNAME(TYPEN, Double)(&tmp, 0); \
					} \
				} \
			} \
			else \
			{ \
				in_conv = CTCNAME(wide_in->mxtype); \
				static_val = in_conv(wide_in->input, 0); \
				if(opts & MSH_USE_ATOMIC_OPS) \
				{ \
					for(i = 0; i < wide_accum->num_elems; i++) \
					{ \
						do \
						{ \
							old_val = wide_accum->input[i]; \
							new_val = FNAME(old_val, static_val); \
						} while(CASNAME(wide_accum->input + i, old_val, new_val) != old_val); \
					} \
				} \
				else \
				{ \
					for(i = 0; i < wide_accum->num_elems; i++) \
					{ \
						wide_accum->input[i] = FNAME(wide_accum->input[i], static_val); \
					} \
				} \
			} \
		} \
		else \
		{ \
			if(opts & MSH_USE_ATOMIC_OPS) \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					do \
					{ \
						old_val = wide_accum->input[i]; \
						new_val = FNAME(old_val, *wide_in->input); \
					} while(CASNAME(wide_accum->input + i, old_val, new_val) != old_val); \
				} \
			} \
			else \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					wide_accum->input[i] = FNAME(wide_accum->input[i], *wide_in->input); \
				} \
			} \
		} \
	} \
	else \
	{ \
		if(wide_accum->mxtype != wide_in->mxtype) \
		{ \
			in_conv = CTCNAME(wide_in->mxtype); \
			if(opts & MSH_USE_ATOMIC_OPS) \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					do \
					{ \
						old_val = wide_accum->input[i]; \
						new_val = FNAME(old_val, in_conv(wide_in->input, i)); \
					} while(CASNAME(wide_accum->input + i, old_val, new_val) != old_val); \
				} \
			} \
			else \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					wide_accum->input[i] = FNAME(wide_accum->input[i], in_conv(wide_in->input, i)); \
				} \
			} \
		} \
		else \
		{ \
			if(opts & MSH_USE_ATOMIC_OPS) \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					do \
					{ \
						old_val = wide_accum->input[i]; \
						new_val = FNAME(old_val, wide_in->input[i]); \
					} while(CASNAME(wide_accum->input + i, old_val, new_val) != old_val); \
				} \
			} \
			else \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					wide_accum->input[i] = FNAME(wide_accum->input[i], wide_in->input[i]); \
				} \
			} \
		} \
	} \
}

#define BINARY_OP_RUNNER_METADEF(OP, TYPE, TYPEN, TYPEC) \
BINARY_OP_RUNNER(VO_FCN_RNAME(OP,TYPEN), VO_FCN_FNAME(OP,TYPEN), VO_FCN_CASNAME(TYPEN), VO_FCN_CTCNAME(TYPEN), OP, TYPE, TYPEN, TYPEC)

/** Signed integer arithmetic
 * We assume here that ints are two's complement, and
 * that right shifts of signed negative integers preserve
 * the sign bit (thus go to -1). This should be tested
 * before compilation.
 */

/** SIGN FUNCTIONS **/
#define SGNBIT_INT_DEF(NAME, TYPE, UTYPE, SIZEM1) \
static TYPE NAME(TYPE in)                         \
{                                                 \
	return (UTYPE)in >> (SIZEM1);                  \
}

#define SGNBIT_INT_METADEF(SIZE) SGNBIT_INT_DEF(FW_INT_FCN_FNAME(Sgn, SIZE), FW_INT_TYPE(SIZE), FW_UINT_TYPE(SIZE), (SIZE)-1);

SGNBIT_INT_METADEF(8);
SGNBIT_INT_METADEF(16);
SGNBIT_INT_METADEF(32);
#if MSH_BITNESS==64
SGNBIT_INT_METADEF(64);
#endif

/** SIGNED ABSOLUTE VALUES **/

#define ABS_INT_DEF(NAME, TYPE, SIZEM1, MAX_VAL) \
static TYPE NAME(TYPE in)                        \
{                                                \
	TYPE out;                                   \
	TYPE mask = in >> (SIZEM1);                   \
	out = (in ^ mask) - mask;                   \
	if(out < 0)                                 \
	{                                           \
		return MAX_VAL;                        \
	}                                           \
	return out;                                 \
}

#define ABS_INT_METADEF(SIZE)                                                           \
ABS_INT_DEF(FW_INT_FCN_FNAME(Abs, SIZE), FW_INT_TYPE(SIZE), (SIZE)-1, FW_INT_MAX(SIZE)); \
UNARY_OP_RUNNER_METADEF(Abs, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE));

ABS_INT_METADEF(8);
ABS_INT_METADEF(16);
ABS_INT_METADEF(32);
#if MSH_BITNESS==64
ABS_INT_METADEF(64);
#endif

/** SIGNED ADDITION **/
#define ADD_INT_DEF(NAME, TYPE, UTYPE, SIZEM1, MAX_VAL) \
static TYPE NAME(TYPE augend, TYPE addend)              \
{                                                       \
	TYPE uadd = (UTYPE)augend + (UTYPE)addend;         \
	TYPE flip1 = uadd ^ augend;                        \
	TYPE flip2 = uadd ^ addend;                        \
	if((flip1 & flip2) < 0)                            \
	{                                                  \
		return (TYPE)(MAX_VAL) + ((UTYPE)~uadd >> (SIZEM1));    \
	}                                                  \
	return uadd;                                       \
}

#define ADD_INT_METADEF(SIZE)                                                                               \
ADD_INT_DEF(FW_INT_FCN_FNAME(Add, SIZE), FW_INT_TYPE(SIZE), FW_UINT_TYPE(SIZE), (SIZE)-1, FW_INT_MAX(SIZE)); \
BINARY_OP_RUNNER_METADEF(Add, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

ADD_INT_METADEF(8);
ADD_INT_METADEF(16);
ADD_INT_METADEF(32);
#if MSH_BITNESS==64
ADD_INT_METADEF(64);
#endif

/** SIGNED SUBTRACTION **/

#define SUB_INT_DEF(NAME, TYPE, UTYPE, SIZEM1, MAX_VAL) \
static TYPE NAME(TYPE minuend, TYPE subtrahend)         \
{                                                       \
	TYPE usub = (UTYPE)minuend - (UTYPE)subtrahend;    \
	TYPE flip1 = usub ^ minuend;                       \
	TYPE flip2 = usub ^ ~subtrahend;                   \
	if((flip1 & flip2) < 0)                            \
	{                                                  \
		return (TYPE)(MAX_VAL)  + ((UTYPE)~usub >> (SIZEM1));    \
	}                                                  \
	return usub;                                       \
}

#define SUB_INT_METADEF(SIZE)                                                                               \
SUB_INT_DEF(FW_INT_FCN_FNAME(Sub, SIZE), FW_INT_TYPE(SIZE), FW_UINT_TYPE(SIZE), (SIZE)-1, FW_INT_MAX(SIZE)); \
BINARY_OP_RUNNER_METADEF(Sub, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

SUB_INT_METADEF(8);
SUB_INT_METADEF(16);
SUB_INT_METADEF(32);
#if MSH_BITNESS==64
SUB_INT_METADEF(64);
#endif

/** SIGNED MULTIPLICATION **/

#define MUL_INT_DEF(NAME, TYPE, PTYPE, MAX_VAL, MIN_VAL) \
static TYPE NAME(TYPE mul1, TYPE mul2)                   \
{                                                        \
	PTYPE promoted_mul = (PTYPE)mul1 * (PTYPE)mul2;     \
	if(promoted_mul > (MAX_VAL))                          \
	{                                                   \
		return MAX_VAL;                                \
	}                                                   \
	else if(promoted_mul < (MIN_VAL))                     \
	{                                                   \
		return MIN_VAL;                                \
	}                                                   \
	return (TYPE)promoted_mul;                          \
}

#define MUL_INT_METADEF(SIZE, PSIZE)                                                                                \
MUL_INT_DEF(FW_INT_FCN_FNAME(Mul, SIZE), FW_INT_TYPE(SIZE), FW_INT_TYPE(PSIZE), FW_INT_MAX(SIZE), FW_INT_MIN(SIZE)); \
BINARY_OP_RUNNER_METADEF(Mul, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

MUL_INT_METADEF(8, 16);
MUL_INT_METADEF(16, 32);
#if MSH_BITNESS==32

static int32_T msh_MulInt32(int32_T m1, int32_T m2)
{
	uint32_T uret;
	uint32_T m1a_l16, m2a_l16;
	uint32_T m1au_m2al, m1al_m2al, m1al_m2au;
	
	int      ret_is_pos   = ((m1 < 0) == (m2 < 0));
	
	uint32_T m1_abs       = (uint32_T)FW_INT_FCN_FNAME(Abs, 32)(m1);
	uint32_T m2_abs       = (uint32_T)FW_INT_FCN_FNAME(Abs, 32)(m2);
	
	uint32_T m1a_u16      = m1_abs >> 16;
	uint32_T m2a_u16      = m2_abs >> 16;
	
	if(m1a_u16)
	{
		if(m2a_u16)
		{
			return ret_is_pos? INT32_MAX : INT32_MIN;
		}
		
		m2a_l16 = (uint16_T)m2_abs;
		m1au_m2al = m1a_u16 * m2a_l16;
		
		if(m1au_m2al >> 16)
		{
			return ret_is_pos? INT32_MAX : INT32_MIN;
		}
		
		m1au_m2al <<= 16;
		m1a_l16 = (uint16_T)m1_abs;
		m1al_m2al = m1a_l16 * m2a_l16;
		uret = m1au_m2al + m1al_m2al;
		
		if(uret < m1au_m2al)
		{
			return ret_is_pos? INT32_MAX : INT32_MIN;
		}
		
	}
	else if(m2a_u16)
	{
		/* do the opposite of the previous branch */
		m1a_l16 = (uint16_T)m1_abs;
		m1al_m2au = m1a_l16 * m2a_u16;
		
		if(m1al_m2au >> 16)
		{
			return ret_is_pos? INT32_MAX : INT32_MIN;
		}
		
		m1al_m2au <<= 16;
		m2a_l16 = (uint16_T)m2_abs;
		m1al_m2al = m1a_l16 * m2a_l16;
		uret = m1al_m2al + m1al_m2au;
		
		if(uret < m1al_m2au)
		{
			return ret_is_pos? INT32_MAX : INT32_MIN;
		}
	}
	else
	{
		
		uret = m1_abs * m2_abs;
	}
	
	if(ret_is_pos)
	{
		if(uret > (uint32_T)INT32_MAX)
		{
			return INT32_MAX;
		}
		return (int32_T)uret;
	}
	else
	{
		if(uret > (uint32_T)INT32_MIN)
		{
			return INT32_MIN;
		}
		
		/* note: this works for uret == INT32_MIN because negation wraps to itself */
		return -(int32_T)uret;
	}
	
}

BINARY_OP_RUNNER_METADEF(Mul, FW_INT_TYPE(32), FW_INT_TYPEN(32), FW_INT_TYPEC(32));

#else

MUL_INT_METADEF(32, 64);

static int64_T msh_MulInt64(int64_T m1, int64_T m2)
{
	uint64_T uret;
	uint64_T m1a_l32, m2a_l32;
	uint64_T m1au_m2al, m1al_m2al, m1al_m2au;
	
	int      ret_is_pos   = ((m1 < 0) == (m2 < 0));
	
	uint64_T m1_abs       = (uint64_T)FW_INT_FCN_FNAME(Abs, 64)(m1);
	uint64_T m2_abs       = (uint64_T)FW_INT_FCN_FNAME(Abs, 64)(m2);
	
	uint64_T m1a_u32      = m1_abs >> 32;
	uint64_T m2a_u32      = m2_abs >> 32;
	
	if(m1a_u32)
	{
		if(m2a_u32)
		{
			return ret_is_pos? INT64_MAX : INT64_MIN;
		}
		
		m2a_l32 = (uint32_T)m2_abs;
		m1au_m2al = m1a_u32 * m2a_l32;
		
		if(m1au_m2al >> 32)
		{
			return ret_is_pos? INT64_MAX : INT64_MIN;
		}
		
		m1au_m2al <<= 32;
		m1a_l32 = (uint32_T)m1_abs;
		m1al_m2al = m1a_l32 * m2a_l32;
		uret = m1au_m2al + m1al_m2al;
		
		if(uret < m1au_m2al)
		{
			return ret_is_pos? INT64_MAX : INT64_MIN;
		}
		
	}
	else if(m2a_u32)
	{
		/* do the opposite of the previous branch */
		m1a_l32 = (uint32_T)m1_abs;
		m1al_m2au = m1a_l32 * m2a_u32;
		
		if(m1al_m2au >> 32)
		{
			return ret_is_pos? INT64_MAX : INT64_MIN;
		}
		
		m1al_m2au <<= 32;
		m2a_l32 = (uint32_T)m2_abs;
		m1al_m2al = m1a_l32 * m2a_l32;
		uret = m1al_m2al + m1al_m2au;
		
		if(uret < m1al_m2au)
		{
			return ret_is_pos? INT64_MAX : INT64_MIN;
		}
	}
	else
	{
		
		uret = m1_abs * m2_abs;
	}
	
	if(ret_is_pos)
	{
		if(uret > (uint64_T)INT64_MAX)
		{
			return INT64_MAX;
		}
		return (int64_T)uret;
	}
	else
	{
		if(uret > (uint64_T)INT64_MIN)
		{
			return INT64_MIN;
		}
		
		/* note: this works for uret == INT64_MIN because negation wraps to itself */
		return -(int64_T)uret;
	}
	
}

BINARY_OP_RUNNER_METADEF(Mul, FW_INT_TYPE(64), FW_INT_TYPEN(64), FW_INT_TYPEC(64));

#endif


/** SIGNED DIVISION (ROUNDED) **/

#define DIV_INT_DEF(NAME, SIZE, TYPE, MAX_VAL, MIN_VAL)                          \
static TYPE NAME(TYPE numer, TYPE denom)                                         \
{                                                                                \
	TYPE ret, overflow_check;                                                   \
	if(denom == 0)                                                              \
	{                                                                           \
		if(numer > 0)                                                          \
		{                                                                      \
			return MAX_VAL;                                                   \
		}                                                                      \
		else if(numer < 0)                                                     \
		{                                                                      \
			return MIN_VAL;                                                   \
		}                                                                      \
		else                                                                   \
		{                                                                      \
			return 0;                                                         \
		}                                                                      \
	}                                                                           \
	else if(denom < 0)                                                          \
	{                                                                           \
		if(denom == -1 && numer == (MIN_VAL))                                    \
		{                                                                      \
			return MAX_VAL;                                                   \
		}                                                                      \
		else                                                                   \
		{                                                                      \
			ret = numer/denom;                                                \
			overflow_check = -FW_INT_FCN_FNAME(Abs, SIZE)(numer % denom);      \
			if(overflow_check <= denom - overflow_check)                      \
			{                                                                 \
				return ret - (1 - (FW_INT_FCN_FNAME(Sgn, SIZE)(numer) << 1)); \
			}                                                                 \
		}                                                                      \
	}                                                                           \
	else                                                                        \
	{                                                                           \
		ret = numer/denom;                                                     \
		overflow_check = FW_INT_FCN_FNAME(Abs, SIZE)(numer % denom);            \
		if(overflow_check >= denom - overflow_check)                           \
		{                                                                      \
			return ret + (1 - (FW_INT_FCN_FNAME(Sgn, SIZE)(numer) << 1));      \
		}                                                                      \
	}                                                                           \
	return ret;                                                                 \
}

#define DIV_INT_METADEF(SIZE)                                                                         \
DIV_INT_DEF(FW_INT_FCN_FNAME(Div, SIZE), SIZE, FW_INT_TYPE(SIZE), FW_INT_MAX(SIZE), FW_INT_MIN(SIZE)); \
BINARY_OP_RUNNER_METADEF(Div, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

DIV_INT_METADEF(8);
DIV_INT_METADEF(16);
DIV_INT_METADEF(32);
#if MSH_BITNESS==64
DIV_INT_METADEF(64);
#endif

/** SIGNED REMAINDER **/

#define REM_INT_DEF(NAME, TYPE)              \
static TYPE NAME(TYPE numer, TYPE denom)     \
{                                            \
	return (denom != 0)? (numer%denom) : 0; \
}

#define REM_INT_METADEF(SIZE)                               \
REM_INT_DEF(FW_INT_FCN_FNAME(Rem, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(Rem, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

REM_INT_METADEF(8);
REM_INT_METADEF(16);
REM_INT_METADEF(32);
#if MSH_BITNESS==64
REM_INT_METADEF(64);
#endif

/** SIGNED MODULUS **/

#define MOD_INT_DEF(NAME, TYPE)                                                     \
static TYPE NAME(TYPE numer, TYPE denom)                                            \
{                                                                                   \
	TYPE rem;                                                                      \
	if(denom != 0)                                                                 \
	{                                                                              \
		rem = numer%denom;                                                        \
		return (rem != 0)? (((rem < 0) != (denom < 0))? (rem + denom) : rem) : 0; \
	}                                                                              \
	return numer;                                                                  \
}

#define MOD_INT_METADEF(SIZE)                               \
MOD_INT_DEF(FW_INT_FCN_FNAME(Mod, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(Mod, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

MOD_INT_METADEF(8);
MOD_INT_METADEF(16);
MOD_INT_METADEF(32);
#if MSH_BITNESS==64
MOD_INT_METADEF(64);
#endif

/** SIGNED NEGATION **/

#define NEG_INT_DEF(NAME, TYPE, MAX_VAL, MIN_VAL) \
static TYPE NAME(TYPE in)                         \
{                                                 \
	if(in == (MIN_VAL))                            \
	{                                            \
		return MAX_VAL;                         \
	}                                            \
	return -in;                                  \
}

#define NEG_INT_METADEF(SIZE)                                                                   \
NEG_INT_DEF(FW_INT_FCN_FNAME(Neg, SIZE), FW_INT_TYPE(SIZE), FW_INT_MAX(SIZE), FW_INT_MIN(SIZE)); \
UNARY_OP_RUNNER_METADEF(Neg, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE));

NEG_INT_METADEF(8);
NEG_INT_METADEF(16);
NEG_INT_METADEF(32);
#if MSH_BITNESS==64
NEG_INT_METADEF(64);
#endif

/** SIGNED RIGHT SHIFT **/

#define ARS_INT_DEF(NAME, TYPE)           \
static TYPE NAME(TYPE in, TYPE num_shift) \
{                                         \
	return in >> num_shift;              \
}

#define ARS_INT_METADEF(SIZE)                               \
ARS_INT_DEF(FW_INT_FCN_FNAME(ARS, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(ARS, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

ARS_INT_METADEF(8);
ARS_INT_METADEF(16);
ARS_INT_METADEF(32);
#if MSH_BITNESS==64
ARS_INT_METADEF(64);
#endif

/** SIGNED LEFT SHIFT **/

#define ALS_INT_DEF(NAME, TYPE)           \
static TYPE NAME(TYPE in, TYPE num_shift) \
{                                         \
	return in << num_shift;              \
}

#define ALS_INT_METADEF(SIZE)                               \
ALS_INT_DEF(FW_INT_FCN_FNAME(ALS, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(ALS, FW_INT_TYPE(SIZE), FW_INT_TYPEN(SIZE), FW_INT_TYPEC(SIZE));

ALS_INT_METADEF(8);
ALS_INT_METADEF(16);
ALS_INT_METADEF(32);
#if MSH_BITNESS==64
ALS_INT_METADEF(64);
#endif

/** Unsigned integer arithmetic
 *  The functions here are pretty straightforward, except
 *  for uint64 multiplication.
 */

/** UNSIGNED ABSOLUTE VALUES **/

#define ABS_UINT_DEF(NAME, TYPE) \
static TYPE NAME(TYPE in)        \
{                                \
	return in;                  \
}

#define ABS_UINT_METADEF(SIZE)                                 \
ABS_UINT_DEF(FW_UINT_FCN_FNAME(Abs, SIZE), FW_UINT_TYPE(SIZE)); \
UNARY_OP_RUNNER_METADEF(Abs, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE));

ABS_UINT_METADEF(8);
ABS_UINT_METADEF(16);
ABS_UINT_METADEF(32);
#if MSH_BITNESS==64
ABS_UINT_METADEF(64);
#endif

/** UNSIGNED ADDITION **/

#define ADD_UINT_DEF(NAME, TYPE, MAX_VAL)  \
static TYPE NAME(TYPE augend, TYPE addend) \
{                                          \
	TYPE ret = augend + addend;           \
	return (ret < augend)? (MAX_VAL) : ret; \
}

#define ADD_UINT_METADEF(SIZE)                                                                               \
ADD_UINT_DEF(FW_UINT_FCN_FNAME(Add, SIZE), FW_UINT_TYPE(SIZE), FW_UINT_MAX(SIZE)); \
BINARY_OP_RUNNER_METADEF(Add, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

ADD_UINT_METADEF(8);
ADD_UINT_METADEF(16);
ADD_UINT_METADEF(32);
#if MSH_BITNESS==64
ADD_UINT_METADEF(64);
#endif

/** UNSIGNED SUBTRACTION **/

#define SUB_UINT_DEF(NAME, TYPE)                \
static TYPE NAME(TYPE minuend, TYPE subtrahend) \
{                                               \
	TYPE ret = minuend - subtrahend;           \
	return (ret > minuend)? 0 : ret;      \
}

#define SUB_UINT_METADEF(SIZE)                                                                               \
SUB_UINT_DEF(FW_UINT_FCN_FNAME(Sub, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(Sub, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

SUB_UINT_METADEF(8);
SUB_UINT_METADEF(16);
SUB_UINT_METADEF(32);
#if MSH_BITNESS==64
SUB_UINT_METADEF(64);
#endif

/** UNSIGNED MULTIPLICATION **/

#define MUL_UINT_DEF(NAME, TYPE, PTYPE, MAX_VAL)    \
static TYPE NAME(TYPE mul1, TYPE mul2)               \
{                                                    \
	PTYPE promoted_mul = (PTYPE)mul1 * (PTYPE)mul2; \
	if(promoted_mul > (MAX_VAL))                      \
	{                                               \
		return MAX_VAL;                            \
	}                                               \
	return (TYPE)promoted_mul;                      \
}

#define MUL_UINT_METADEF(SIZE, PSIZE)                                                                  \
MUL_UINT_DEF(FW_UINT_FCN_FNAME(Mul, SIZE), FW_UINT_TYPE(SIZE), FW_UINT_TYPE(PSIZE), FW_UINT_MAX(SIZE)); \
BINARY_OP_RUNNER_METADEF(Mul, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

MUL_UINT_METADEF(8, 16);
MUL_UINT_METADEF(16, 32);
#if MSH_BITNESS==32

static uint32_T msh_MulUInt32(uint32_T m1, uint32_T m2)
{
	
	uint32_T m1_l16, m2_l16;
	uint32_T m1u_m2l, m1l_m2l, m1l_m2u;
	
	uint32_T m1_u16      = m1 >> 16;
	uint32_T m2_u16      = m2 >> 16;
	
	if(m1_u16)
	{
		if(m2_u16)
		{
			return UINT32_MAX;
		}
		
		m2_l16 = (uint16_T)m2;
		m1u_m2l = m1_u16 * m2_l16;
		
		if(m1u_m2l >> 16)
		{
			return UINT32_MAX;
		}
		
		m1u_m2l <<= 16;
		m1_l16 = (uint16_T)m1;
		m1l_m2l = m1_l16 * m2_l16;
		
		return FW_UINT_FCN_FNAME(Add, 32)(m1u_m2l, m1l_m2l);
		
	}
	else if(m2_u16)
	{
		/* do the opposite of the previous branch */
		m1_l16 = (uint16_T)m1;
		m1l_m2u = m1_l16 * m2_u16;
		
		if(m1l_m2u >> 16)
		{
			return UINT32_MAX;
		}
		
		m1l_m2u <<= 16;
		m2_l16 = (uint16_T)m2;
		m1l_m2l = m1_l16 * m2_l16;
		
		return FW_UINT_FCN_FNAME(Add, 32)(m1l_m2l, m1l_m2u);
		
	}
	else
	{
		return m1 * m2;
	}
	
}

BINARY_OP_RUNNER_METADEF(Mul, FW_UINT_TYPE(32), FW_UINT_TYPEN(32), FW_UINT_TYPEC(32));

#else
MUL_UINT_METADEF(32, 64);

static uint64_T msh_MulUInt64(uint64_T m1, uint64_T m2)
{
	
	uint64_T m1_l32, m2_l32;
	uint64_T m1u_m2l, m1l_m2l, m1l_m2u;
	
	uint64_T m1_u32      = m1 >> 32;
	uint64_T m2_u32      = m2 >> 32;
	
	if(m1_u32)
	{
		if(m2_u32)
		{
			return UINT64_MAX;
		}
		
		m2_l32 = (uint32_T)m2;
		m1u_m2l = m1_u32 * m2_l32;
		
		if(m1u_m2l >> 32)
		{
			return UINT64_MAX;
		}
		
		m1u_m2l <<= 32;
		m1_l32 = (uint32_T)m1;
		m1l_m2l = m1_l32 * m2_l32;
		
		return FW_UINT_FCN_FNAME(Add, 64)(m1u_m2l, m1l_m2l);
		
	}
	else if(m2_u32)
	{
		/* do the opposite of the previous branch */
		m1_l32 = (uint32_T)m1;
		m1l_m2u = m1_l32 * m2_u32;
		
		if(m1l_m2u >> 32)
		{
			return UINT64_MAX;
		}
		
		m1l_m2u <<= 32;
		m2_l32 = (uint32_T)m2;
		m1l_m2l = m1_l32 * m2_l32;
		
		return FW_UINT_FCN_FNAME(Add, 64)(m1l_m2l, m1l_m2u);
		
	}
	else
	{
		return m1 * m2;
	}
	
}

BINARY_OP_RUNNER_METADEF(Mul, FW_UINT_TYPE(64), FW_UINT_TYPEN(64), FW_UINT_TYPEC(64));

#endif

/** UNSIGNED DIVISION (ROUNDED) **/

#define DIV_UINT_DEF(NAME, TYPE, MAX_VAL)        \
static TYPE NAME(TYPE numer, TYPE denom)         \
{                                                \
	TYPE ret, rem;                              \
	if(denom == 0)                              \
	{                                           \
		return numer? (MAX_VAL) : 0;             \
	}                                           \
	ret = numer/denom;                          \
	rem = numer%denom;                          \
	return (rem >= denom - rem)? ret + 1 : ret; \
}

#define DIV_UINT_METADEF(SIZE)                                                                         \
DIV_UINT_DEF(FW_UINT_FCN_FNAME(Div, SIZE), FW_UINT_TYPE(SIZE), FW_UINT_MAX(SIZE)); \
BINARY_OP_RUNNER_METADEF(Div, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

DIV_UINT_METADEF(8);
DIV_UINT_METADEF(16);
DIV_UINT_METADEF(32);
#if MSH_BITNESS==64
DIV_UINT_METADEF(64);
#endif

/** UNSIGNED REMAINDER **/

#define REM_UINT_DEF(NAME, TYPE)              \
static TYPE NAME(TYPE numer, TYPE denom)     \
{                                            \
	return (denom != 0)? (numer%denom) : 0; \
}

#define REM_UINT_METADEF(SIZE)                               \
REM_UINT_DEF(FW_UINT_FCN_FNAME(Rem, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(Rem, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

REM_UINT_METADEF(8);
REM_UINT_METADEF(16);
REM_UINT_METADEF(32);
#if MSH_BITNESS==64
REM_UINT_METADEF(64);
#endif

/** UNSIGNED MODULUS **/

#define MOD_UINT_DEF(NAME, TYPE)                 \
static TYPE NAME(TYPE numer, TYPE denom)         \
{                                                \
	return (denom != 0)? (numer%denom) : numer; \
}

#define MOD_UINT_METADEF(SIZE)                                 \
MOD_UINT_DEF(FW_UINT_FCN_FNAME(Mod, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(Mod, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

MOD_UINT_METADEF(8);
MOD_UINT_METADEF(16);
MOD_UINT_METADEF(32);
#if MSH_BITNESS==64
MOD_UINT_METADEF(64);
#endif


/** UNSIGNED NEGATION **/

#define NEG_UINT_DEF(NAME, TYPE) \
static TYPE NAME(TYPE in)        \
{                                \
	return 0;                   \
}

#define NEG_UINT_METADEF(SIZE)                                 \
NEG_UINT_DEF(FW_UINT_FCN_FNAME(Neg, SIZE), FW_UINT_TYPE(SIZE)); \
UNARY_OP_RUNNER_METADEF(Neg, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE));

NEG_UINT_METADEF(8);
NEG_UINT_METADEF(16);
NEG_UINT_METADEF(32);
#if MSH_BITNESS==64
NEG_UINT_METADEF(64);
#endif

/** UNSIGNED RIGHT SHIFT **/

#define ARS_UINT_DEF(NAME, TYPE)          \
static TYPE NAME(TYPE in, TYPE num_shift) \
{                                         \
	return in >> num_shift;              \
}

#define ARS_UINT_METADEF(SIZE)                                 \
ARS_UINT_DEF(FW_UINT_FCN_FNAME(ARS, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(ARS, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

ARS_UINT_METADEF(8);
ARS_UINT_METADEF(16);
ARS_UINT_METADEF(32);
#if MSH_BITNESS==64
ARS_UINT_METADEF(64);
#endif

/** UNSIGNED LEFT SHIFT **/

#define ALS_UINT_DEF(NAME, TYPE)          \
static TYPE NAME(TYPE in, TYPE num_shift) \
{                                         \
	return in << num_shift;              \
}

#define ALS_UINT_METADEF(SIZE)                                 \
ALS_UINT_DEF(FW_UINT_FCN_FNAME(ALS, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_OP_RUNNER_METADEF(ALS, FW_UINT_TYPE(SIZE), FW_UINT_TYPEN(SIZE), FW_UINT_TYPEC(SIZE));

ALS_UINT_METADEF(8);
ALS_UINT_METADEF(16);
ALS_UINT_METADEF(32);
#if MSH_BITNESS==64
ALS_UINT_METADEF(64);
#endif


/** Floating point arithmetic
 * These are just derived directly from stdlib,
 * so no need for the single value subfunctions.
 */
 
/** mxSingle **/

static mxSingle msh_AbsSingle(mxSingle accum)
{
#if __STDC_VERSION__ >= 199901L
	return (mxSingle)fabsf((float)accum);
#else
	return (mxSingle)fabs((double)accum);
#endif
}
UNARY_OP_RUNNER_METADEF(Abs, mxSingle, Single);


static mxSingle msh_AddSingle(mxSingle accum, mxSingle in)
{
	return accum + in;
}
BINARY_OP_RUNNER_METADEF(Add, mxSingle, Single, singleconv_T);


static mxSingle msh_SubSingle(mxSingle accum, mxSingle in)
{
	return accum - in;
}
BINARY_OP_RUNNER_METADEF(Sub, mxSingle, Single, singleconv_T);


static mxSingle msh_MulSingle(mxSingle accum, mxSingle in)
{
	return accum * in;
}
BINARY_OP_RUNNER_METADEF(Mul, mxSingle, Single, singleconv_T);


static mxSingle msh_DivSingle(mxSingle accum, mxSingle in)
{
	return accum / in;
}
BINARY_OP_RUNNER_METADEF(Div, mxSingle, Single, singleconv_T);


static mxSingle msh_RemSingle(mxSingle accum, mxSingle in)
{
#if __STDC_VERSION__ >= 199901L
	return (mxSingle)fmodf((float)accum, (float)in);
#else
	return (mxSingle)fmod((double)accum, (double)in);
#endif
}
BINARY_OP_RUNNER_METADEF(Rem, mxSingle, Single, singleconv_T);


static mxSingle msh_ModSingle(mxSingle accum, mxSingle in)
{
	mxSingle  rem;
	if(in != 0)
	{
		rem = msh_RemSingle(accum, in);
		return (rem != 0) ? (((rem < 0) != (in < 0)) ? (rem + in) : rem) : 0;
	}
	return accum;
}
BINARY_OP_RUNNER(msh_ModSingleRunnerW, VO_FCN_FNAME(Mod, Single), VO_FCN_CASNAME(Single), VO_FCN_CTCNAME(Single), Mod, mxSingle, Single, singleconv_T);

static void msh_ModSingleRunner(void* v_accum, void* v_in, long opts)
{
	WideInput_T(single)* wide_in = v_in;
	if(wide_in->num_elems == 1 && *wide_in->input == 0)
	{
		return;
	}
	msh_ModSingleRunnerW(v_accum, v_in, opts);
}


static mxSingle msh_NegSingle(mxSingle accum)
{
	return -accum;
}
UNARY_OP_RUNNER_METADEF(Neg, mxSingle, Single);


static mxSingle msh_ARSSingle(mxSingle accum, mxSingle in)
{
#if __STDC_VERSION__ >= 199901L
	return accum / (float)powf(2.0, (float)in);
#else
	return accum / (mxSingle)pow(2.0, (double)in);
#endif
}
BINARY_OP_RUNNER(msh_ARSSingleRunnerW, VO_FCN_FNAME(ARS, Single), VO_FCN_CASNAME(Single), VO_FCN_CTCNAME(Single), ARS, mxSingle, Single, singleconv_T);

static void msh_ARSSingleRunner(void* v_accum, void* v_in, long opts)
{
	mxSingle             mult, static_val;
	singleconv_T         in_conv;
	WideInput_T(single)  sing_in;
	WideInput_T(single)* wide_accum = v_accum;
	WideInput_T(single)* wide_in    = v_in;
	
	if(wide_in->num_elems == 1)
	{
		if(wide_accum->mxtype != wide_in->mxtype)
		{
			in_conv = msh_ChooseSingleConverter(wide_in->mxtype);
			static_val = in_conv(wide_in->input, 0);
		}
		else
		{
			static_val = *wide_in->input;
		}
#if __STDC_VERSION__ >= 199901L
		mult = (float)powf(2.0, (float)static_val);
#else
		mult = (mxSingle)pow(2.0, (double)static_val);
#endif
		sing_in.input = &mult;
		sing_in.num_elems = 1;
		sing_in.mxtype = wide_accum->mxtype;
		msh_DivSingleRunner(v_accum, &sing_in, opts);
	}
	else
	{
		msh_ARSSingleRunnerW(v_accum, v_in, opts);
	}
}

static mxSingle msh_ALSSingle(mxSingle accum, mxSingle in)
{
#if __STDC_VERSION__ >= 199901L
	return accum * (float)powf(2.0, (float)in);
#else
	return accum * (mxSingle)pow(2.0, (double)in);
#endif
}
BINARY_OP_RUNNER(msh_ALSSingleRunnerW, VO_FCN_FNAME(ALS, Single), VO_FCN_CASNAME(Single), VO_FCN_CTCNAME(Single), ALS, mxSingle, Single, singleconv_T);

static void msh_ALSSingleRunner(void* v_accum, void* v_in, long opts)
{
	mxSingle             mult, static_val;
	singleconv_T         in_conv;
	WideInput_T(single)  sing_in;
	WideInput_T(single)* wide_accum = v_accum;
	WideInput_T(single)* wide_in    = v_in;
	
	if(wide_in->num_elems == 1)
	{
		if(wide_accum->mxtype != wide_in->mxtype)
		{
			in_conv = msh_ChooseSingleConverter(wide_in->mxtype);
			static_val = in_conv(wide_in->input, 0);
		}
		else
		{
			static_val = *wide_in->input;
		}
#if __STDC_VERSION__ >= 199901L
		mult = (float)powf(2.0, (float)static_val);
#else
		mult = (mxSingle)pow(2.0, (double)static_val);
#endif
		sing_in.input = &mult;
		sing_in.num_elems = 1;
		sing_in.mxtype = wide_accum->mxtype;
		msh_MulSingleRunner(v_accum, &sing_in, opts);
	}
	else
	{
		msh_ALSSingleRunnerW(v_accum, v_in, opts);
	}
}

/** mxDouble **/

static mxDouble msh_AbsDouble(mxDouble accum)
{
	return (mxDouble)fabs((double)accum);
}
UNARY_OP_RUNNER_METADEF(Abs, mxDouble, Double);


static mxDouble msh_AddDouble(mxDouble accum, mxDouble in)
{
	return accum + in;
}
BINARY_OP_RUNNER_METADEF(Add, mxDouble, Double, doubleconv_T);

static mxDouble msh_SubDouble(mxDouble accum, mxDouble in)
{
	return accum - in;
}
BINARY_OP_RUNNER_METADEF(Sub, mxDouble, Double, doubleconv_T);


static mxDouble msh_MulDouble(mxDouble accum, mxDouble in)
{
	return accum * in;
}
BINARY_OP_RUNNER_METADEF(Mul, mxDouble, Double, doubleconv_T);


static mxDouble msh_DivDouble(mxDouble accum, mxDouble in)
{
	return accum / in;
}
BINARY_OP_RUNNER_METADEF(Div, mxDouble, Double, doubleconv_T);


static mxDouble msh_RemDouble(mxDouble accum, mxDouble in)
{
	return (mxDouble)fmod((double)accum, (double)in);
}
BINARY_OP_RUNNER_METADEF(Rem, mxDouble, Double, doubleconv_T);


static mxDouble msh_ModDouble(mxDouble accum, mxDouble in)
{
	mxDouble  rem;
	if(in != 0)
	{
		rem = msh_RemDouble(accum, in);
		return (rem != 0) ? (((rem < 0) != (in < 0)) ? (rem + in) : rem) : 0;
	}
	return accum;
}
BINARY_OP_RUNNER(msh_ModDoubleRunnerW, VO_FCN_FNAME(Mod, Double), VO_FCN_CASNAME(Double), VO_FCN_CTCNAME(Double), Mod, mxDouble, Double, doubleconv_T);

static void msh_ModDoubleRunner(void* v_accum, void* v_in, long opts)
{
	WideInput_T(double)* wide_in = v_in;
	if(wide_in->num_elems == 1 && *wide_in->input == 0)
	{
		return;
	}
	msh_ModDoubleRunnerW(v_accum, v_in, opts);
}


static mxDouble msh_NegDouble(mxDouble accum)
{
	return -accum;
}
UNARY_OP_RUNNER_METADEF(Neg, mxDouble, Double);


static mxDouble msh_ARSDouble(mxDouble accum, mxDouble in)
{
	return accum / (mxDouble)pow(2.0, (double)in);
}
BINARY_OP_RUNNER(msh_ARSDoubleRunnerW, VO_FCN_FNAME(ARS, Double), VO_FCN_CASNAME(Double), VO_FCN_CTCNAME(Double), ARS, mxDouble, Double, doubleconv_T);

static void msh_ARSDoubleRunner(void* v_accum, void* v_in, long opts)
{
	mxDouble             mult, static_val;
	doubleconv_T         in_conv;
	WideInput_T(double)  sing_in;
	WideInput_T(double)* wide_accum = v_accum;
	WideInput_T(double)* wide_in    = v_in;
	
	if(wide_in->num_elems == 1)
	{
		if(wide_accum->mxtype != wide_in->mxtype)
		{
			in_conv = msh_ChooseDoubleConverter(wide_in->mxtype);
			static_val = in_conv(wide_in->input, 0);
		}
		else
		{
			static_val = *wide_in->input;
		}
		mult = pow(2.0, static_val);
		sing_in.input = &mult;
		sing_in.num_elems = 1;
		sing_in.mxtype = wide_accum->mxtype;
		msh_DivDoubleRunner(v_accum, &sing_in, opts);
	}
	else
	{
		msh_ARSDoubleRunnerW(v_accum, v_in, opts);
	}
}


static mxDouble msh_ALSDouble(mxDouble accum, mxDouble in)
{
	return accum * pow(2.0, in);
}
BINARY_OP_RUNNER(msh_ALSDoubleRunnerW, VO_FCN_FNAME(ALS, Double), VO_FCN_CASNAME(Double), VO_FCN_CTCNAME(Double), ALS, mxDouble, Double, doubleconv_T);

static void msh_ALSDoubleRunner(void* v_accum, void* v_in, long opts)
{
	mxDouble             mult, static_val;
	doubleconv_T         in_conv;
	WideInput_T(double)  sing_in;
	WideInput_T(double)* wide_accum = v_accum;
	WideInput_T(double)* wide_in    = v_in;
	
	if(wide_in->num_elems == 1)
	{
		if(wide_accum->mxtype != wide_in->mxtype)
		{
			in_conv = msh_ChooseDoubleConverter(wide_in->mxtype);
			static_val = in_conv(wide_in->input, 0);
		}
		else
		{
			static_val = *wide_in->input;
		}
		mult = pow(2.0, static_val);
		sing_in.input = &mult;
		sing_in.num_elems = 1;
		sing_in.mxtype = wide_accum->mxtype;
		msh_MulDoubleRunner(v_accum, &sing_in, opts);
	}
	else
	{
		msh_ALSDoubleRunnerW(v_accum, v_in, opts);
	}
}

#define CPY_FCN_DEF(RNAME, NAME, SNAME, CTCNAME, TYPE, TYPEC) \
static void RNAME(void* v_accum, void* v_in, long opts) \
{ \
	size_t i; \
	TYPEC in_conv; \
	TYPE new_val; \
	WideInput_T(TYPE)* wide_accum = v_accum; \
	WideInput_T(TYPE)* wide_in    = v_in; \
	if(wide_in->num_elems == 1) \
	{ \
		if(wide_accum->mxtype != wide_in->mxtype) \
		{ \
			in_conv = CTCNAME(wide_in->mxtype); \
			new_val = in_conv(wide_in->input, 0); \
		} \
		else \
		{ \
			new_val = *wide_in->input; \
		} \
		if(opts & MSH_USE_ATOMIC_OPS) \
		{ \
			for(i = 0; i < wide_accum->num_elems;) \
			{ \
				SNAME(wide_accum->input + i, new_val); \
			} \
		} \
		else \
		{ \
			for(i = 0; i < wide_accum->num_elems; i++) \
			{ \
				wide_accum->input[i] = new_val; \
			} \
		} \
	} \
	else \
	{ \
		if(wide_accum->mxtype != wide_in->mxtype) \
		{ \
			in_conv = CTCNAME(wide_in->mxtype); \
			if(opts & MSH_USE_ATOMIC_OPS) \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					SNAME(wide_accum->input + i, in_conv(wide_in->input, i)); \
				} \
			} \
			else \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					wide_accum->input[i] = in_conv(wide_in->input, i); \
				} \
			} \
		} \
		else \
		{ \
			if(opts & MSH_USE_ATOMIC_OPS) \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					SNAME(wide_accum->input + i, wide_in->input[i]); \
				} \
			} \
			else \
			{ \
				for(i = 0; i < wide_accum->num_elems; i++) \
				{ \
					wide_accum->input[i] = wide_in->input[i]; \
				} \
			} \
		} \
	} \
}

#define CPY_INT_METADEF(SIZE) \
CPY_FCN_DEF(FW_INT_FCN_RNAME(Cpy, SIZE), FW_INT_FCN_FNAME(Cpy, SIZE), VO_FCN_SNAME(FW_INT_TYPEN(SIZE)), FW_INT_FCN_CTCNAME(SIZE), FW_INT_TYPE(SIZE), FW_INT_TYPEC(SIZE))

#define CPY_UINT_METADEF(SIZE) \
CPY_FCN_DEF(FW_UINT_FCN_RNAME(Cpy, SIZE), FW_UINT_FCN_FNAME(Cpy, SIZE), VO_FCN_SNAME(FW_UINT_TYPEN(SIZE)), FW_UINT_FCN_CTCNAME(SIZE), FW_UINT_TYPE(SIZE), FW_UINT_TYPEC(SIZE))

CPY_INT_METADEF(8);
CPY_INT_METADEF(16);
CPY_INT_METADEF(32);
#if MSH_BITNESS==64
CPY_INT_METADEF(64);
#endif

CPY_UINT_METADEF(8);
CPY_UINT_METADEF(16);
CPY_UINT_METADEF(32);
#if MSH_BITNESS==64
CPY_UINT_METADEF(64);
#  define msh_CpySizeRunner(V_ACCUM, V_IN, OPTS) msh_CpyUInt64Runner(V_ACCUM, V_IN, OPTS)
#else
#  define msh_CpySizeRunner(V_ACCUM, V_IN, OPTS) msh_CpyUInt32Runner(V_ACCUM, V_IN, OPTS)
#endif

CPY_FCN_DEF(msh_CpySingleRunner, msh_CpySingle, msh_AtomicSetSingle, msh_ChooseSingleConverter, mxSingle, singleconv_T);
CPY_FCN_DEF(msh_CpyDoubleRunner, msh_CpyDouble, msh_AtomicSetDouble, msh_ChooseDoubleConverter, mxDouble, doubleconv_T);

/** function choosers **/

#if MSH_BITNESS==64
#define CLASS_FCN_SWITCH_CASES(OP, CLASS_ID_NAME)          \
switch(CLASS_ID_NAME)                                      \
{                                                          \
	case(mxINT8_CLASS):    return msh_##OP##Int8Runner;   \
	case(mxINT16_CLASS):   return msh_##OP##Int16Runner;  \
	case(mxINT32_CLASS):   return msh_##OP##Int32Runner;  \
	case(mxINT64_CLASS):   return msh_##OP##Int64Runner;  \
                                                           \
	case(mxUINT8_CLASS):   return msh_##OP##UInt8Runner;  \
	case(mxUINT16_CLASS):  return msh_##OP##UInt16Runner; \
	case(mxUINT32_CLASS):  return msh_##OP##UInt32Runner; \
	case(mxUINT64_CLASS):  return msh_##OP##UInt64Runner; \
                                                           \
	case(mxSINGLE_CLASS):  return msh_##OP##SingleRunner; \
	case(mxDOUBLE_CLASS):  return msh_##OP##DoubleRunner; \
	                                                      \
	case(mxLOGICAL_CLASS): return msh_##OP##Int8Runner;   \
	case(mxCHAR_CLASS):    return msh_##OP##Int16Runner;  \
	default: goto NOT_FOUND_ERROR;                        \
}
#else
#define CLASS_FCN_SWITCH_CASES(OP, CLASS_ID_NAME)         \
switch(CLASS_ID_NAME)                                      \
{                                                          \
	case(mxINT8_CLASS):    return msh_##OP##Int8Runner;   \
	case(mxINT16_CLASS):   return msh_##OP##Int16Runner;  \
	case(mxINT32_CLASS):   return msh_##OP##Int32Runner;  \
                                                           \
	case(mxUINT8_CLASS):   return msh_##OP##UInt8Runner;  \
	case(mxUINT16_CLASS):  return msh_##OP##UInt16Runner; \
	case(mxUINT32_CLASS):  return msh_##OP##UInt32Runner; \
                                                           \
	case(mxSINGLE_CLASS):  return msh_##OP##SingleRunner; \
	case(mxDOUBLE_CLASS):  return msh_##OP##DoubleRunner; \
	                                                      \
	case(mxLOGICAL_CLASS): return msh_##OP##Int8Runner;   \
	case(mxCHAR_CLASS):    return msh_##OP##Int16Runner;  \
	default: goto NOT_FOUND_ERROR;                        \
}
#endif

static unaryvaropfcn_T msh_ChooseUnaryVarOpFcn(msh_varop_T varop, mxClassID class_id)
{
	switch(varop)
	{
		case(VAROP_ABS):
		{
			CLASS_FCN_SWITCH_CASES(Abs, class_id);
		}
		case(VAROP_NEG):
		{
			CLASS_FCN_SWITCH_CASES(Neg, class_id);
		}
		default:
		{
			goto NOT_FOUND_ERROR;
		}
	}

NOT_FOUND_ERROR:
	meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "NoVaropFoundError", "Could not find a suitable variable operation.");
	
	return 0;
	
}


void msh_UnaryVariableOperation(IndexedVariable_T* indexed_var, msh_varop_T varop, long opts, mxArray** output)
{
	size_t          i, j, nzmax, elem_size, dest_offset, dest_num_elems;
	
	int             is_complex = mxIsComplex(indexed_var->dest_var);
	unaryvaropfcn_T varop_fcn  = msh_ChooseUnaryVarOpFcn(varop, mxGetClassID(indexed_var->dest_var));
	
	int8_T*             dest_real       = mxGetData(indexed_var->dest_var);
	int8_T*             dest_imag       = mxGetImagData(indexed_var->dest_var);
	
	WideInput_T(int8_T) wide_dest_real  = {dest_real, 0};
	WideInput_T(int8_T) wide_dest_imag  = {dest_imag, 0};
	
	if(mxIsSparse(indexed_var->dest_var))
	{
		nzmax = mxGetNzmax(indexed_var->dest_var);
		
		wide_dest_real.num_elems = nzmax;
		
		varop_fcn(&wide_dest_real, opts);
		if(is_complex)
		{
			wide_dest_imag.num_elems = nzmax;
			varop_fcn(&wide_dest_imag, opts);
		}
		
	}
	else
	{
		
		elem_size = mxGetElementSize(indexed_var->dest_var);
		
		if(indexed_var->indices.start_idxs == NULL)
		{
			
			dest_num_elems  = mxGetNumberOfElements(indexed_var->dest_var);
			wide_dest_real.num_elems = dest_num_elems;
			
			varop_fcn(&wide_dest_real, opts);
			if(is_complex)
			{
				wide_dest_imag.num_elems = dest_num_elems;
				
				varop_fcn(&wide_dest_imag, opts);
			}
		}
		else
		{
			for(i = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
			{
				for(j = 0; j < indexed_var->indices.num_lens; j++)
				{
					dest_offset = indexed_var->indices.start_idxs[i+j]*elem_size/sizeof(int8_T);
					
					wide_dest_real.input     = dest_real + dest_offset;
					wide_dest_real.num_elems = indexed_var->indices.slice_lens[j];
					
					varop_fcn(&wide_dest_real, opts);
					
					if(is_complex)
					{
						wide_dest_imag.input     = dest_imag + dest_offset;
						wide_dest_imag.num_elems = indexed_var->indices.slice_lens[j];
						
						varop_fcn(&wide_dest_imag, opts);
					}
				}
			}
		}
	}
	
	if(output != NULL)
	{
		*output = mxDuplicateArray(indexed_var->dest_var);
	}
	
}

static binaryvaropfcn_T msh_ChooseBinaryVarOpFcn(msh_varop_T varop, mxClassID class_id)
{
	switch(varop)
	{
		case(VAROP_ADD): CLASS_FCN_SWITCH_CASES(Add, class_id);
		case(VAROP_SUB): CLASS_FCN_SWITCH_CASES(Sub, class_id);
		case(VAROP_MUL): CLASS_FCN_SWITCH_CASES(Mul, class_id);
		case(VAROP_DIV): CLASS_FCN_SWITCH_CASES(Div, class_id);
		case(VAROP_REM): CLASS_FCN_SWITCH_CASES(Rem, class_id);
		case(VAROP_MOD): CLASS_FCN_SWITCH_CASES(Mod, class_id);
		case(VAROP_ARS): CLASS_FCN_SWITCH_CASES(ARS, class_id);
		case(VAROP_ALS): CLASS_FCN_SWITCH_CASES(ALS, class_id);
		case(VAROP_CPY): CLASS_FCN_SWITCH_CASES(Cpy, class_id);
		default: goto NOT_FOUND_ERROR;
	}

NOT_FOUND_ERROR:
	meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "NoVaropFoundError", "Could not find a suitable variable operation.");
	
	return 0;
	
}

void msh_BinaryVariableOperation(IndexedVariable_T* indexed_var, const mxArray* in_var, msh_varop_T varop, long opts, mxArray** output)
{
	int               field_num, in_num_fields, is_complex;
	size_t            i, j;
	size_t            dest_idx, dest_elem_size, dest_offset;
	size_t            in_idx, in_elem_size, in_num_elems;
	binaryvaropfcn_T  varop_fcn;
	const char_T*     curr_field_name;
	
	int8_T*             dest_real_anchor;
	int8_T*             dest_imag_anchor;
	
	WideInput_T(int8_T)  wide_dest_real;
	WideInput_T(int8_T)  wide_dest_imag;
	WideInput_T(mwIndex) wide_dest_ir;
	WideInput_T(mwIndex) wide_dest_jc;
	
	WideInput_T(int8_T)  wide_in_real;
	WideInput_T(int8_T)  wide_in_imag;
	WideInput_T(mwIndex) wide_in_ir;
	WideInput_T(mwIndex) wide_in_jc;
	
	IndexedVariable_T sub_variable = {0};
	
	switch(mxGetClassID(indexed_var->dest_var))
	{
		case (mxSTRUCT_CLASS):
		{
			in_num_fields = mxGetNumberOfFields(in_var);
			in_num_elems = mxGetNumberOfElements(in_var);
			
			/* go through each element */
			if(indexed_var->indices.start_idxs == NULL)
			{
				for(field_num = 0; field_num < in_num_fields; field_num++)     /* each field */
				{
					curr_field_name = mxGetFieldNameByNumber(indexed_var->dest_var, field_num);
					for(dest_idx = 0; dest_idx < in_num_elems; dest_idx++)
					{
						sub_variable.dest_var = mxGetField(indexed_var->dest_var, dest_idx, curr_field_name);
						msh_BinaryVariableOperation(&sub_variable, mxGetField(in_var, dest_idx, curr_field_name), varop, opts, output);
					}
				}
			}
			else
			{
				for(field_num = 0; field_num < in_num_fields; field_num++)     /* each field */
				{
					curr_field_name = mxGetFieldNameByNumber(indexed_var->dest_var, field_num);
					for(i = 0, in_idx = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
					{
						for(j = 0; j < indexed_var->indices.num_lens; j++)
						{
							for(dest_idx = indexed_var->indices.start_idxs[i + j]; dest_idx < indexed_var->indices.start_idxs[i + j] + indexed_var->indices.slice_lens[j]; dest_idx++, in_idx++)
							{
								/* this inner conditional should be optimized out by the compiler */
								sub_variable.dest_var = mxGetField(indexed_var->dest_var, dest_idx, curr_field_name);
								msh_BinaryVariableOperation(&sub_variable, mxGetField(in_var, (in_num_elems == 1)? 0 : in_idx, curr_field_name), varop, opts, output);
							}
						}
					}
				}
			}
			break;
		}
		case (mxCELL_CLASS):
		{
			
			in_num_elems = mxGetNumberOfElements(in_var);
			
			/* go through each element */
			if(indexed_var->indices.start_idxs == NULL)
			{
				for(dest_idx = 0; dest_idx < in_num_elems; dest_idx++)
				{
					sub_variable.dest_var = mxGetCell(indexed_var->dest_var, dest_idx);
					msh_BinaryVariableOperation(&sub_variable, mxGetCell(in_var, dest_idx), varop, opts, output);
				}
			}
			else
			{
				for(i = 0, in_idx = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
				{
					for(j = 0; j < indexed_var->indices.num_lens; j++)
					{
						for(dest_idx = indexed_var->indices.start_idxs[i + j]; dest_idx < indexed_var->indices.start_idxs[i + j] + indexed_var->indices.slice_lens[j]; dest_idx++, in_idx++)
						{
							/* this inner conditional should be optimized out by the compiler */
							sub_variable.dest_var = mxGetCell(indexed_var->dest_var, dest_idx);
							msh_BinaryVariableOperation(&sub_variable, mxGetCell(in_var, (in_num_elems == 1)? 0 : in_idx), varop, opts, output);
						}
					}
				}
			}
			break;
		}
		default:
		{
			
			
			wide_in_real.mxtype   = mxGetClassID(in_var);
			wide_in_imag.mxtype   = wide_in_real.mxtype;
			in_elem_size          = mxGetElementSize(in_var);
			
			wide_dest_real.mxtype = mxGetClassID(indexed_var->dest_var);
			wide_dest_imag.mxtype = wide_dest_real.mxtype;
			dest_elem_size        = mxGetElementSize(indexed_var->dest_var);
			
			
			varop_fcn = msh_ChooseBinaryVarOpFcn(varop, mxGetClassID(indexed_var->dest_var));
			is_complex = mxIsComplex(in_var);
			
			if(mxIsSparse(indexed_var->dest_var))
			{
				in_num_elems = mxGetNumberOfElements(in_var);
				
				/* repetitive function calls should be optimized by the compiler */
				
				/* ir setup */
				wide_in_ir.input = mxGetIr(in_var);
				wide_in_ir.num_elems = mxGetNzmax(in_var);
				wide_in_ir.mxtype = mxINDEX_CLASS;
				
				wide_dest_ir.input = mxGetIr(indexed_var->dest_var);
				wide_dest_ir.num_elems = wide_in_ir.num_elems; /* copy the nzmax of input because in_nzmax <= dest_nzmax */
				wide_dest_ir.mxtype = mxINDEX_CLASS;
				
				/* jc setup */
				wide_in_jc.input = mxGetJc(in_var);
				wide_in_jc.num_elems = mxGetN(in_var) + 1;
				wide_in_jc.mxtype = mxINDEX_CLASS;
				
				wide_dest_jc.input = mxGetJc(indexed_var->dest_var);
				wide_dest_jc.num_elems = wide_in_jc.num_elems;
				wide_dest_jc.mxtype = mxINDEX_CLASS;
				
				/* real setup */
				wide_in_real.input = mxGetData(in_var);
				wide_in_real.num_elems = mxGetNzmax(in_var);
				
				wide_dest_real.input = mxGetData(indexed_var->dest_var);
				wide_dest_real.num_elems = wide_in_real.num_elems; /* copy the nzmax of input because in_nzmax <= dest_nzmax */
				
				/* copying */
				varop_fcn(&wide_dest_ir, &wide_in_ir, opts);
				varop_fcn(&wide_dest_jc, &wide_in_jc, opts);
				varop_fcn(&wide_dest_real, &wide_in_real, opts);
				
				if(is_complex)
				{
					/* imag setup */
					wide_in_imag.input = mxGetImagData(in_var);
					wide_in_imag.num_elems = wide_in_real.num_elems;
					
					wide_dest_imag.input = mxGetImagData(indexed_var->dest_var);
					wide_dest_imag.num_elems = wide_in_real.num_elems;
					
					varop_fcn(&wide_dest_imag, &wide_in_imag, opts);
				}
			}
			else
			{
				if(indexed_var->indices.start_idxs == NULL)
				{
					/* use the entire variable */
					
					/* real setup */
					wide_in_real.input = mxGetData(in_var);
					wide_in_real.num_elems = mxGetNumberOfElements(in_var);
					
					wide_dest_real.input = mxGetData(indexed_var->dest_var);
					wide_dest_real.num_elems = mxGetNumberOfElements(indexed_var->dest_var);
					
					/* copying */
					varop_fcn(&wide_dest_real, &wide_in_real, opts);
					
					if(is_complex)
					{
						/* imag setup */
						wide_in_imag.input = mxGetImagData(in_var);
						wide_in_imag.num_elems = wide_in_real.num_elems;
						
						wide_dest_imag.input = mxGetImagData(indexed_var->dest_var);
						wide_dest_imag.num_elems = wide_dest_real.num_elems;
						
						/* copying */
						varop_fcn(&wide_dest_imag, &wide_in_imag, opts);
					}
				}
				else
				{
					
					dest_real_anchor = mxGetData(indexed_var->dest_var);
					dest_imag_anchor = mxGetImagData(indexed_var->dest_var);
					
					wide_in_real.input = mxGetData(in_var);
					wide_in_real.num_elems = mxGetNumberOfElements(in_var);
					
					if(is_complex)
					{
						wide_in_imag.input = mxGetImagData(in_var);
						wide_in_imag.num_elems = wide_in_real.num_elems;
					}
					
					for(i = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
					{
						for(j = 0; j < indexed_var->indices.num_lens; j++)
						{
							dest_offset = indexed_var->indices.start_idxs[i+j]*dest_elem_size/sizeof(int8_T);
							
							/* real setup */
							wide_dest_real.input = dest_real_anchor + dest_offset;
							wide_dest_real.num_elems = indexed_var->indices.slice_lens[j];
							
							varop_fcn(&wide_dest_real, &wide_in_real, opts);
							
							if(is_complex)
							{
								wide_dest_imag.input = dest_imag_anchor + dest_offset;
								wide_dest_imag.num_elems = indexed_var->indices.slice_lens[j];
								
								varop_fcn(&wide_dest_imag, &wide_in_imag, opts);
							}
							
							/* this condition should be optimized to the exterior by the compiler */
							if(wide_in_real.num_elems != 1)
							{
								wide_in_real.input += indexed_var->indices.slice_lens[j]*in_elem_size/sizeof(int8_T);
								if(is_complex)
								{
									wide_in_imag.input += indexed_var->indices.slice_lens[j]*in_elem_size/sizeof(int8_T);
								}
							}
						}
					}
				}
			}
			break;
		}
	}
	
	if(output != NULL)
	{
		*output = mxDuplicateArray(indexed_var->dest_var);
	}
	
}


void msh_VariableOperation(const mxArray* parent_var, const mxArray* in_vars, const mxArray* subs_struct, msh_varop_T varop, long opts, mxArray** output)
{
	/* Note: in_vars is always a cell array */
	size_t i;
	
	int exp_num_in_args = msh_GetNumVarOpArgs(varop)-1;
	IndexedVariable_T indexed_var = {parent_var, {NULL, NULL, 0, NULL, 0}};
	
	if(mxGetNumberOfElements(in_vars) != exp_num_in_args)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNumberOfInputsError", "Too many or too few inputs.");
	}
	
	if(!mxIsCell(in_vars))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "InVarsNotCellError", "Input variables should be contained in a cell array. Please use the provided entry functions.");
	}
	
	if(subs_struct != NULL)
	{
		indexed_var = msh_ParseSubscriptStruct(parent_var, subs_struct);
	}
	
	for(i = 0; i < exp_num_in_args; i++)
	{
		/* no varops for sparses except for copying---this is checked here */
		msh_CheckValidInput(&indexed_var, mxGetCell(in_vars, i), varop);
	}
	
	if(opts & MSH_IS_SYNCHRONOUS) msh_AcquireProcessLock(g_process_lock);
	
	switch(exp_num_in_args)
	{
		case(0):
		{
			msh_UnaryVariableOperation(&indexed_var, varop, opts, output);
			break;
		}
		case(1):
		{
			msh_BinaryVariableOperation(&indexed_var, mxGetCell(in_vars, 0), varop, opts, output);
			break;
		}
		default:
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidNumberOfInputsError", "Too many or too few inputs.");
		}
	}
	
	if(opts & MSH_IS_SYNCHRONOUS) msh_ReleaseProcessLock(g_process_lock);
	
	if(indexed_var.indices.start_idxs != NULL)
	{
		mxFree(indexed_var.indices.start_idxs);
	}
	
	if(indexed_var.indices.slice_lens != NULL)
	{
		mxFree(indexed_var.indices.slice_lens);
	}
	
}
