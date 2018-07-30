/** mshheader.c
 * Provides a definition for the shared memory header and
 * defines access functions. Also defines functions for
 * placing variables into shared memory.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "mex.h"

#include "mshheader.h"
#include "mshvariables.h"
#include "mlerrorutils.h"
#include "mshexterntypes.h"


#ifdef MSH_UNIX
#include <string.h>
#endif

#if MSH_BITNESS == 64
#  define ALLOCATION_HEADER_MAGIC_CHECK 0xFEEDFACE
#  define ALLOCATION_HEADER_SIZE 0x10
#  define ALLOCATION_HEADER_SIZE_SHIFT 0x0F

typedef struct AllocationHeader_t
{
	uint64_T aligned_size;
	uint32_T check;
	uint16_T alignment;
	uint16_T offset;
} AllocationHeader_t;

#elif MSH_BITNESS == 32

#  define ALLOCATION_HEADER_MAGIC_CHECK 0xFEED
#  define ALLOCATION_HEADER_SIZE 0x08
#  define ALLOCATION_HEADER_SIZE_SHIFT 0x07

typedef struct AllocationHeader_t
{
	uint32_T aligned_size;
	uint16_T check;
	uint8_T alignment;
	uint8_T offset;
} AllocationHeader_t;

#else
#  error(matshare is only supported in 64-bit and 32-bit variants.)
#endif


/**
 * Close emulation of the structure of mxArray:
 * A variable shall be packed as so: [{SharedVariableHeader_t, dims}, (child offsets, field names, data, imag_data, ir, jc)] where {} is required, () is optional.
 * This is packed to be <= 64 bytes on x64 so that we can back a 2 dimensional array without any padding (64 byte header, 16 bytes for dimensions,
 * 16 bytes for the mxMalloc signature. Then aligned at 96 bytes.).
 */
struct SharedVariableHeader_t
{
	struct
	{
		size_t data;
		size_t imag_data;
		union ir_fn_u
		{
			size_t ir;
			size_t field_names;
		} ir_fn;
		union jc_co_u
		{
			size_t jc;
			size_t child_offs; /* offset of array of the offsets of the children*/
		} jc_co;
	} data_offsets;               /* these are actually the relative offsets of data in shared memory (needed because memory maps are to virtual pointers) */
	size_t num_dims;          /* dimensionality of the matrix */
	size_t elem_size;               /* size of each element in data and imag_data */
	union ne_nz_u
	{
		size_t num_elems;
		size_t nzmax;
	} ne_nz;
	int num_fields;       /* the number of fields.  The field string immediately follows the size array */
	struct class_info_pack_tag
	{
		int32_T class_id          : 8;
		int32_T is_empty          : 8;
		int32_T is_sparse         : 8;
		int32_T is_numeric        : 8;
	} class_info_pack;
};

extern size_t msh_PadToAlignData(size_t curr_sz);

/**
 * Gets the total number of bytes needed to store the fields strings.
 *
 * @param in_var The input variable.
 * @return The total bytes needed.
 */
static size_t msh_GetFieldNamesSize(const mxArray* in_var);


/**
 * Gets the next field name.
 *
 * @param field_str A pointer to the field string.
 */
static void msh_GetNextFieldName(const char_t** field_str);


/**
 * Creates the allocation signature in the style of mxMalloc.
 *
 * @param alloc_hdr A pointer to the allocation header to be modified.
 * @param copy_sz The size of the data copy.
 */
static void msh_MakeAllocationHeader(AllocationHeader_t* alloc_hdr, size_t copy_sz);


/**
 * Finds the padded size of the copied data.
 *
 * @note Pads to a multiple of the size of the header, so 64 bit is 16 byte padded, 32 bit is 8 byte padded.
 * @param copy_sz The size of the data copy.
 * @return The padded size.
 */
static size_t FindPaddedDataSize(size_t copy_sz);


/** offset Get functions **/

size_t msh_GetDataOffset(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.data;
}


size_t msh_GetImagDataOffset(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.imag_data;
}


size_t msh_GetIrOffset(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.ir_fn.ir;
}


size_t msh_GetFieldNamesOffset(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.ir_fn.field_names;
}


size_t msh_GetJcOffset(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.jc_co.jc;
}


size_t msh_GetChildOffsOffset(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.jc_co.child_offs;
}


/** attribute Get functions **/

size_t msh_GetNumDims(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->num_dims;
}


size_t msh_GetElemSize(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->elem_size;
}


size_t msh_GetNumElems(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->ne_nz.num_elems;
}


size_t msh_GetNzmax(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->ne_nz.nzmax;
}


int msh_GetNumFields(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->num_fields;
}


int msh_GetClassID(SharedVariableHeader_t* hdr_ptr)
{
	return (mxClassID)hdr_ptr->class_info_pack.class_id;
}


int msh_GetIsEmpty(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->class_info_pack.is_empty;
}


int msh_GetIsSparse(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->class_info_pack.is_sparse;
}


int msh_GetIsNumeric(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->class_info_pack.is_numeric;
}



/** offset Set functions **/


void msh_SetDataOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off)
{
	hdr_ptr->data_offsets.data = new_off;
}


void msh_SetImagDataOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off)
{
	hdr_ptr->data_offsets.imag_data = new_off;
}


void msh_SetIrOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off)
{
	hdr_ptr->data_offsets.ir_fn.ir = new_off;
}


void msh_SetFieldNamesOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off)
{
	hdr_ptr->data_offsets.ir_fn.field_names = new_off;
}


void msh_SetJcOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off)
{
	hdr_ptr->data_offsets.jc_co.jc = new_off;
}


void msh_SetChildOffsOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off)
{
	hdr_ptr->data_offsets.jc_co.child_offs = new_off;
}


/** attribute Set functions **/


void msh_SetNumDims(SharedVariableHeader_t* hdr_ptr, size_t in)
{
	hdr_ptr->num_dims = in;
}


void msh_SetElemSize(SharedVariableHeader_t* hdr_ptr, size_t in)
{
	hdr_ptr->elem_size = in;
}


void msh_SetNumElems(SharedVariableHeader_t* hdr_ptr, size_t in)
{
	hdr_ptr->ne_nz.num_elems = in;
}


void msh_SetNzmax(SharedVariableHeader_t* hdr_ptr, size_t in)
{
	hdr_ptr->ne_nz.nzmax = in;
}


void msh_SetNumFields(SharedVariableHeader_t* hdr_ptr, int in)
{
	hdr_ptr->num_fields = in;
}


void msh_SetClassId(SharedVariableHeader_t* hdr_ptr, int in)
{
	hdr_ptr->class_info_pack.class_id = in;
}


void msh_SetIsEmpty(SharedVariableHeader_t* hdr_ptr, int in)
{
	hdr_ptr->class_info_pack.is_empty = in;
}


void msh_SetIsSparse(SharedVariableHeader_t* hdr_ptr, int in)
{
	hdr_ptr->class_info_pack.is_sparse = in;
}


void msh_SetIsNumeric(SharedVariableHeader_t* hdr_ptr, int in)
{
	hdr_ptr->class_info_pack.is_numeric = in;
}


/** data pointer accessors */

mwSize* msh_GetDimensions(SharedVariableHeader_t* hdr_ptr)
{
	return (mwSize*)((byte_t*)hdr_ptr + sizeof(SharedVariableHeader_t));
}


void* msh_GetData(SharedVariableHeader_t* hdr_ptr)
{
	return (byte_t*)hdr_ptr + msh_GetDataOffset(hdr_ptr);
}


void* msh_GetImagData(SharedVariableHeader_t* hdr_ptr)
{
	return (byte_t*)hdr_ptr + msh_GetImagDataOffset(hdr_ptr);
}


mwIndex* msh_GetIr(SharedVariableHeader_t* hdr_ptr)
{
	return (mwIndex*)((byte_t*)hdr_ptr + msh_GetIrOffset(hdr_ptr));
}


char_t* msh_GetFieldNames(SharedVariableHeader_t* hdr_ptr)
{
	return (char_t*)((byte_t*)hdr_ptr + msh_GetFieldNamesOffset(hdr_ptr));
}


mwIndex* msh_GetJc(SharedVariableHeader_t* hdr_ptr)
{
	return (mwIndex*)((byte_t*)hdr_ptr + msh_GetJcOffset(hdr_ptr));
}


size_t* msh_GetChildOffsets(SharedVariableHeader_t* hdr_ptr)
{
	return (size_t*)((byte_t*)hdr_ptr + msh_GetChildOffsOffset(hdr_ptr));
}


/** Miscellaneous functions utilizing inference **/

SharedVariableHeader_t* msh_GetChildHeader(SharedVariableHeader_t* hdr_ptr, size_t child_num)
{
	return (SharedVariableHeader_t*)((byte_t*)hdr_ptr + msh_GetChildOffsets(hdr_ptr)[child_num]);
}


int msh_GetIsComplex(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.imag_data != SIZE_MAX;
}


size_t msh_FindSharedSize(const mxArray* in_var)
{
	/* Note: Compiler optimization should cache the redundant function calls here because of the
	 * const declaration (and if it doesn't, then these are very light calls anyway). I don't
	 * want to use SharedVariableHeader_t for anything other than the actual segment. */
	
	/* adds the total size of the aligned data */
#define AddDataSize(obj_sz_, data_sz_) (obj_sz_) = msh_PadToAlignData((obj_sz_) + ALLOCATION_HEADER_SIZE) + (data_sz_);
	
	/* counters */
	size_t idx, count, obj_tree_sz = 0;
	int field_num;
	
	/* Add space for the header */
	obj_tree_sz += sizeof(SharedVariableHeader_t);
	
	if(mxGetNumberOfDimensions(in_var) < 2)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "UndefinedDimensionsError", "There was an unexpected number of dimensions. Make sure the array has at least two dimensions.");
	}
	
	/* Add space for the dimensions */
	obj_tree_sz += mxGetNumberOfDimensions(in_var)*sizeof(mwSize);
	
	/* Structure case */
	if(mxGetClassID(in_var) == mxSTRUCT_CLASS)
	{
		
		/* add size of storing the offsets */
		obj_tree_sz += (mxGetNumberOfFields(in_var)*mxGetNumberOfElements(in_var))*sizeof(size_t);
		
		/* Add space for the field string */
		obj_tree_sz += msh_GetFieldNamesSize(in_var);
		
		/* make sure the structure is aligned so we don't have slow access */
		obj_tree_sz = msh_PadToAlignData(obj_tree_sz);
		
		/* go through each recursively */
		for(field_num = 0, count = 0; field_num < mxGetNumberOfFields(in_var); field_num++)          /* each field */
		{
			for(idx = 0; idx < mxGetNumberOfElements(in_var); idx++, count++)                         /* each element */
			{
				/* call recursivley */
				obj_tree_sz += msh_PadToAlignData(msh_FindSharedSize(mxGetFieldByNumber(in_var, idx, field_num)));
			}
		}
	}
	else if(mxGetClassID(in_var) == mxCELL_CLASS) /* Cell case */
	{
		
		/* add size of storing the offsets */
		obj_tree_sz += mxGetNumberOfElements(in_var)*sizeof(size_t);
		
		/* make sure the structure is aligned so we don't have slow access */
		obj_tree_sz = msh_PadToAlignData(obj_tree_sz);
		
		/* go through each recursively */
		for(count = 0; count < mxGetNumberOfElements(in_var); count++)
		{
			obj_tree_sz += msh_PadToAlignData(msh_FindSharedSize(mxGetCell(in_var, count)));
		}
	}
	else if(mxIsNumeric(in_var) || mxGetClassID(in_var) == mxLOGICAL_CLASS || mxGetClassID(in_var) == mxCHAR_CLASS)  /* base case */
	{
		
		if(mxIsSparse(in_var))
		{
			/* len(data)==nzmax, len(imag_data)==nzmax, len(ir)=nzmax, len(jc)==N+1 */
			
			/* add the size of the real data */
			AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*mxGetNzmax(in_var));
			if(mxIsComplex(in_var))
			{
				/* and the imaginary data */
				AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*mxGetNzmax(in_var));
			}
			AddDataSize(obj_tree_sz, sizeof(mwIndex)*mxGetNzmax(in_var));          /* ir */
			AddDataSize(obj_tree_sz, sizeof(mwIndex)*(mxGetN(in_var) + 1)); /* jc */
			
		}
		else
		{
			/* ensure both pointers are aligned individually */
			if(!mxIsEmpty(in_var))
			{
				/* add the size of the real data */
				if(mxGetNumberOfElements(in_var) == 1)
				{
					AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*2);
					if(mxIsComplex(in_var))
					{
						/* and the imaginary data */
						AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*2);
					}
					
				}
				else
				{
					AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*mxGetNumberOfElements(in_var));
					if(mxIsComplex(in_var))
					{
						/* and the imaginary data */
						AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*mxGetNumberOfElements(in_var));
					}
				}
			}
		}
		
	}
	else
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER, "InvalidTypeError",
					   "Unexpected input type '%s'. All elements of the shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.", mxGetClassName(in_var));
	}
	
	return obj_tree_sz;
}


size_t msh_CopyVariable(void* dest, const mxArray* in_var)
{
	size_t curr_off = 0, idx, copy_sz, alloc_sz, count, num_elems;
	
	int field_num, num_fields;
	const char_t* field_name;
	char* field_name_dest;
	
	/* initialize header info */
	msh_SetDataOffset(dest, SIZE_MAX);
	msh_SetImagDataOffset(dest, SIZE_MAX);
	msh_SetIrOffset(dest, SIZE_MAX);
	msh_SetJcOffset(dest, SIZE_MAX);
	
	msh_SetNumDims(dest, mxGetNumberOfDimensions(in_var));
	msh_SetElemSize(dest, mxGetElementSize(in_var));
	msh_SetNumElems(dest, mxGetNumberOfElements(in_var));
	/* set nzmax only if this is sparse since we use a union */
	msh_SetNumFields(dest, mxGetNumberOfFields(in_var));
	msh_SetClassId(dest, mxGetClassID(in_var));
	msh_SetIsEmpty(dest, mxIsEmpty(in_var));
	msh_SetIsSparse(dest, mxIsSparse(in_var));
	msh_SetIsNumeric(dest, mxIsNumeric(in_var));
	
	/* shift to beginning of dims */
	curr_off += sizeof(SharedVariableHeader_t);
	
	/* copy the dimensions */
	copy_sz = msh_GetNumDims(dest)*sizeof(mwSize);
	memcpy(msh_GetDimensions(dest), mxGetDimensions(in_var), copy_sz);
	
	/* shift to end of dims */
	curr_off += copy_sz;
	
	/* Structure case */
	if(mxGetClassID(in_var) == mxSTRUCT_CLASS)
	{
		
		num_elems = msh_GetNumElems(dest);
		num_fields = msh_GetNumFields(dest);
		
		/* child header offsets */
		msh_SetChildOffsOffset(dest, curr_off);
		
		/* shift to end of child offs */
		curr_off += num_fields*num_elems*sizeof(size_t);
		
		/* field names */
		msh_SetFieldNamesOffset(dest, curr_off);
		
		/* shift to end of field names */
		curr_off += msh_GetFieldNamesSize(in_var);
		
		/* align this object */
		curr_off = msh_PadToAlignData(curr_off);
		
		/* copy the children recursively */
		field_name_dest = msh_GetFieldNames(dest);
		for(field_num = 0, count = 0; field_num < num_fields; field_num++)                /* the fields */
		{
			
			field_name = mxGetFieldNameByNumber(in_var, field_num);
			copy_sz = strlen(field_name) + 1;
			memcpy(field_name_dest, field_name, copy_sz);
			field_name_dest += copy_sz;
			
			for(idx = 0; idx < num_elems; idx++, count++)                                   /* the struct array indices */
			{
				/* place relative offset into shared memory */
				msh_GetChildOffsets(dest)[count] = curr_off;
				
				/* And fill it */
				curr_off += msh_PadToAlignData(msh_CopyVariable(msh_GetChildHeader(dest, count), mxGetFieldByNumber(in_var, idx, field_num)));
				
			}
			
		}
		
	}
	else if(mxGetClassID(in_var) == mxCELL_CLASS) /* Cell case */
	{
		
		num_elems = msh_GetNumElems(dest);
		
		/* store the child headers here */
		msh_SetChildOffsOffset(dest, curr_off);
		curr_off += num_elems*sizeof(size_t);
		
		/* align this object */
		curr_off = msh_PadToAlignData(curr_off);
		
		/* recurse for each cell element */
		for(count = 0; count < num_elems; count++)
		{
			/* place relative offset into shared memory */
			msh_GetChildOffsets(dest)[count] = curr_off;
			
			/* And fill it */
			curr_off += msh_PadToAlignData(msh_CopyVariable(msh_GetChildHeader(dest, count), mxGetCell(in_var, count)));
		}
		
	}
	else /* base case */
	{
		
		/* and the indices of the sparse data as well */
		if(msh_GetIsSparse(dest))
		{
			
			/* note: in this case mxIsEmpty being TRUE means the sparse is 0x0
			 *  also note that nzmax is still 1 */
			msh_SetNzmax(dest, mxGetNzmax(in_var));
			
			/** begin copy data **/
			
			/* make room for the mxMalloc signature */
			curr_off = msh_PadToAlignData(curr_off + ALLOCATION_HEADER_SIZE);
			
			/* set the offset of the data */
			msh_SetDataOffset(dest, curr_off);
			
			/* copy over the data with the signature */
			copy_sz = msh_GetNzmax(dest)*msh_GetElemSize(dest), alloc_sz = copy_sz;
			msh_MakeAllocationHeader((AllocationHeader_t*)msh_GetData(dest) - 1, alloc_sz);
			memcpy(msh_GetData(dest), mxGetData(in_var), copy_sz);
			
			/* shift to end of the data */
			curr_off += alloc_sz;
			
			/** begin copy imag_data **/
			if(mxIsComplex(in_var))
			{
				/* make room for the mxMalloc signature */
				curr_off = msh_PadToAlignData(curr_off + ALLOCATION_HEADER_SIZE);
				
				/* set the offset of the imaginary data */
				msh_SetImagDataOffset(dest, curr_off);
				
				/* copy over the data with the signature */
				msh_MakeAllocationHeader((AllocationHeader_t*)msh_GetImagData(dest) - 1, alloc_sz);
				memcpy(msh_GetImagData(dest), mxGetImagData(in_var), copy_sz);
				
				/* shift to end of the imaginary data */
				curr_off += alloc_sz;
				
			}
			
			
			/** begin copy ir **/
			
			/* make room for the mxMalloc signature */
			curr_off = msh_PadToAlignData(curr_off + ALLOCATION_HEADER_SIZE);
			
			/* set the offset of ir */
			msh_SetIrOffset(dest, curr_off);
			
			/* copy over ir with the signature */
			copy_sz = msh_GetNzmax(dest)*sizeof(mwIndex), alloc_sz = copy_sz;
			msh_MakeAllocationHeader((AllocationHeader_t*)msh_GetIr(dest) - 1, alloc_sz);
			memcpy(msh_GetIr(dest), mxGetIr(in_var), copy_sz);
			
			/* shift to end of ir */
			curr_off += alloc_sz;
			
			/** begin copy jc **/
			
			/* make room for the mxMalloc signature */
			curr_off = msh_PadToAlignData(curr_off + ALLOCATION_HEADER_SIZE);
			
			/* set the offset of jc */
			msh_SetJcOffset(dest, curr_off);
			
			/* copy over jc with the signature */
			copy_sz = (msh_GetDimensions(dest)[1] + 1)*sizeof(mwIndex), alloc_sz = copy_sz;
			msh_MakeAllocationHeader((AllocationHeader_t*)msh_GetJc(dest) - 1, alloc_sz);
			memcpy(msh_GetJc(dest), mxGetJc(in_var), copy_sz);
			
			/* shift to the end of jc */
			curr_off += alloc_sz;
			
		}
		else
		{
			/* copy data */
			if(!mxIsEmpty(in_var))
			{
				
				/* copy data */
				
				/* make room for the mxMalloc signature */
				curr_off = msh_PadToAlignData(curr_off + ALLOCATION_HEADER_SIZE);
				
				/* set the offset of the data */
				msh_SetDataOffset(dest, curr_off);
				
				/* copy over the data with the signature */
				copy_sz = msh_GetNumElems(dest)*msh_GetElemSize(dest);
				alloc_sz = copy_sz;
				
				msh_MakeAllocationHeader((AllocationHeader_t*)msh_GetData(dest) - 1, alloc_sz);
				memcpy(msh_GetData(dest), mxGetData(in_var), copy_sz);
				
				/* shift to end of the data */
				curr_off += alloc_sz;
				
				/* copy imag_data */
				if(mxIsComplex(in_var))
				{
					/* make room for the mxMalloc signature */
					curr_off = msh_PadToAlignData(curr_off + ALLOCATION_HEADER_SIZE);
					
					/* set the offset of the imaginary data */
					msh_SetImagDataOffset(dest, curr_off);
					
					/* copy over the data with the signature */
					msh_MakeAllocationHeader((AllocationHeader_t*)msh_GetImagData(dest) - 1, alloc_sz);
					memcpy(msh_GetImagData(dest), mxGetImagData(in_var), copy_sz);
					
					/* shift to end of the imaginary data */
					curr_off += alloc_sz;
					
				}
			}
		}
		
	}
	
	/* return the total size of the object with padding to align */
	return curr_off;
}


mxArray* msh_FetchVariable(SharedVariableHeader_t* shared_header)
{
	mxArray* ret_var = NULL;
	
	/* for loops */
	size_t idx, count, num_elems;
	
	
	/* for structures */
	int field_num, num_fields;
	
	const char_t** field_names;
	const char_t* field_name;
	
	mxClassID shared_class_id = (mxClassID)msh_GetClassID(shared_header);
	
	/* Structure case */
	if(shared_class_id == mxSTRUCT_CLASS)
	{
		num_elems = msh_GetNumElems(shared_header);
		num_fields = msh_GetNumFields(shared_header);
		
		field_names = mxMalloc(num_fields*sizeof(char_t*));
		field_name = msh_GetFieldNames(shared_header);
		for(field_num = 0; field_num < num_fields; field_num++, msh_GetNextFieldName(&field_name))
		{
			field_names[field_num] = field_name;
		}
		ret_var = mxCreateStructArray(msh_GetNumDims(shared_header), msh_GetDimensions(shared_header), num_fields, field_names);
		mxFree((char**)field_names);
		
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < num_elems; idx++, count++)
			{
				mxSetFieldByNumber(ret_var, idx, field_num, msh_FetchVariable(msh_GetChildHeader(shared_header, count)));
			}
		}
	}
	else if(shared_class_id == mxCELL_CLASS) /* Cell case */
	{
		num_elems = msh_GetNumElems(shared_header);
		
		ret_var = mxCreateCellArray(msh_GetNumDims(shared_header), msh_GetDimensions(shared_header));
		
		for(count = 0; count < num_elems; count++)
		{
			mxSetCell(ret_var, count, msh_FetchVariable(msh_GetChildHeader(shared_header, count)));
		}
	}
	else /*base case*/
	{
		
		if(msh_GetIsSparse(shared_header))
		{
			
			if(shared_class_id == mxDOUBLE_CLASS)
			{
				ret_var = mxCreateSparse(0, 0, 1, msh_GetComplexity(shared_header));
			}
			else if(shared_class_id == mxLOGICAL_CLASS)
			{
				ret_var = mxCreateSparseLogicalMatrix(0, 0, 1);
			}
			else
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_USER | MEU_SEVERITY_INTERNAL, "UnrecognizedTypeError",
							   "The fetched array was of class 'sparse' but not of type 'double' or 'logical'.");
			}
			
			/* free the pointers relating to sparse */
			mxFree(mxGetIr(ret_var));
			mxSetIr(ret_var, msh_GetIr(shared_header));
			
			mxFree(mxGetJc(ret_var));
			mxSetJc(ret_var, msh_GetJc(shared_header));
			
			/* set the new data */
			mxFree(mxGetData(ret_var));
			mxSetData(ret_var, msh_GetData(shared_header));
			if(msh_GetIsComplex(shared_header))
			{
				mxFree(mxGetImagData(ret_var));
				mxSetImagData(ret_var, msh_GetImagData(shared_header));
			}
			
			mxSetNzmax(ret_var, msh_GetNzmax(shared_header));
			
			mxSetDimensions(ret_var, msh_GetDimensions(shared_header), msh_GetNumDims(shared_header));
			
		}
		else
		{
			
			if(msh_GetIsNumeric(shared_header))
			{
				ret_var = mxCreateNumericArray(0, NULL, shared_class_id, msh_GetComplexity(shared_header));
			}
			else if(shared_class_id == mxLOGICAL_CLASS)
			{
				ret_var = mxCreateLogicalArray(0, NULL);
			}
			else if(shared_class_id == mxCHAR_CLASS)
			{
				ret_var = mxCreateCharArray(0, NULL);
			}
			else
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL | MEU_SEVERITY_CORRUPTION, "UnrecognizedTypeError",
							   "The fetched array was of class not of type 'numeric', 'logical', or 'char'.");
			}
			
			/* there is no data if it is empty */
			if(!msh_GetIsEmpty(shared_header))
			{
				mxFree(mxGetData(ret_var));
				mxSetData(ret_var, msh_GetData(shared_header));
				if(msh_GetIsComplex(shared_header))
				{
					mxFree(mxGetImagData(ret_var));
					mxSetImagData(ret_var, msh_GetImagData(shared_header));
				}
			}
			
			/* set up virtual scalar tracking */
			mxSetDimensions(ret_var, msh_GetDimensions(shared_header), msh_GetNumDims(shared_header));
		}
		
	}
	
	return ret_var;
	
}


void msh_OverwriteHeader(SharedVariableHeader_t* shared_header, const mxArray* in_var)
{
	size_t idx, count, num_elems, nzmax;
	
	/* for structures */
	int field_num, num_fields;                /* current field */
	
	mxClassID class_id = (mxClassID)msh_GetClassID(shared_header);
	
	/* Structure case */
	if(class_id == mxSTRUCT_CLASS)
	{
		num_elems = msh_GetNumElems(shared_header);
		num_fields = msh_GetNumFields(shared_header);
		
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < num_elems; idx++, count++)
			{
				/* And fill it */
				msh_OverwriteHeader(msh_GetChildHeader(shared_header, count), mxGetFieldByNumber(in_var, idx, field_num));
			}
		}
	}
	else if(class_id == mxCELL_CLASS) /* Cell case */
	{
		num_elems = msh_GetNumElems(shared_header);
		
		for(count = 0; count < num_elems; count++)
		{
			/* And fill it */
			msh_OverwriteHeader(msh_GetChildHeader(shared_header, count), mxGetCell(in_var, count));
		}
	}
	else     /*base case*/
	{
		
		/* if sparse get a list of the elements */
		if(msh_GetIsSparse(shared_header))
		{
			
			nzmax = msh_GetNzmax(shared_header);
			
			memcpy(msh_GetIr(shared_header), mxGetIr(in_var), nzmax*sizeof(mwIndex));
			memcpy(msh_GetJc(shared_header), mxGetJc(in_var), (mxGetN(in_var) + 1)*sizeof(mwIndex));
			
			/* rewrite real data */
			memcpy(msh_GetData(shared_header), mxGetData(in_var), nzmax*msh_GetElemSize(shared_header));
			
			/* if complex get a pointer to the complex data */
			if(msh_GetIsComplex(shared_header))
			{
				memcpy(msh_GetImagData(shared_header), mxGetImagData(in_var), nzmax*msh_GetElemSize(shared_header));
			}
		}
		else if(!msh_GetIsEmpty(shared_header))
		{
			
			num_elems = msh_GetNumElems(shared_header);
			
			/* rewrite real data */
			memcpy(msh_GetData(shared_header), mxGetData(in_var), num_elems*msh_GetElemSize(shared_header));
			
			/* if complex get a pointer to the complex data */
			if(msh_GetIsComplex(shared_header))
			{
				memcpy(msh_GetImagData(shared_header), mxGetImagData(in_var), num_elems*msh_GetElemSize(shared_header));
			}
		}
		
	}
	
}


int msh_CompareHeaderSize(SharedVariableHeader_t* shared_header, const mxArray* comp_var)
{
	
	size_t idx, count, shared_num_elems;
	
	/* for structures */
	int field_num, shared_num_fields;                /* current field */
	
	const char_t* shared_field_name;
	
	mxClassID shared_class_id = (mxClassID)msh_GetClassID(shared_header);
	
	/* can't allow differing dimensions because matlab doesn't use shared pointers to dimensions in mxArrays */
	if(shared_class_id != mxGetClassID(comp_var) || msh_GetNumDims(shared_header) != mxGetNumberOfDimensions(comp_var)
	   || memcmp(msh_GetDimensions(shared_header), mxGetDimensions(comp_var), msh_GetNumDims(shared_header)*sizeof(mwSize)) != 0)
	{
		return FALSE;
	}
	
	/* Structure case */
	if(shared_class_id == mxSTRUCT_CLASS)
	{
		
		shared_num_elems = msh_GetNumElems(shared_header);
		shared_num_fields = msh_GetNumFields(shared_header);
		
		if(shared_num_fields != mxGetNumberOfFields(comp_var) || shared_num_elems != mxGetNumberOfElements(comp_var))
		{
			return FALSE;
		}
		
		/* Go through each element */
		shared_field_name = msh_GetFieldNames(shared_header);
		for(field_num = 0, count = 0; field_num < shared_num_fields; field_num++)     /* each field */
		{
			
			if(strcmp(shared_field_name, mxGetFieldNameByNumber(comp_var, field_num)) != 0)
			{
				return FALSE;
			}
			
			for(idx = 0; idx < shared_num_elems; idx++, count++)
			{
				if(!msh_CompareHeaderSize(msh_GetChildHeader(shared_header, count), mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
			
			msh_GetNextFieldName(&shared_field_name);
			
		}
		
	}
	else if(shared_class_id == mxCELL_CLASS) /* Cell case */
	{
		shared_num_elems = msh_GetNumElems(shared_header);
		
		if(shared_num_elems != mxGetNumberOfElements(comp_var))
		{
			return FALSE;
		}
		
		for(count = 0; count < shared_num_elems; count++)
		{
			if(!msh_CompareHeaderSize(msh_GetChildHeader(shared_header, count), mxGetCell(comp_var, count)))
			{
				return FALSE;
			}
		}
	}
	else if(msh_GetIsNumeric(shared_header) || shared_class_id == mxLOGICAL_CLASS || shared_class_id == mxCHAR_CLASS)      /*base case*/
	{
		
		if(msh_GetIsComplex(shared_header) != mxIsComplex(comp_var))
		{
			return FALSE;
		}
		
		if(msh_GetIsSparse(shared_header))
		{
			if(msh_GetNzmax(shared_header) != mxGetNzmax(comp_var) || msh_GetDimensions(shared_header)[1] != mxGetN(comp_var) || !mxIsSparse(comp_var))
			{
				return FALSE;
			}
		}
		else
		{
			if(msh_GetNumElems(shared_header) != mxGetNumberOfElements(comp_var) || mxIsSparse(comp_var))
			{
				return FALSE;
			}
		}
		
	}
	
	return TRUE;
	
}


void msh_DetachVariable(mxArray* ret_var)
{
	mxArray* link;
	size_t idx, num_elems;
	int field_num, num_fields;
	mwSize new_dims[] = {0, 0};
	mwSize num_dims = 2, new_nzmax = 0;
	void* new_data = NULL, * new_imag_data = NULL;
	mwIndex* new_ir = NULL, * new_jc = NULL;
	
	/* restore matlab  memory */
	if(ret_var == NULL)
	{
		return;
	}
	else if(mxIsStruct(ret_var))
	{
		/* detach each field for each element */
		num_elems = mxGetNumberOfElements(ret_var);
		num_fields = mxGetNumberOfFields(ret_var);
		for(field_num = 0; field_num < num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < num_elems; idx++)                    /* each element */
			{
				msh_DetachVariable(mxGetFieldByNumber(ret_var, idx, field_num));
			}
		}
	}
	else if(mxIsCell(ret_var))
	{
		/* detach each cell */
		num_elems = mxGetNumberOfElements(ret_var);
		for(idx = 0; idx < num_elems; idx++)
		{
			msh_DetachVariable(mxGetCell(ret_var, idx));
		}
		
	}
	else if(mxIsNumeric(ret_var) || mxIsLogical(ret_var) || mxIsChar(ret_var))  /* a matrix containing data */
	{
		
		/* handle sparse objects */
		if(mxIsSparse(ret_var))
		{
			new_nzmax = 1;
			
			/* allocate 1 element */
			new_data = mxCalloc(new_nzmax, mxGetElementSize(ret_var));
			if(mxIsComplex(ret_var))
			{
				new_imag_data = mxCalloc(new_nzmax, mxGetElementSize(ret_var));
			}
			
			new_ir = mxCalloc(new_nzmax, sizeof(mwIndex));
			new_jc = mxCalloc(new_dims[1] + 1, sizeof(mwIndex));
		}
		
		
		/** HACK **/
		/* reset all the crosslinks so nothing in MATLAB is pointing to shared data (which will be gone soon) */
		link = ret_var;
		do
		{
			mxSetData(link, new_data);
			if(mxIsComplex(link))
			{
				mxSetImagData(link, new_imag_data);
			}
			
			if(mxIsSparse(link))
			{
				mxSetNzmax(link, new_nzmax);
				mxSetIr(link, new_ir);
				mxSetJc(link, new_jc);
			}
			
			mxSetDimensions(link, new_dims, num_dims);
			
			link = met_GetCrosslink(link);
		} while(link != NULL && link != ret_var);
		
	}
	else
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_INTERNAL | MEU_SEVERITY_CORRUPTION, "InvalidTypeError", "Unsupported type. The segment may have been corrupted.");
	}
}


static size_t msh_GetFieldNamesSize(const mxArray* in_var)
{
	const char_t* field_name;
	int i, num_fields;
	size_t cml_sz = 0;
	
	/* Go through them */
	num_fields = mxGetNumberOfFields(in_var);
	for(i = 0; i < num_fields; i++)
	{
		/* This field */
		field_name = mxGetFieldNameByNumber(in_var, i);
		cml_sz += (strlen(field_name) + 1)*sizeof(char_t); /* remember to add the null termination */
	}
	
	return cml_sz;
}


static void msh_GetNextFieldName(const char_t** field_str)
{
	*field_str = *field_str + strlen(*field_str) + 1;
}


static void msh_MakeAllocationHeader(AllocationHeader_t* alloc_hdr, size_t copy_sz)
{
	/*
	 * MXMALLOC SIGNATURE INFO:
	 * 	mxMalloc adjusts the size of memory for 32 byte alignment (shifts either 16 or 32 bytes)
	 * 	it appends the following 16 byte header immediately before data segment
	 *
	 * HEADER:
	 * 		bytes 0-7   - the 16 byte aligned size of the segment
	 * 		bytes  8-11 - a signature for the vector check
	 * 		bytes 12-13 - the alignment (should be 32 bytes for newer MATLAB)
	 * 		bytes 14-15 - the offset from the original pointer to the newly aligned pointer (should be 16 or 32)
	 */
	
	alloc_hdr->aligned_size = (copy_sz > 0)? FindPaddedDataSize(copy_sz) : 0;
	alloc_hdr->check = ALLOCATION_HEADER_MAGIC_CHECK;
	alloc_hdr->alignment = DATA_ALIGNMENT;
	alloc_hdr->offset = ALLOCATION_HEADER_SIZE;
	
}


static size_t FindPaddedDataSize(size_t copy_sz)
{
	return copy_sz + (ALLOCATION_HEADER_SIZE_SHIFT - ((copy_sz - 1) & ALLOCATION_HEADER_SIZE_SHIFT));
}
