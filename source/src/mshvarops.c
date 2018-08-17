/** mshvarops.c
 * Defines functions for variable operations.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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
		
		/* Go through each element */
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
	
	/* Structure case */
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
	else if(shared_class_id == mxCELL_CLASS) /* Cell case */
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
				
				for(i = 0, in_offset = 0; i < indexed_var->indices.num_idxs; i += indexed_var->indices.num_lens)
				{
					for(j = 0; j < indexed_var->indices.num_lens; j++)
					{
						dest_offset = indexed_var->indices.start_idxs[i+j]*elem_size/sizeof(byte_T);
						
						if(will_sync) msh_AcquireProcessLock(g_process_lock);
						
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
			}
		}
		
	}
}
