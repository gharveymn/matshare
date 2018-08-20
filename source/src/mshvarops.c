/** mshvarops.c
 * Defines functions for variable operations.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#include <math.h>

#include "mshvarops.h"
#include "mshutils.h"
#include "mlerrorutils.h"


static size_t msh_ParseIndicesWorker(mxArray*      subs_arr,
                                     const mwSize* dest_dims,
                                     size_t        dest_num_dims,
                                     mwIndex*      start_idxs,
                                     const mwSize* slice_lens,
                                     size_t        num_parsed,
                                     size_t        curr_dim,
                                     size_t        base_dim,
                                     size_t        curr_mult,
                                     size_t        curr_index);


void msh_ParseSubscriptStruct(IndexedVariable_t* indexed_var, const mxArray* in_var, const mxArray* substruct)
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
	
	for(i = 0, num_idxs = mxGetNumberOfElements(substruct); i < num_idxs; i++)
	{
		idxstruct.type = mxGetField(substruct, i, "type");
		idxstruct.subs = mxGetField(substruct, i, "subs");
		
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
				if(!mxIsStruct(indexed_var->dest_var))
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndexError", "Variable class 'struct' required for this assignment.");
				}
				
				if(mxGetNumberOfElements(indexed_var->dest_var) != 1)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndexError", "Scalar struct required for this assignment.");
				}
				
				mxGetString(idxstruct.subs, idxstruct.subs_str, MSH_NAME_LEN_MAX);
				
				if((indexed_var->dest_var = mxGetField(indexed_var->dest_var, 0, idxstruct.subs_str)) == NULL)
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "FieldNotFoundError", "Could not find field '%s'.", idxstruct.subs_str);
				}
				
				break;
			}
			case ('{'):
			{
				/* this should be a cell array */
				if(!mxIsCell(indexed_var->dest_var))
				{
					meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidIndexError", "Variable class 'cell' required for this assignment.");
				}
				
				/* only proceed if this is a singular index */
				
				index = msh_ParseSingleIndex(idxstruct.subs, indexed_var->dest_var, in_var);
				indexed_var->dest_var = mxGetCell(indexed_var->dest_var, index);
				
				break;
			}
			case ('('):
			{
				/* if this is not the last indexing then can only proceed if this is a singular index to a struct */
				if(i + 1 < num_idxs)
				{
					if(mxIsStruct(indexed_var->dest_var))
					{
						index = msh_ParseSingleIndex(idxstruct.subs, indexed_var->dest_var, in_var);
						
						/* to to next subscript */
						i++;
						
						idxstruct.type = mxGetField(substruct, i, "type");
						idxstruct.subs = mxGetField(substruct, i, "subs");
						
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
						
						if((indexed_var->dest_var = mxGetField(indexed_var->dest_var, index, idxstruct.subs_str)) == NULL)
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
					indexed_var->indices = msh_ParseIndices(idxstruct.subs, indexed_var->dest_var, in_var);
				}
				break;
			}
			default:
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UnrecognizedIndexTypeError", "Unrecognized type '%s' for indexing", mxArrayToString(idxstruct.type));
			}
		}
	}
}


ParsedIndices_t msh_ParseIndices(mxArray* subs_arr, const mxArray* dest_var, const mxArray* in_var)
{
	size_t          i, j, k, num_subs, base_dim, max_mult, exp_num_elems;
	size_t          first_sig_dim, last_sig_dim, num_sig_dims;
	double          dbl_iter;
	mxArray*        curr_subs;
	
	size_t          dest_num_elems = mxGetNumberOfElements(dest_var);
	size_t          num_subs_elems = 0;
	size_t          base_mult     = 1;
	mwSize          dest_num_dims  = mxGetNumberOfDimensions(dest_var);
	mwSize          in_num_dims    = mxGetNumberOfDimensions(in_var);
	ParsedIndices_t parsed_indices = {0};
	double*         indices        = NULL;
	const size_t*   dest_dims      = mxGetDimensions(dest_var);
	const size_t*   in_dims        = mxGetDimensions(in_var);
	
	if(mxIsEmpty(in_var))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NullAssignmentError", "Assignment input must be non-null.");
	}
	
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
	
	msh_ParseIndicesWorker(subs_arr, dest_dims, dest_num_dims, parsed_indices.start_idxs, parsed_indices.slice_lens, 0, num_subs - 1, base_dim, max_mult, 0);
	
	return parsed_indices;
	
}


mwIndex msh_ParseSingleIndex(mxArray* subs_arr, const mxArray* dest_var, const mxArray* in_var)
{
	size_t          i, num_subs, max_mult;
	mwIndex         ret_idx;
	mxArray*        curr_subs;
	
	size_t          num_subs_elems   = 0;
	mwSize          dummy_slice_lens = 1;
	mwSize          dest_num_dims    = mxGetNumberOfDimensions(dest_var);
	const size_t*   dest_dims        = mxGetDimensions(dest_var);
	
	if(mxIsEmpty(in_var))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NullAssignmentError", "Assignment input must be non-null.");
	}
	
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
	
	msh_ParseIndicesWorker(subs_arr, dest_dims, dest_num_dims, &ret_idx, &dummy_slice_lens, 0, num_subs - 1, 0, max_mult, 0);
	
	return ret_idx;
}


static size_t msh_ParseIndicesWorker(mxArray* subs_arr, const mwSize* dest_dims, size_t dest_num_dims, mwIndex* start_idxs, const mwSize* slice_lens, size_t num_parsed, size_t curr_dim, size_t base_dim, size_t curr_mult, size_t curr_index)
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
					                                    start_idxs,
					                                    slice_lens,
					                                    num_parsed,
					                                    curr_dim - 1,
					                                    base_dim,
					                                    curr_mult/dest_dims[curr_dim - 1],
					                                    curr_index + i*curr_mult);
				}
			}
			else
			{
				num_parsed = msh_ParseIndicesWorker(subs_arr,
				                                    dest_dims,
				                                    dest_num_dims,
				                                    start_idxs,
				                                    slice_lens,
				                                    num_parsed,
				                                    curr_dim - 1,
				                                    base_dim,
				                                    curr_mult,
				                                    curr_index);
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
					                                    start_idxs,
					                                    slice_lens,
					                                    num_parsed,
					                                    curr_dim - 1,
					                                    base_dim,
					                                    curr_mult/dest_dims[curr_dim - 1],
					                                    curr_index + ((mwIndex)indices[i] - 1)*curr_mult);
				}
			}
			else
			{
				for(i = 0, num_elems = mxGetNumberOfElements(curr_sub); i < num_elems; i++)
				{
					num_parsed = msh_ParseIndicesWorker(subs_arr,
					                                    dest_dims,
					                                    dest_num_dims,
					                                    start_idxs,
					                                    slice_lens,
					                                    num_parsed,
					                                    curr_dim - 1,
					                                    base_dim,
					                                    curr_mult,
					                                    curr_index);
				}
			}
		}
	}
	else
	{
		if(mxIsChar(curr_sub))
		{
			start_idxs[num_parsed] = curr_index;
			num_parsed += 1;
		}
		else/*if(mxIsDouble(curr_sub))*/
		{
			indices = mxGetData(curr_sub);
			for(i = 0, j = 0, num_elems = mxGetNumberOfElements(curr_sub); i < num_elems; i += slice_lens[j++])
			{
				start_idxs[num_parsed] = curr_index + ((mwIndex)indices[i]-1)*curr_mult;
				num_parsed += 1;
			}
		}
	}
	
	return num_parsed;
	
}

void msh_CompareVariableSize(IndexedVariable_t* indexed_var, const mxArray* comp_var)
{
	int               field_num, dest_num_fields;
	size_t            i, j, dest_idx, comp_idx, dest_num_elems, comp_num_elems;
	const char_T*     curr_field_name;
	
	IndexedVariable_t sub_variable  = {0};
	mxClassID         dest_class_id = mxGetClassID(indexed_var->dest_var);
	
	if(dest_class_id != mxGetClassID(comp_var))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(comp_var), mxGetClassName(indexed_var->dest_var));
	}
	
	if(mxGetElementSize(indexed_var->dest_var) != mxGetElementSize(comp_var))
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "ElementSizeError", "Element sizes were incompatible.");
	}
	
	dest_num_elems = mxGetNumberOfElements(indexed_var->dest_var);
	comp_num_elems = mxGetNumberOfElements(comp_var);
	
	if(indexed_var->indices.start_idxs == NULL)
	{
		/* can't allow differing dimensions because matlab doesn't use shared pointers to dimensions in mxArrays */
		if(mxGetNumberOfDimensions(indexed_var->dest_var) != mxGetNumberOfDimensions(comp_var) ||
		   memcmp(mxGetDimensions(indexed_var->dest_var), mxGetDimensions(comp_var), mxGetNumberOfDimensions(indexed_var->dest_var)*sizeof(mwSize)) != 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleSizeError", "The input variable was of differing size in comparison to the destination.");
		}
	}
	else
	{
		
		if(indexed_var->indices.num_idxs < indexed_var->indices.num_lens)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "IndexParsingError", "The internal index parser failed.");
		}
		
		if(mxIsSparse(indexed_var->dest_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL, "SubscriptedSparseError", "Cannot directly assign sparse matrices through subscripts.");
		}
		else
		{
			for(i = 0, comp_idx = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
			{
				for(j = 0; j < indexed_var->indices.num_lens; j++)
				{
					if(dest_num_elems < indexed_var->indices.start_idxs[i+j] + indexed_var->indices.slice_lens[j])
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "DestIndexingError", "Index exceeds destination variable size.");
					}
					
					comp_idx += indexed_var->indices.slice_lens[j];
					if(comp_num_elems != 1 && comp_num_elems < comp_idx)
					{
						meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InputIndexingError", "Index exceeds input variable size.");
					}
				}
			}
		}
	}
	
	/* Structure case */
	if(dest_class_id == mxSTRUCT_CLASS)
	{
		
		/* must be struct
		if(dest_class_id != mxGetClassID(comp_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(comp_var), mxGetClassName(indexed_var->dest_var));
		}
		*/
		
		dest_num_elems = mxGetNumberOfElements(indexed_var->dest_var);
		dest_num_fields = mxGetNumberOfFields(indexed_var->dest_var);
		
		if(dest_num_fields != mxGetNumberOfFields(comp_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "NumFieldsError", "The number of fields in the input variable (%d) differs from that of the destination (%d).", mxGetNumberOfFields(comp_var), dest_num_fields);
		}
		
		/* search for each field name */
		for(field_num = 0; field_num < dest_num_fields; field_num++)
		{
			if(mxGetField(comp_var, 0, mxGetFieldNameByNumber(indexed_var->dest_var, field_num)) == NULL)
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
					msh_CompareVariableSize(&sub_variable, mxGetField(comp_var, dest_idx, curr_field_name));
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
							msh_CompareVariableSize(&sub_variable, mxGetField(comp_var, (comp_num_elems == 1)? 0 : comp_idx, curr_field_name));
						}
					}
				}
			}
		}
	}
	else if(dest_class_id == mxCELL_CLASS) /* Cell case */
	{
		/* must be cell
		if(dest_class_id != mxGetClassID(comp_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(comp_var), mxGetClassName(indexed_var->dest_var));
		}
		*/
		
		dest_num_elems = mxGetNumberOfElements(indexed_var->dest_var);
		
		if(indexed_var->indices.start_idxs == NULL)
		{
			for(dest_idx = 0; dest_idx < dest_num_elems; dest_idx++)
			{
				sub_variable.dest_var = mxGetCell(indexed_var->dest_var, dest_idx);
				msh_CompareVariableSize(&sub_variable, mxGetCell(comp_var, dest_idx));
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
						msh_CompareVariableSize(&sub_variable, mxGetCell(comp_var, (comp_num_elems == 1)? 0 : comp_idx));
					}
				}
			}
		}
	}
	else if(mxIsNumeric(indexed_var->dest_var) || dest_class_id == mxLOGICAL_CLASS || dest_class_id == mxCHAR_CLASS)      /*base case*/
	{
		
		/* Note: implement this later
		if(!mxIsNumeric(comp_var) && !mxIsLogical(comp_var) && !mxIsChar(comp_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "IncompatibleClassError", "The input variable class '%s' is not compatible with the destination class '%s'.", mxGetClassName(comp_var), mxGetClassName(indexed_var->dest_var));
		}
		 */
		
		if(mxIsSparse(indexed_var->dest_var))
		{
			/* sparse inputs are always the same size as the destination */
			if(mxGetNzmax(indexed_var->dest_var) < mxGetNzmax(comp_var))
			{
				meu_PrintMexError(MEU_FL,
				                  MEU_SEVERITY_USER,
				                  "NzmaxError",
				                  "The number of non-zero entries in the input variable ("SIZE_FORMAT") is larger than maximum number \n"
				                                                                                     "allocated for destination ("SIZE_FORMAT"). This is an error because doing so would change the size \n"
				                                                                                                                             "of shared memory.",
				mxGetNzmax(comp_var),
				mxGetNzmax(indexed_var->dest_var));
			}
		}
		
		if(mxIsComplex(indexed_var->dest_var) != mxIsComplex(comp_var))
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "ComplexityError", "The complexity of the input variable does not match with that of the destination.");
		}
	}
	else
	{
		/* this may occur if the user passes a destination variable which is not in shared memory */
		meu_PrintMexError(MEU_FL,
		                  MEU_SEVERITY_USER,
		                  "InvalidTypeError",
		                  "Unexpected input type. All elements of the shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
	}
	
}


void msh_OverwriteVariable(IndexedVariable_t* indexed_var, const mxArray* in_var, int will_sync)
{
	int               field_num, in_num_fields, is_complex;
	size_t            i, j, dest_idx, in_idx, nzmax, elem_size, dest_offset, in_offset;
	byte_T*           dest_data, * in_data, * dest_imag_data, * in_imag_data;
	const char_T*     curr_field_name;
	
	size_t            in_num_elems    = mxGetNumberOfElements(in_var);
	mxClassID         shared_class_id = mxGetClassID(in_var);
	IndexedVariable_t sub_variable    = {0};
	
	/* struct case */
	if(shared_class_id == mxSTRUCT_CLASS)
	{
		in_num_fields = mxGetNumberOfFields(in_var);
		
		/* go through each element */
		if(indexed_var->indices.start_idxs == NULL)
		{
			for(field_num = 0; field_num < in_num_fields; field_num++)     /* each field */
			{
				curr_field_name = mxGetFieldNameByNumber(indexed_var->dest_var, field_num);
				for(dest_idx = 0; dest_idx < in_num_elems; dest_idx++)
				{
					sub_variable.dest_var = mxGetField(indexed_var->dest_var, dest_idx, curr_field_name);
					msh_OverwriteVariable(&sub_variable, mxGetField(in_var, dest_idx, curr_field_name), will_sync);
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
						for(dest_idx = indexed_var->indices.start_idxs[i+j]; dest_idx < indexed_var->indices.start_idxs[i+j] + indexed_var->indices.slice_lens[j]; dest_idx++, in_idx++)
						{
							/* this inner conditional should be optimized out by the compiler */
							sub_variable.dest_var = mxGetField(indexed_var->dest_var, dest_idx, curr_field_name);
							msh_OverwriteVariable(&sub_variable, mxGetField(in_var, (in_num_elems == 1)? 0 : in_idx, curr_field_name), will_sync);
						}
					}
				}
			}
		}
	}
	else if(shared_class_id == mxCELL_CLASS) /* cell case */
	{
		
		/* go through each element */
		if(indexed_var->indices.start_idxs == NULL)
		{
			for(dest_idx = 0; dest_idx < in_num_elems; dest_idx++)
			{
				sub_variable.dest_var = mxGetCell(indexed_var->dest_var, dest_idx);
				msh_OverwriteVariable(&sub_variable, mxGetCell(in_var, dest_idx), will_sync);
			}
		}
		else
		{
			for(i = 0, in_idx = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
			{
				for(j = 0; j < indexed_var->indices.num_lens; j++)
				{
					for(dest_idx = indexed_var->indices.start_idxs[i+j]; dest_idx < indexed_var->indices.start_idxs[i+j] + indexed_var->indices.slice_lens[j]; dest_idx++, in_idx++)
					{
						/* this inner conditional should be optimized out by the compiler */
						sub_variable.dest_var = mxGetCell(indexed_var->dest_var, dest_idx);
						msh_OverwriteVariable(&sub_variable, mxGetCell(in_var, (in_num_elems == 1)? 0 : in_idx), will_sync);
					}
				}
			}
		}
	}
	else     /*base case*/
	{
		
		is_complex = mxIsComplex(in_var);
		
		if(mxIsSparse(in_var))
		{
			
			nzmax = mxGetNzmax(in_var);
			
			if(will_sync) msh_AcquireProcessLock(g_process_lock);
			
			memcpy(mxGetIr(indexed_var->dest_var), mxGetIr(in_var), nzmax*sizeof(mwIndex));
			memcpy(mxGetJc(indexed_var->dest_var), mxGetJc(in_var), (mxGetN(in_var) + 1)*sizeof(mwIndex));
			memcpy(mxGetData(indexed_var->dest_var), mxGetData(in_var), nzmax*mxGetElementSize(in_var));
			if(is_complex) memcpy(mxGetImagData(indexed_var->dest_var), mxGetImagData(in_var), nzmax*mxGetElementSize(in_var));
			
			if(will_sync) msh_ReleaseProcessLock(g_process_lock);
			
		}
		else if(!mxIsEmpty(in_var))
		{
			
			elem_size = mxGetElementSize(indexed_var->dest_var);
			
			if(indexed_var->indices.start_idxs == NULL)
			{
				in_num_elems = mxGetNumberOfElements(in_var);
				
				if(will_sync) msh_AcquireProcessLock(g_process_lock);
				
				memcpy(mxGetData(indexed_var->dest_var), mxGetData(in_var), in_num_elems*elem_size);
				if(is_complex) memcpy(mxGetImagData(indexed_var->dest_var), mxGetImagData(in_var), in_num_elems*elem_size);
				
				if(will_sync) msh_ReleaseProcessLock(g_process_lock);
				
			}
			else
			{
				
				dest_data = mxGetData(indexed_var->dest_var);
				in_data = mxGetData(in_var);
				
				dest_imag_data = mxGetImagData(indexed_var->dest_var);
				in_imag_data = mxGetImagData(in_var);
				
				if(will_sync) msh_AcquireProcessLock(g_process_lock);
				
				for(i = 0, in_offset = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
				{
					for(j = 0; j < indexed_var->indices.num_lens; j++)
					{
						dest_offset = indexed_var->indices.start_idxs[i+j]*elem_size/sizeof(byte_T);
						
						/* optimized out */
						if(in_num_elems == 1)
						{
							for(dest_idx = 0; dest_idx < indexed_var->indices.slice_lens[j]; dest_idx++)
							{
								memcpy(dest_data + dest_offset + dest_idx*elem_size/sizeof(byte_T), in_data, elem_size);
								if(is_complex) memcpy(dest_imag_data + dest_offset + dest_idx*elem_size/sizeof(byte_T), in_imag_data, elem_size);
							}
						}
						else
						{
							memcpy(dest_data + dest_offset, in_data + in_offset, indexed_var->indices.slice_lens[j]*elem_size);
							if(is_complex) memcpy(dest_imag_data + dest_offset, in_imag_data + in_offset, indexed_var->indices.slice_lens[j]*elem_size);
							in_offset += indexed_var->indices.slice_lens[j]*elem_size/sizeof(byte_T);
						}
						
						if(will_sync) msh_ReleaseProcessLock(g_process_lock);
						
					}
				}
				
				if(will_sync) msh_ReleaseProcessLock(g_process_lock);
				
			}
		}
	}
}


void msh_VOAtomicAdd(IndexedVariable_t* indexed_var, const mxArray* in_var, int will_sync)
{
	size_t            i, j, dest_idx, nzmax, elem_size, dest_offset, in_offset;
	byte_T*           dest_data, * in_data, * dest_imag_data, * in_imag_data;
	
	int               is_complex = mxIsComplex(in_var);
	size_t            in_num_elems    = mxGetNumberOfElements(in_var);
	
	if(mxIsSparse(in_var))
	{
		
		nzmax = mxGetNzmax(in_var);
		
		msh_AcquireProcessLock(g_process_lock);
		
		memcpy(mxGetIr(indexed_var->dest_var), mxGetIr(in_var), nzmax*sizeof(mwIndex));
		memcpy(mxGetJc(indexed_var->dest_var), mxGetJc(in_var), (mxGetN(in_var) + 1)*sizeof(mwIndex));
		memcpy(mxGetData(indexed_var->dest_var), mxGetData(in_var), nzmax*mxGetElementSize(in_var));
		if(is_complex) memcpy(mxGetImagData(indexed_var->dest_var), mxGetImagData(in_var), nzmax*mxGetElementSize(in_var));
		
		msh_ReleaseProcessLock(g_process_lock);
		
	}
	else if(!mxIsEmpty(in_var))
	{
		
		elem_size = mxGetElementSize(indexed_var->dest_var);
		
		if(indexed_var->indices.start_idxs == NULL)
		{
			in_num_elems = mxGetNumberOfElements(in_var);
			
			msh_AcquireProcessLock(g_process_lock);
			
			msh_AddDoubleWorker(mxGetData(indexed_var->dest_var), mxGetData(in_var), in_num_elems);
			if(is_complex) memcpy(mxGetImagData(indexed_var->dest_var), mxGetImagData(in_var), in_num_elems*elem_size);
			
			msh_ReleaseProcessLock(g_process_lock);
			
		}
		else
		{
			
			dest_data = mxGetData(indexed_var->dest_var);
			in_data = mxGetData(in_var);
			
			dest_imag_data = mxGetImagData(indexed_var->dest_var);
			in_imag_data = mxGetImagData(in_var);
			
			msh_AcquireProcessLock(g_process_lock);
			
			for(i = 0, in_offset = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
			{
				for(j = 0; j < indexed_var->indices.num_lens; j++)
				{
					dest_offset = indexed_var->indices.start_idxs[i+j]*elem_size/sizeof(byte_T);
					
					msh_AcquireProcessLock(g_process_lock);
					
					/* optimized out */
					if(in_num_elems == 1)
					{
						for(dest_idx = 0; dest_idx < indexed_var->indices.slice_lens[j]; dest_idx++)
						{
							memcpy(dest_data + dest_offset + dest_idx*elem_size/sizeof(byte_T), in_data, elem_size);
							if(is_complex) memcpy(dest_imag_data + dest_offset + dest_idx*elem_size/sizeof(byte_T), in_imag_data, elem_size);
						}
					}
					else
					{
						memcpy(dest_data + dest_offset, in_data + in_offset, indexed_var->indices.slice_lens[j]*elem_size);
						if(is_complex) memcpy(dest_imag_data + dest_offset, in_imag_data + in_offset, indexed_var->indices.slice_lens[j]*elem_size);
						in_offset += indexed_var->indices.slice_lens[j]*elem_size/sizeof(byte_T);
					}
					
				}
			}
			
			msh_ReleaseProcessLock(g_process_lock);
			
		}
	}
	
}

/** meta routines **/
#define FW_INT_FCNR_NAME(OP, SIZE) msh_##OP##Int##SIZE##Runner
#define FW_INT_FCN_NAME(OP, SIZE) msh_##OP##Int##SIZE
#define FW_INT_TYPE(SIZE) int##SIZE##_T
#define FW_INT_MAX(SIZE) INT##SIZE##_MAX
#define FW_INT_MIN(SIZE) INT##SIZE##_MIN

#define FW_UINT_FCNR_NAME(OP, SIZE) msh_##OP##UInt##SIZE##Runner
#define FW_UINT_FCN_NAME(OP, SIZE) msh_##OP##UInt##SIZE
#define FW_UINT_TYPE(SIZE) uint##SIZE##_T
#define FW_UINT_MAX(SIZE) UINT##SIZE##_MAX

#define UNARY_OP_RUNNER(RNAME, NAME, TYPE_ACCUM)    \
static void RNAME(TYPE_ACCUM* in, size_t num_elems) \
{                                                   \
	size_t i;                                      \
	for(i = 0; i < num_elems; i++, in++)           \
	{                                              \
		*in = NAME(*in);                          \
	}                                              \
}

#define BINARY_OP_RUNNER(RNAME, NAME, TYPE_ACCUM, TYPE_IN)                               \
static void RNAME(TYPE_ACCUM* accum, TYPE_IN* in, size_t num_elems, int is_singular_val) \
{                                                                                        \
	size_t i;                                                                           \
	if(is_singular_val)                                                                 \
	{                                                                                   \
		for(i = 0; i < num_elems; i++, accum++)	                                       \
		{                                                                              \
			*accum = NAME(*accum, *in);                                               \
		}                                                                              \
	}                                                                                   \
	else                                                                                \
	{                                                                                   \
		for(i = 0; i < num_elems; i++, accum++, in++)                                  \
		{                                                                              \
			*accum = NAME(*accum, *in);                                               \
		}                                                                              \
	}                                                                                   \
}

/** Signed integer arithmetic
 * We assume here that ints are two's complement, and
 * that right shifts of signed negative integers preserve
 * the sign bit (thus go to -1). This should be tested
 * before compilation.
 */
 
#define UNARY_INT_OP_RUNNER(OP, SIZE) \
UNARY_OP_RUNNER(FW_INT_FCNR_NAME(OP, SIZE), FW_INT_FCN_NAME(OP, SIZE), FW_INT_TYPE(SIZE))

#define BINARY_INT_OP_RUNNER(OP, SIZE) \
BINARY_OP_RUNNER(FW_INT_FCNR_NAME(OP, SIZE), FW_INT_FCN_NAME(OP, SIZE), FW_INT_TYPE(SIZE), FW_INT_TYPE(SIZE))

/** SIGN FUNCTIONS **/
#define SGNBIT_INT_DEF(NAME, TYPE, UTYPE, SIZEM1) \
static TYPE NAME(TYPE in)                         \
{                                                 \
	return (UTYPE)in >> SIZEM1;                  \
}

#define SGNBIT_INT_METADEF(SIZE) SGNBIT_INT_DEF(FW_INT_FCN_NAME(Sgn, SIZE), FW_INT_TYPE(SIZE), FW_UINT_TYPE(SIZE), (SIZE-1));

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
	TYPE mask = in >> SIZEM1;                   \
	out = (in ^ mask) - mask;                   \
	if(out < 0)                                 \
	{                                           \
		return MAX_VAL;                        \
	}                                           \
	return out;                                 \
}

#define ABS_INT_METADEF(SIZE)                                                           \
ABS_INT_DEF(FW_INT_FCN_NAME(Abs, SIZE), FW_INT_TYPE(SIZE), (SIZE-1), FW_INT_MAX(SIZE)); \
UNARY_INT_OP_RUNNER(Abs, SIZE);

ABS_INT_METADEF(8);
ABS_INT_METADEF(16);
ABS_INT_METADEF(32);
#if MSH_BITNESS==64
ABS_INT_METADEF(64);
#endif

/** SIGNED ADDITION **/

#define ADD_INT_DEF(NAME, TYPE, UTYPE, SIZEM1, MAX_VAL) \
static TYPE NAME(TYPE augend, TYPE addend)                            \
{                                                                     \
	TYPE uadd = (UTYPE)augend + (UTYPE)addend;                       \
	TYPE flip1 = uadd ^ augend;                                      \
	TYPE flip2 = uadd ^ addend;                                      \
	if((flip1 & flip2) < 0)                                          \
	{                                                                \
		return (TYPE)MAX_VAL  + (~uadd >> SIZEM1);                  \
	}                                                                \
	return uadd;                                                     \
}

#define ADD_INT_METADEF(SIZE)                                                                               \
ADD_INT_DEF(FW_INT_FCN_NAME(Add, SIZE), FW_INT_TYPE(SIZE), FW_UINT_TYPE(SIZE), (SIZE-1), FW_INT_MAX(SIZE)); \
BINARY_INT_OP_RUNNER(Add, SIZE);

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
		return (TYPE)MAX_VAL  + (~usub >> SIZEM1);    \
	}                                                  \
	return usub;                                       \
}

#define SUB_INT_METADEF(SIZE)                                                                               \
SUB_INT_DEF(FW_INT_FCN_NAME(Sub, SIZE), FW_INT_TYPE(SIZE), FW_UINT_TYPE(SIZE), (SIZE-1), FW_INT_MAX(SIZE)); \
BINARY_INT_OP_RUNNER(Sub, SIZE);

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
	if(promoted_mul > MAX_VAL)                          \
	{                                                   \
		return MAX_VAL;                                \
	}                                                   \
	else if(promoted_mul < MIN_VAL)                     \
	{                                                   \
		return MIN_VAL;                                \
	}                                                   \
	return (TYPE)promoted_mul;                          \
}

#define MUL_INT_METADEF(SIZE, PSIZE)                                                                                \
MUL_INT_DEF(FW_INT_FCN_NAME(Mul, SIZE), FW_INT_TYPE(SIZE), FW_INT_TYPE(PSIZE), FW_INT_MAX(SIZE), FW_INT_MIN(SIZE)); \
BINARY_INT_OP_RUNNER(Mul, SIZE);

MUL_INT_METADEF(8, 16);
MUL_INT_METADEF(16, 32);
#if MSH_BITNESS==32

static int32_T msh_MulInt32(int32_T m1, int32_T m2)
{
	uint32_T uret;
	uint32_T m1a_l16, m2a_l16;
	uint32_T m1au_m2al, m1al_m2al, m1al_m2au;
	
	int      ret_is_pos   = ((m1 < 0) == (m2 < 0));
	
	uint32_T m1_abs       = (uint32_T)FW_INT_FCN_NAME(Abs, 32)(m1);
	uint32_T m2_abs       = (uint32_T)FW_INT_FCN_NAME(Abs, 32)(m2);
	
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

BINARY_INT_OP_RUNNER(32);

#else

MUL_INT_METADEF(32, 64);

static int64_T msh_MulInt64(int64_T m1, int64_T m2)
{
	uint64_T uret;
	uint64_T m1a_l32, m2a_l32;
	uint64_T m1au_m2al, m1al_m2al, m1al_m2au;
	
	int      ret_is_pos   = ((m1 < 0) == (m2 < 0));
	
	uint64_T m1_abs       = (uint64_T)FW_INT_FCN_NAME(Abs, 64)(m1);
	uint64_T m2_abs       = (uint64_T)FW_INT_FCN_NAME(Abs, 64)(m2);
	
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

BINARY_INT_OP_RUNNER(Mul, 64);

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
		if(denom == -1 && numer == MIN_VAL)                                    \
		{                                                                      \
			return MAX_VAL;                                                   \
		}                                                                      \
		else                                                                   \
		{                                                                      \
			ret = numer/denom;                                                \
			overflow_check = -FW_INT_FCN_NAME(Abs, SIZE)(numer % denom);      \
			if(overflow_check <= denom - overflow_check)                      \
			{                                                                 \
				return ret - (1 - (FW_INT_FCN_NAME(Sgn, SIZE)(numer) << 1)); \
			}                                                                 \
		}                                                                      \
	}                                                                           \
	else                                                                        \
	{                                                                           \
		ret = numer/denom;                                                     \
		overflow_check = FW_INT_FCN_NAME(Abs, SIZE)(numer % denom);            \
		if(overflow_check >= denom - overflow_check)                           \
		{                                                                      \
			return ret + (1 - (FW_INT_FCN_NAME(Sgn, SIZE)(numer) << 1));      \
		}                                                                      \
	}                                                                           \
}

#define DIV_INT_METADEF(SIZE)                                                                         \
DIV_INT_DEF(FW_INT_FCN_NAME(Div, SIZE), SIZE, FW_INT_TYPE(SIZE), FW_INT_MAX(SIZE), FW_INT_MIN(SIZE)); \
BINARY_INT_OP_RUNNER(Div, SIZE);

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
REM_INT_DEF(FW_INT_FCN_NAME(Rem, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_INT_OP_RUNNER(Rem, SIZE);

REM_INT_METADEF(8);
REM_INT_METADEF(16);
REM_INT_METADEF(32);
#if MSH_BITNESS==64
REM_INT_METADEF(64);
#endif

/** SIGNED MODULUS **/

#define MOD_INT_DEF(NAME, TYPE)                                   \
static TYPE NAME(TYPE numer, TYPE denom)                          \
{                                                                 \
	TYPE rem;                                                    \
	if(denom != 0)                                               \
	{                                                            \
		rem = numer%denom;                                      \
		return ((rem < 0) != (denom < 0))? (rem + denom) : rem; \
	}                                                            \
	return numer;                                                \
}

#define MOD_INT_METADEF(SIZE)                               \
MOD_INT_DEF(FW_INT_FCN_NAME(Mod, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_INT_OP_RUNNER(Mod, SIZE);

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
	if(in == MIN_VAL)                            \
	{                                            \
		return MAX_VAL;                         \
	}                                            \
	return -in;                                  \
}

#define NEG_INT_METADEF(SIZE)                                                                   \
NEG_INT_DEF(FW_INT_FCN_NAME(Neg, SIZE), FW_INT_TYPE(SIZE), FW_INT_MAX(SIZE), FW_INT_MIN(SIZE)); \
UNARY_INT_OP_RUNNER(Neg, SIZE);

NEG_INT_METADEF(8);
NEG_INT_METADEF(16);
NEG_INT_METADEF(32);
#if MSH_BITNESS==64
NEG_INT_METADEF(64);
#endif

/** SIGNED RIGHT SHIFT **/

#define SRA_INT_DEF(NAME, TYPE)          \
static TYPE NAME(TYPE in, int num_shift) \
{                                        \
	return in >> num_shift;             \
}

#define SRA_INT_METADEF(SIZE)                               \
SRA_INT_DEF(FW_INT_FCN_NAME(SRA, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_OP_RUNNER(FW_INT_FCNR_NAME(SRA, SIZE), FW_INT_FCN_NAME(SRA, SIZE), FW_INT_TYPE(SIZE), int);

SRA_INT_METADEF(8);
SRA_INT_METADEF(16);
SRA_INT_METADEF(32);
#if MSH_BITNESS==64
SRA_INT_METADEF(64);
#endif

/** SIGNED LEFT SHIFT **/

#define SLA_INT_DEF(NAME, TYPE)          \
static TYPE NAME(TYPE in, int num_shift) \
{                                        \
	return in << num_shift;             \
}

#define SLA_INT_METADEF(SIZE)                               \
SLA_INT_DEF(FW_INT_FCN_NAME(SLA, SIZE), FW_INT_TYPE(SIZE)); \
BINARY_OP_RUNNER(FW_INT_FCNR_NAME(SLA, SIZE), FW_INT_FCN_NAME(SLA, SIZE), FW_INT_TYPE(SIZE), int);

SLA_INT_METADEF(8);
SLA_INT_METADEF(16);
SLA_INT_METADEF(32);
#if MSH_BITNESS==64
SLA_INT_METADEF(64);
#endif

/** Unsigned integer arithmetic
 *  The functions here are pretty straightforward, except
 *  for uint64 multiplication.
 */

#define UNARY_UINT_OP_RUNNER(OP, SIZE) \
UNARY_OP_RUNNER(FW_UINT_FCNR_NAME(OP, SIZE), FW_UINT_FCN_NAME(OP, SIZE), FW_UINT_TYPE(SIZE))

#define BINARY_UINT_OP_RUNNER(OP, SIZE) \
BINARY_OP_RUNNER(FW_UINT_FCNR_NAME(OP, SIZE), FW_UINT_FCN_NAME(OP, SIZE), FW_UINT_TYPE(SIZE), FW_UINT_TYPE(SIZE))

/** UNSIGNED ADDITION **/

#define ADD_UINT_DEF(NAME, TYPE, MAX_VAL)  \
static TYPE NAME(TYPE augend, TYPE addend) \
{                                          \
	TYPE ret = augend + addend;           \
	return (ret < augend)? MAX_VAL : ret; \
}

#define ADD_UINT_METADEF(SIZE)                                                                               \
ADD_UINT_DEF(FW_UINT_FCN_NAME(Add, SIZE), FW_UINT_TYPE(SIZE), FW_UINT_MAX(SIZE)); \
BINARY_UINT_OP_RUNNER(Add, SIZE);

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
SUB_UINT_DEF(FW_UINT_FCN_NAME(Sub, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_UINT_OP_RUNNER(Sub, SIZE);

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
	if(promoted_mul > MAX_VAL)                      \
	{                                               \
		return MAX_VAL;                            \
	}                                               \
	return (TYPE)promoted_mul;                      \
}

#define MUL_UINT_METADEF(SIZE, PSIZE)                                                                  \
MUL_UINT_DEF(FW_UINT_FCN_NAME(Mul, SIZE), FW_UINT_TYPE(SIZE), FW_UINT_TYPE(PSIZE), FW_UINT_MAX(SIZE)); \
BINARY_UINT_OP_RUNNER(Mul, SIZE);

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
		
		return FW_UINT_FCN_NAME(Add, 32)(m1u_m2l, m1l_m2l);
		
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
		
		return FW_UINT_FCN_NAME(Add, 32)(m1l_m2l, m1l_m2u);
		
	}
	else
	{
		return m1 * m2;
	}
	
}

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
		
		return FW_UINT_FCN_NAME(Add, 64)(m1u_m2l, m1l_m2l);
		
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
		
		return FW_UINT_FCN_NAME(Add, 64)(m1l_m2l, m1l_m2u);
		
	}
	else
	{
		return m1 * m2;
	}
	
}

#endif

/** UNSIGNED DIVISION (ROUNDED) **/

#define DIV_UINT_DEF(NAME, TYPE, MAX_VAL)        \
static TYPE NAME(TYPE numer, TYPE denom)         \
{                                                \
	TYPE ret, rem;                              \
	if(denom == 0)                              \
	{                                           \
		return numer? MAX_VAL : 0;             \
	}                                           \
	ret = numer/denom;                          \
	rem = numer%denom;                          \
	return (rem >= denom - rem)? ret + 1 : ret; \
}

#define DIV_UINT_METADEF(SIZE)                                                                         \
DIV_UINT_DEF(FW_UINT_FCN_NAME(Div, SIZE), FW_UINT_TYPE(SIZE), FW_UINT_MAX(SIZE)); \
BINARY_UINT_OP_RUNNER(Div, SIZE);

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
REM_UINT_DEF(FW_UINT_FCN_NAME(Rem, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_UINT_OP_RUNNER(Rem, SIZE);

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
MOD_UINT_DEF(FW_UINT_FCN_NAME(Mod, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_UINT_OP_RUNNER(Mod, SIZE);

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
NEG_UINT_DEF(FW_UINT_FCN_NAME(Neg, SIZE), FW_UINT_TYPE(SIZE)); \
UNARY_UINT_OP_RUNNER(Neg, SIZE);

NEG_UINT_METADEF(8);
NEG_UINT_METADEF(16);
NEG_UINT_METADEF(32);
#if MSH_BITNESS==64
NEG_UINT_METADEF(64);
#endif

/** UNSIGNED RIGHT SHIFT **/

#define SRA_UINT_DEF(NAME, TYPE)              \
static TYPE NAME(TYPE in, unsigned num_shift) \
{                                             \
	return in >> num_shift;                  \
}

#define SRA_UINT_METADEF(SIZE)                                 \
SRA_UINT_DEF(FW_UINT_FCN_NAME(SRA, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_OP_RUNNER(FW_UINT_FCNR_NAME(SRA, SIZE), FW_UINT_FCN_NAME(SRA, SIZE), FW_UINT_TYPE(SIZE), unsigned);

SRA_UINT_METADEF(8);
SRA_UINT_METADEF(16);
SRA_UINT_METADEF(32);
#if MSH_BITNESS==64
SRA_UINT_METADEF(64);
#endif

/** UNSIGNED LEFT SHIFT **/

#define SLA_UINT_DEF(NAME, TYPE)              \
static TYPE NAME(TYPE in, unsigned num_shift) \
{                                             \
	return in << num_shift;                  \
}

#define SLA_UINT_METADEF(SIZE)                                 \
SLA_UINT_DEF(FW_UINT_FCN_NAME(SLA, SIZE), FW_UINT_TYPE(SIZE)); \
BINARY_OP_RUNNER(FW_UINT_FCNR_NAME(SLA, SIZE), FW_UINT_FCN_NAME(SLA, SIZE), FW_UINT_TYPE(SIZE), unsigned);

SLA_UINT_METADEF(8);
SLA_UINT_METADEF(16);
SLA_UINT_METADEF(32);
#if MSH_BITNESS==64
SLA_UINT_METADEF(64);
#endif


/** Floating point arithmetic
 * These are just derived directly from stdlib,
 * so no need for the single value subfunctions.
 */
 
/** mxSingle **/

static void msh_AddSingleRunner(mxSingle* accum, mxSingle* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum += *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum += *in;
		}
	}
}

static void msh_SubSingleRunner(mxSingle* accum, mxSingle* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum -= *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum -= *in;
		}
	}
}

static void msh_MulSingleRunner(mxSingle* accum, mxSingle* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum *= *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum *= *in;
		}
	}
}

static void msh_DivSingleRunner(mxSingle* accum, mxSingle* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum /= *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum /= *in;
		}
	}
}

static void msh_RemSingleRunner(mxSingle* accum, mxSingle* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum = fmodf(*accum, *in);
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum = fmodf(*accum, *in);
		}
	}
}

static void msh_ModSingleRunner(mxSingle* accum, mxSingle* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	mxSingle rem;
	if(is_singular_val)
	{
		if(*in == 0)
		{
			return;
		}
		
		for(i = 0; i < num_elems; i++, accum++)
		{
			rem = fmodf(*accum, *in);
			*accum = ((rem < 0) != (*in < 0))? rem + *in : *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			if(*in != 0)
			{
				rem = fmodf(*accum, *in);
				*accum = ((rem < 0) != (*in < 0))? rem + *in : *in;
			}
		}
	}
}

static void msh_NegSingleRunner(mxSingle* accum, size_t num_elems)
{
	size_t i;
	for(i = 0; i < num_elems; i++, accum++)
	{
		*accum = -*accum;
	}
}

static void msh_SRASingleRunner(mxSingle* accum, int* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	mxSingle mult;
	if(is_singular_val)
	{
		mult = powf(2, *in);
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum /= mult;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum /= powf(2, *in);
		}
	}
}

static void msh_SLASingleRunner(mxSingle* accum, int* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	mxSingle mult;
	if(is_singular_val)
	{
		mult = powf(2, *in);
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum *= mult;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum *= powf(2, *in);
		}
	}
}

/** mxDouble **/

static void msh_AddDoubleRunner(mxDouble* accum, mxDouble* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum += *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum += *in;
		}
	}
}

static void msh_SubDoubleRunner(mxDouble* accum, mxDouble* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum -= *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum -= *in;
		}
	}
}

static void msh_MulDoubleRunner(mxDouble* accum, mxDouble* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum *= *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum *= *in;
		}
	}
}

static void msh_DivDoubleRunner(mxDouble* accum, mxDouble* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum /= *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum /= *in;
		}
	}
}

static void msh_RemDoubleRunner(mxDouble* accum, mxDouble* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	if(is_singular_val)
	{
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum = fmodf(*accum, *in);
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum = fmodf(*accum, *in);
		}
	}
}

static void msh_ModDoubleRunner(mxDouble* accum, mxDouble* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	mxDouble rem;
	if(is_singular_val)
	{
		if(*in == 0)
		{
			return;
		}
		
		for(i = 0; i < num_elems; i++, accum++)
		{
			rem = fmodf(*accum, *in);
			*accum = ((rem < 0) != (*in < 0))? rem + *in : *in;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			if(*in != 0)
			{
				rem = fmodf(*accum, *in);
				*accum = ((rem < 0) != (*in < 0))? rem + *in : *in;
			}
		}
	}
}

static void msh_NegDoubleRunner(mxDouble* accum, size_t num_elems)
{
	size_t i;
	for(i = 0; i < num_elems; i++, accum++)
	{
		*accum = -*accum;
	}
}

static void msh_SRADoubleRunner(mxDouble* accum, int* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	mxDouble mult;
	if(is_singular_val)
	{
		mult = powf(2, *in);
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum /= mult;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum /= powf(2, *in);
		}
	}
}

static void msh_SLADoubleRunner(mxDouble* accum, int* in, size_t num_elems, int is_singular_val)
{
	size_t i;
	mxDouble mult;
	if(is_singular_val)
	{
		mult = powf(2, *in);
		for(i = 0; i < num_elems; i++, accum++)
		{
			*accum *= mult;
		}
	}
	else
	{
		for(i = 0; i < num_elems; i++, accum++, in++)
		{
			*accum *= powf(2, *in);
		}
	}
}
