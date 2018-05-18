#include "../mshheader.h"
#include "../matlabutils.h"
#include "../mshexterntypes.h"

/**
 * Close emulation of the structure of mxArray:
 * A variable shall be packed as so: [{SharedVariableHeader_t, dims}, (child offsets, field names, data, imag_data, ir, jc)] where {} is required, () is optional.
 * This is packed to be 64 bytes so that we can back a 2 dimensional array without any padding (64 byte header, 16 bytes for dimensions, 16 bytes for the mxMalloc
 * signature, and then the data is aligned at offset 96 without any padding.
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
		msh_classid_t class_id; /* matlab class id packed to save space */
		bool_t is_empty;
		bool_t is_sparse;
		bool_t is_numeric;
	} class_info_pack;
};

static size_t msh_GetFieldNamesSize(const mxArray* in_var);

static void msh_GetNextFieldName(const char_t** field_str);

static void* msh_CopyData(byte_t* dest, byte_t* orig, size_t cpy_sz);

static void msh_MakeMxMallocSignature(uint8_T* sig, size_t seg_size);

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

mxClassID msh_GetClassId(SharedVariableHeader_t* hdr_ptr)
{
	return (mxClassID)hdr_ptr->class_info_pack.class_id;
}

bool_t msh_GetIsEmpty(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->class_info_pack.is_empty;
}

bool_t msh_GetIsSparse(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->class_info_pack.is_sparse;
}

bool_t msh_GetIsNumeric(SharedVariableHeader_t* hdr_ptr)
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

void msh_SetClassId(SharedVariableHeader_t* hdr_ptr, mxClassID in)
{
	hdr_ptr->class_info_pack.class_id = (msh_classid_t)in;
}

void msh_SetIsEmpty(SharedVariableHeader_t* hdr_ptr, bool_t in)
{
	hdr_ptr->class_info_pack.is_empty = in;
}

void msh_SetIsSparse(SharedVariableHeader_t* hdr_ptr, bool_t in)
{
	hdr_ptr->class_info_pack.is_sparse = in;
}

void msh_SetIsNumeric(SharedVariableHeader_t* hdr_ptr, bool_t in)
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
	return  (mwIndex*)((byte_t*)hdr_ptr + msh_GetIrOffset(hdr_ptr));
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

bool_t msh_GetIsComplex(SharedVariableHeader_t* hdr_ptr)
{
	return hdr_ptr->data_offsets.imag_data != SIZE_MAX;
}

mxComplexity msh_GetComplexity(SharedVariableHeader_t* hdr_ptr)
{
	return msh_GetIsComplex(hdr_ptr)? mxCOMPLEX : mxREAL;
}


size_t msh_FindSharedSize(const mxArray* in_var)
{
	/* Note: Compiler optimization should cache the redundant function calls here because of the
	 * const declaration (and if it doesn't, then these are very light calls anyway). I don't
	 * want to use SharedVariableHeader_t for anything other than the actual segment. */

	/* adds the total size of the aligned data */
#define AddDataSize(obj_sz_, data_sz_) (obj_sz_) = PadToAlign((obj_sz_) + MXMALLOC_SIG_LEN) + (data_sz_);
	
	/* counters */
	size_t idx, count, obj_tree_sz = 0;
	int field_num;
	
	/* Add space for the header */
	obj_tree_sz += sizeof(SharedVariableHeader_t);
	
	/* Add space for the dimensions */
	obj_tree_sz += mxGetNumberOfDimensions(in_var)*sizeof(mwSize);
	
	/* Structure case */
	if(mxGetClassID(in_var) == mxSTRUCT_CLASS)
	{
		
		/* add size of storing the offsets */
		obj_tree_sz += (mxGetNumberOfFields(in_var)*mxGetNumberOfElements(in_var))*sizeof(size_t);
		
		/* Add space for the field string */
		obj_tree_sz += msh_GetFieldNamesSize(in_var);
		
		/* go through each recursively */
		for(field_num = 0, count = 0; field_num < mxGetNumberOfFields(in_var); field_num++)          /* each field */
		{
			for(idx = 0; idx < mxGetNumberOfElements(in_var); idx++, count++)                         /* each element */
			{
				/* call recursivley */
				obj_tree_sz += PadToAlign(msh_FindSharedSize(mxGetFieldByNumber(in_var, idx, field_num)));
			}
		}
	}
	else if(mxGetClassID(in_var) == mxCELL_CLASS) /* Cell case */
	{
		
		/* add size of storing the offsets */
		obj_tree_sz += mxGetNumberOfElements(in_var)*sizeof(size_t);
		
		/* go through each recursively */
		for(count = 0; count < mxGetNumberOfElements(in_var); count++)
		{
			obj_tree_sz += PadToAlign(msh_FindSharedSize(mxGetCell(in_var, count)));
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
				AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*mxGetNumberOfElements(in_var));
				if(mxIsComplex(in_var))
				{
					/* and the imaginary data */
					AddDataSize(obj_tree_sz, mxGetElementSize(in_var)*mxGetNumberOfElements(in_var));
				}
			}
		}
		
	}
	else
	{
		ReadMexError(__FILE__, __LINE__, "InvalidTypeError", "Unexpected input type. All elements of the shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
	}
	
	return obj_tree_sz;
}


size_t msh_CopyVariable(void* dest, const mxArray* in_var)
{
	size_t curr_off = 0, idx, cpy_sz, count, num_elems;
	
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
	cpy_sz = msh_GetNumDims(dest)*sizeof(mwSize);
	memcpy(msh_GetDimensions(dest), mxGetDimensions(in_var), cpy_sz);
	
	/* shift to end of dims */
	curr_off += cpy_sz;
	
	/* Structure case */
	if(msh_GetClassId(dest) == mxSTRUCT_CLASS)
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
		curr_off = PadToAlign(curr_off);
		
		/* copy the children recursively */
		field_name_dest = msh_GetFieldNames(dest);
		for(field_num = 0, count = 0; field_num < num_fields; field_num++)                /* the fields */
		{
			
			field_name = mxGetFieldNameByNumber(in_var, field_num);
			cpy_sz = strlen(field_name) + 1;
			memcpy(field_name_dest, field_name, cpy_sz);
			field_name_dest += cpy_sz;
			
			for(idx = 0; idx < num_elems; idx++, count++)                                   /* the struct array indices */
			{
				/* place relative offset into shared memory */
				msh_GetChildOffsets(dest)[count] = curr_off;
				
				/* And fill it */
				curr_off += PadToAlign(msh_CopyVariable(msh_GetChildHeader(dest, count), mxGetFieldByNumber(in_var, idx, field_num)));
				
			}
			
		}
		
	}
	else if(msh_GetClassId(dest) == mxCELL_CLASS) /* Cell case */
	{
		
		num_elems = msh_GetNumElems(dest);
		
		/* store the child headers here */
		msh_SetChildOffsOffset(dest, curr_off);
		curr_off += num_elems*sizeof(size_t);
		
		/* align this object */
		curr_off = PadToAlign(curr_off);
		
		/* recurse for each cell element */
		for(count = 0; count < num_elems; count++)
		{
			/* place relative offset into shared memory */
			msh_GetChildOffsets(dest)[count] = curr_off;
			
			/* And fill it */
			curr_off += PadToAlign(msh_CopyVariable(msh_GetChildHeader(dest, count), mxGetCell(in_var, count)));
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
			curr_off = PadToAlign(curr_off + MXMALLOC_SIG_LEN);
			
			/* set the offset of the data */
			msh_SetDataOffset(dest, curr_off);
			
			/* copy over the data with the signature */
			cpy_sz = msh_GetNzmax(dest)*msh_GetElemSize(dest);
			msh_CopyData(msh_GetData(dest), mxGetData(in_var), cpy_sz);
			
			/* shift to end of the data */
			curr_off += cpy_sz;
			
			/** begin copy imag_data **/
			if(mxIsComplex(in_var))
			{
				/* make room for the mxMalloc signature */
				curr_off = PadToAlign(curr_off + MXMALLOC_SIG_LEN);
				
				/* set the offset of the imaginary data */
				msh_SetImagDataOffset(dest, curr_off);
				
				/* copy over the data with the signature */
				msh_CopyData(msh_GetImagData(dest), mxGetImagData(in_var), cpy_sz);
				
				/* shift to end of the imaginary data */
				curr_off += cpy_sz;
				
			}
			
			
			/** begin copy ir **/
			
			/* make room for the mxMalloc signature */
			curr_off = PadToAlign(curr_off + MXMALLOC_SIG_LEN);
			
			/* set the offset of ir */
			msh_SetIrOffset(dest, curr_off);
			
			/* copy over ir with the signature */
			cpy_sz = msh_GetNzmax(dest)*sizeof(mwIndex);
			msh_CopyData((void*)msh_GetIr(dest), (void*)mxGetIr(in_var), cpy_sz);
			
			/* shift to end of ir */
			curr_off += cpy_sz;
			
			/** begin copy jc **/
			
			/* make room for the mxMalloc signature */
			curr_off = PadToAlign(curr_off + MXMALLOC_SIG_LEN);
			
			/* set the offset of jc */
			msh_SetJcOffset(dest, curr_off);
			
			/* copy over jc with the signature */
			cpy_sz = (msh_GetDimensions(dest)[1] + 1)*sizeof(mwIndex);
			msh_CopyData((void*)msh_GetJc(dest), (void*)mxGetJc(in_var), cpy_sz);
			
			/* shift to the end of jc */
			curr_off += cpy_sz;
			
		}
		else
		{
			/* copy data */
			if(!mxIsEmpty(in_var))
			{
				
				/** begin copy data **/
				
				/* make room for the mxMalloc signature */
				curr_off = PadToAlign(curr_off + MXMALLOC_SIG_LEN);
				
				/* set the offset of the data */
				msh_SetDataOffset(dest, curr_off);
				
				/* copy over the data with the signature */
				cpy_sz = msh_GetNumElems(dest)*msh_GetElemSize(dest);
				msh_CopyData(msh_GetData(dest), mxGetData(in_var), cpy_sz);
				
				/* shift to end of the data */
				curr_off += cpy_sz;
				
				/* copy imag_data */
				if(mxIsComplex(in_var))
				{
					/* make room for the mxMalloc signature */
					curr_off = PadToAlign(curr_off + MXMALLOC_SIG_LEN);
					
					/* set the offset of the imaginary data */
					msh_SetImagDataOffset(dest, curr_off);
					
					/* copy over the data with the signature */
					msh_CopyData(msh_GetImagData(dest), mxGetImagData(in_var), cpy_sz);
					
					/* shift to end of the imaginary data */
					curr_off += cpy_sz;
					
				}
			}
		}
		
	}
	
	/* return the total size of the object with padding to align */
	return curr_off;
}


mxArray* msh_FetchVariable(SharedVariableHeader_t* shm_hdr)
{
	mxArray* ret_var = NULL;
	
	/* for loops */
	size_t idx, count, num_elems;
	
	/* for structures */
	int field_num, num_fields;
	
	const char_t** field_names;
	const char_t* field_name;
	
	/* Structure case */
	if(msh_GetClassId(shm_hdr) == mxSTRUCT_CLASS)
	{
		num_elems = msh_GetNumElems(shm_hdr);
		num_fields = msh_GetNumFields(shm_hdr);
		
		field_names = mxMalloc(num_fields*sizeof(char_t*));
		field_name = msh_GetFieldNames(shm_hdr);
		for(field_num = 0; field_num < num_fields; field_num++, msh_GetNextFieldName(&field_name))
		{
			field_names[field_num] = field_name;
		}
		ret_var = mxCreateStructArray(msh_GetNumDims(shm_hdr), msh_GetDimensions(shm_hdr), num_fields, field_names);
		mxFree((char**)field_names);
		
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < num_elems; idx++, count++)
			{
				mxSetFieldByNumber(ret_var, idx, field_num, msh_FetchVariable(msh_GetChildHeader(shm_hdr, count)));
			}
		}
	}
	else if(msh_GetClassId(shm_hdr) == mxCELL_CLASS) /* Cell case */
	{
		num_elems = msh_GetNumElems(shm_hdr);
		
		ret_var = mxCreateCellArray(msh_GetNumDims(shm_hdr), msh_GetDimensions(shm_hdr));
		
		for(count = 0; count < num_elems; count++)
		{
			mxSetCell(ret_var, count, msh_FetchVariable(msh_GetChildHeader(shm_hdr, count)));
		}
	}
	else /*base case*/
	{
		
		if(msh_GetIsSparse(shm_hdr))
		{
			
			if(msh_GetClassId(shm_hdr) == mxDOUBLE_CLASS)
			{
				ret_var = mxCreateSparse(0, 0, 1, msh_GetComplexity(shm_hdr));
			}
			else if(msh_GetClassId(shm_hdr) == mxLOGICAL_CLASS)
			{
				ret_var = mxCreateSparseLogicalMatrix(0, 0, 1);
			}
			else
			{
				ReadMexError(__FILE__, __LINE__, "UnrecognizedTypeError", "The fetched array was of class 'sparse' but not of type 'double' or 'logical'.");
			}
			
			/* free the pointers relating to sparse */
			mxFree(mxGetIr(ret_var));
			mxSetIr(ret_var, msh_GetIr(shm_hdr));
			
			mxFree(mxGetJc(ret_var));
			mxSetJc(ret_var, msh_GetJc(shm_hdr));
			
			/* set the new data */
			mxFree(mxGetData(ret_var));
			mxSetData(ret_var, msh_GetData(shm_hdr));
			if(msh_GetIsComplex(shm_hdr))
			{
				mxFree(mxGetImagData(ret_var));
				mxSetImagData(ret_var, msh_GetImagData(shm_hdr));
			}
			
			mxSetNzmax(ret_var, msh_GetNzmax(shm_hdr));
			
		}
		else
		{
			
			if(msh_GetIsNumeric(shm_hdr))
			{
				ret_var = mxCreateNumericArray(0, NULL, msh_GetClassId(shm_hdr), msh_GetComplexity(shm_hdr));
			}
			else if(msh_GetClassId(shm_hdr) == mxLOGICAL_CLASS)
			{
				ret_var = mxCreateLogicalArray(0, NULL);
			}
			else if(msh_GetClassId(shm_hdr) == mxCHAR_CLASS)
			{
				ret_var = mxCreateCharArray(0, NULL);
			}
			else
			{
				ReadMexError(__FILE__, __LINE__, "UnrecognizedTypeError", "The fetched array was of class not of type 'numeric', 'logical', or 'char'.");
			}
			
			/* there is no data if it is empty */
			if(!msh_GetIsEmpty(shm_hdr))
			{
				mxFree(mxGetData(ret_var));
				mxSetData(ret_var, msh_GetData(shm_hdr));
				if(msh_GetIsComplex(shm_hdr))
				{
					mxFree(mxGetImagData(ret_var));
					mxSetImagData(ret_var, msh_GetImagData(shm_hdr));
				}
			}
			
		}
		
		
		mxSetDimensions(ret_var, msh_GetDimensions(shm_hdr), msh_GetNumDims(shm_hdr));
		
	}
	
	return ret_var;
	
}


void msh_OverwriteData(SharedVariableHeader_t* shm_hdr, const mxArray* in_var, mxArray* rewrite_var)
{
	size_t idx, count, num_elems;
	
	/* for structures */
	int field_num, num_fields;                /* current field */
	
	/* Structure case */
	if(msh_GetClassId(shm_hdr) == mxSTRUCT_CLASS)
	{
		num_elems = msh_GetNumElems(shm_hdr);
		num_fields = msh_GetNumFields(shm_hdr);
		
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < num_elems; idx++, count++)
			{
				/* And fill it */
				msh_OverwriteData(msh_GetChildHeader(shm_hdr, count), mxGetFieldByNumber(in_var, idx, field_num), mxGetFieldByNumber(rewrite_var, idx, field_num));
			}
		}
	}
	else if(msh_GetClassId(shm_hdr) == mxCELL_CLASS) /* Cell case */
	{
		num_elems = msh_GetNumElems(shm_hdr);
		
		for(count = 0; count < num_elems; count++)
		{
			/* And fill it */
			msh_OverwriteData(msh_GetChildHeader(shm_hdr, count), mxGetCell(in_var, count), mxGetCell(rewrite_var, count));
		}
	}
	else     /*base case*/
	{
		
		/* if sparse get a list of the elements */
		if(msh_GetIsSparse(shm_hdr))
		{
			memcpy(msh_GetIr(shm_hdr), mxGetIr(in_var), msh_GetNzmax(shm_hdr)*sizeof(mwIndex));
			memcpy(msh_GetJc(shm_hdr), mxGetJc(in_var), (msh_GetDimensions(shm_hdr)[1] + 1)*sizeof(mwIndex));
			
			/* rewrite real data */
			memcpy(msh_GetData(shm_hdr), mxGetData(in_var), msh_GetNzmax(shm_hdr)*msh_GetElemSize(shm_hdr));
			
			/* if complex get a pointer to the complex data */
			if(msh_GetIsComplex(shm_hdr))
			{
				memcpy(msh_GetImagData(shm_hdr), mxGetImagData(in_var), msh_GetNzmax(shm_hdr)*msh_GetElemSize(shm_hdr));
			}
		}
		else if(!msh_GetIsEmpty(shm_hdr))
		{
			
			num_elems = msh_GetNumElems(shm_hdr);
			
			/* rewrite real data */
			memcpy(msh_GetData(shm_hdr), mxGetData(in_var), num_elems*msh_GetElemSize(shm_hdr));
			
			/* if complex get a pointer to the complex data */
			if(msh_GetIsComplex(shm_hdr))
			{
				memcpy(msh_GetImagData(shm_hdr), mxGetImagData(in_var), num_elems*msh_GetElemSize(shm_hdr));
			}
		}
		
	}
	
}


bool_t msh_CompareVariableSize(SharedVariableHeader_t* shm_hdr, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count, num_elems;
	
	/* for structures */
	int field_num, num_fields;                /* current field */
	
	const char_t* field_name;
	
	/* can't allow differing dimensions because matlab doesn't use shared pointers to dimensions in mxArrays */
	if(mxGetClassID(comp_var) != msh_GetClassId(shm_hdr) ||
	   mxGetNumberOfDimensions(comp_var) != msh_GetNumDims(shm_hdr) ||
	   memcmp(mxGetDimensions(comp_var), msh_GetDimensions(shm_hdr), msh_GetNumDims(shm_hdr) * sizeof(mwSize)) != 0)
	{
		return FALSE;
	}
	
	/* Structure case */
	if(msh_GetClassId(shm_hdr) == mxSTRUCT_CLASS)
	{
		
		num_elems = msh_GetNumElems(shm_hdr);
		num_fields = msh_GetNumFields(shm_hdr);
		
		if(msh_GetNumFields(shm_hdr) != mxGetNumberOfFields(comp_var) || num_elems != mxGetNumberOfElements(comp_var))
		{
			return FALSE;
		}
		
		/* Go through each element */
		field_name = msh_GetFieldNames(shm_hdr);
		for(field_num = 0, count = 0; field_num < num_fields; field_num++)     /* each field */
		{
			
			if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_name) != 0)
			{
				return FALSE;
			}
			
			for(idx = 0; idx < num_elems; idx++, count++)
			{
				if(!msh_CompareVariableSize(msh_GetChildHeader(shm_hdr, count), mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
			
			msh_GetNextFieldName(&field_name);
			
		}
		
	}
	else if(msh_GetClassId(shm_hdr) == mxCELL_CLASS) /* Cell case */
	{
		num_elems = msh_GetNumElems(shm_hdr);
		
		if(num_elems != mxGetNumberOfElements(comp_var))
		{
			return FALSE;
		}
		
		for(count = 0; count < num_elems; count++)
		{
			if(!msh_CompareVariableSize(msh_GetChildHeader(shm_hdr, count), mxGetCell(comp_var, count)))
			{
				return FALSE;
			}
		}
	}
	else if(mxIsNumeric(comp_var) || mxGetClassID(comp_var) == mxLOGICAL_CLASS || mxGetClassID(comp_var) == mxCHAR_CLASS)      /*base case*/
	{
		
		if(msh_GetIsComplex(shm_hdr) != mxIsComplex(comp_var))
		{
			return FALSE;
		}
		
		if(msh_GetIsSparse(shm_hdr))
		{
			if(msh_GetNzmax(shm_hdr) != mxGetNzmax(comp_var) || msh_GetDimensions(shm_hdr)[1] != mxGetN(comp_var) || !mxIsSparse(comp_var))
			{
				return FALSE;
			}
		}
		else
		{
			if(msh_GetNumElems(shm_hdr) != mxGetNumberOfElements(comp_var) || mxIsSparse(comp_var))
			{
				return FALSE;
			}
		}
		
	}
	else
	{
		ReadMexError(__FILE__, __LINE__, "InvalidTypeError", "Unexpected input type. All elements of the shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
	}
	
	return TRUE;
	
}


void msh_DetachVariable(mxArray* ret_var)
{
	mxArray* link;
	size_t num_dims = 0, idx, num_elems;
	int field_num, num_fields;
	mwSize new_dims[] = {0, 0};
	mwSize new_nzmax = 0;
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
			num_dims = 2;
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
			
			link = msh_GetCrosslink(link);
		} while(link != NULL && link != ret_var);
		
	}
	else
	{
		ReadMexError(__FILE__, __LINE__, "InvalidTypeError", "Unsupported type. The segment may have been corrupted.");
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
		cml_sz += strlen(field_name) + 1; /* remember to add the null termination */
	}
	
	return cml_sz;
}


static void msh_GetNextFieldName(const char_t** field_str)
{
	*field_str = *field_str + strlen(*field_str) + 1;
}


static void* msh_CopyData(byte_t* dest, byte_t* orig, size_t cpy_sz)
{
	uint8_T mxmalloc_sig[MXMALLOC_SIG_LEN];
	msh_MakeMxMallocSignature(mxmalloc_sig, cpy_sz);
	
	memcpy(dest - MXMALLOC_SIG_LEN, mxmalloc_sig, MXMALLOC_SIG_LEN);
	if(orig != NULL)
	{
		memcpy(dest, orig, cpy_sz);
	}
	else
	{
		memset(dest, 0, cpy_sz);
	}
	return dest;
}


static void msh_MakeMxMallocSignature(uint8_T* sig, size_t seg_size)
{
	/*
	 * MXMALLOC SIGNATURE INFO:
	 * 	mxMalloc adjusts the size of memory for 32 byte alignment (shifts either 16 or 32 bytes)
	 * 	it appends the following 16 byte header immediately before data segment
	 *
	 * HEADER:
	 * 		bytes 0-7 - size iterator; rules for each byte are:
	 * 			byte 0 = (((size+0x1F)/2^4)%2^4)*2^4
	 * 			byte 1 = (((size+0x1F)/2^8)%2^8)
	 * 			byte 2 = (((size+0x1F)/2^16)%2^16)
	 * 			byte n = (((size+0x1F)/2^(2^(2+n)))%2^(2^(2+n)))
	 *
	 * 		bytes  8-11 - a signature for the vector check
	 * 		bytes 12-13 - the alignment (should be 32 bytes for new MATLAB)
	 * 		bytes 14-15 - the offset from the original pointer to the newly aligned pointer (should be 16 or 32)
	 */
	size_t multiplier;
	uint8_T i;
	/* const uint8_T mxmalloc_sig_template[MXMALLOC_SIG_LEN] = {16, 0, 0, 0, 0, 0, 0, 0, 206, 250, 237, 254, 32, 0, 32, 0}; */
#define MXMALLOC_SIG_TEMPLATE "\x10\x00\x00\x00\x00\x00\x00\x00\xCE\xFA\xED\xFE\x20\x00\x20\x00"
	
	memcpy(sig, MXMALLOC_SIG_TEMPLATE, MXMALLOC_SIG_LEN * sizeof(char_t));
	multiplier = 16;
	
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	if(seg_size > 0)
	{
		sig[0] = (uint8_T)((((seg_size + 0x0F)/multiplier) & (multiplier - 1))*multiplier);
		
		/* note: this only does bits 1 to 3 because of 64 bit precision limit (maybe implement bit 4 in the future?)*/
		for(i = 1; i < 4; i++)
		{
			multiplier ^= 2;
			sig[i] = (uint8_T)(((seg_size + 0x0F)/multiplier) & (multiplier - 1));
		}
	}
}
