#include "headers/matshare_.h"

GlobalInfo_t* g_info = NULL;

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	/* number of inputs other than the msh_directive */
	int num_in_vars = nrhs - 1;
	
	/* inputs */
	const mxArray* in_directive = prhs[0];
	const mxArray** in_vars = prhs + 1;
	
	/* resultant matshare directive */
	msh_directive_t msh_directive;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		ReadMexError("NotEnoughInputsError", "Minimum input arguments missing; must supply a msh_directive.");
	}
	
	/* get the msh_directive */
	msh_directive = ParseDirective(in_directive);
	
	InitializeMatshare();
	
	if(msh_directive != msh_DETACH && Precheck() != TRUE)
	{
		ReadMexError("NotInitializedError", "At least one of the needed shared memory segments has not been initialized. Cannot continue.");
	}
	
	switch(msh_directive)
	{
		case msh_SHARE:
			
			AcquireProcessLock();
			{
				MshShare(nlhs, plhs, num_in_vars, in_vars);
			}
			ReleaseProcessLock();
			
			break;
		
		case msh_FETCH:
			
			AcquireProcessLock();
			{
				MshFetch(nlhs, plhs);
			}
			ReleaseProcessLock();
			
			break;
		
		case msh_OBJ_DEREGISTER:
			/* deregister an object tracking this function */
			g_info->num_registered_objs -= 1;
			if(g_info->num_registered_objs == 0)
			{
				/* if we deregistered the last object then remove variables that aren't being used elsewhere in this process */
				RemoveUnusedVariables();
			}
			
			break;
		case msh_DETACH:
			
			MshOnExit();
			
			break;
		
		case msh_PARAM:
			
			ParseParams(num_in_vars, in_vars);
			
			break;
		
		case msh_DEEPCOPY:
			
			AcquireProcessLock();
			{
				MshDeepcopy(nlhs, plhs);
			}
			ReleaseProcessLock();
			
			break;
		case msh_DEBUG:
			/* STUB: maybe print debug information at some point */
			break;
		case msh_OBJ_REGISTER:
			
			/* tell matshare to register a new object */
			g_info->num_registered_objs += 1;
			
			break;
		case msh_INIT:
			/* deprecated; does nothing */
			break;
		case msh_CLEAR:
			
			AcquireProcessLock();
			{
				MshClear(num_in_vars, in_vars);
			}
			ReleaseProcessLock();
			
			break;
		default:
			ReadMexError("UnknownDirectiveError", "Unrecognized msh_directive.");
			break;
	}
	
}


void MshShare(int nlhs, mxArray** plhs, int num_vars, const mxArray* in_vars[])
{
	
	SegmentNode_t* new_seg_node;
	const mxArray* in_var;
	int i;
	
	/* check the inputs */
	if(num_vars < 1)
	{
		ReadMexError("NoVariableError", "No variable supplied to clone.");
	}
	
	UpdateSharedSegments();
	
	for(i = 0, in_var = in_vars[i]; i < num_vars; i++, in_var = in_vars[i])
	{
		
		if(!(mxIsNumeric(in_var) || mxIsLogical(in_var) || mxIsChar(in_var) || mxIsStruct(in_var) || mxIsCell(in_var)))
		{
			ReadMexError("InvalidTypeError", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
		}
		
		switch(s_info->user_def.sharetype)
		{
			
			case msh_SHARETYPE_COPY:
				
				RemoveUnusedVariables();
				
				break;
			
			case msh_SHARETYPE_OVERWRITE:
				
				if(g_var_list.num_vars != 0 && (ShmCompareSize_(SegmentData(g_seg_list.last), in_var) == TRUE))
				{
					/* DON'T INCREMENT THE REVISION NUMBER */
					/* this is an in-place change, so everyone is still fine */
					
					/* do the rewrite after checking because the comparison is cheap */
					ShmRewrite_(SegmentData(g_seg_list.last), in_var, g_var_list.last->var);
					
					/* DON'T DO ANYTHING ELSE */
					continue;
				}
				else
				{
					DestroySegmentList(&g_seg_list);
				}
				
				break;
			
			default:
				ReadMexError("UnexpectedError", "Invalid sharetype. The shared memory has been corrupted.");
			
		}
		
		/* scan input data */
		new_seg_node = CreateSegmentNode(CreateSegment(ShmScan_(in_var)));
		
		/* copy data to the shared memory */
		ShmCopy_(SegmentData(new_seg_node), in_var);
		
		/* track the new segment */
		AddSegmentNode(&g_seg_list, new_seg_node);
		
	}
	
	if(nlhs > 0)
	{
		MshFetch(nlhs, plhs);
	}
	
	UpdateAll();
	
}


void MshFetch(int nlhs, mxArray** plhs)
{
	
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;
	size_t i, num_new_vars = 0;
	size_t ret_dims[2];
	
	UpdateSharedSegments();
	
	if(s_info->user_def.sharetype == msh_SHARETYPE_COPY)
	{
		RemoveUnusedVariables();
	}
	
	if(nlhs >= 1)
	{
		if(nlhs >= 2)
		{
			
			for(i = 0, curr_seg_node = g_seg_list.first;
					i < g_seg_list.num_segs;
					i++, curr_seg_node = curr_seg_node->next)
			{
				if(curr_seg_node->var_node == NULL)
				{
					/* create the variable node if it hasnt been created yet */
					AddVariableNode(&g_var_list, CreateVariableNode(curr_seg_node));
					num_new_vars += 1;
				}
			}
			
			/* return the new variables detected */
			ret_dims[0] = (size_t)(num_new_vars != 0);
			ret_dims[1] = num_new_vars;
			plhs[1] = mxCreateCellArray(2, ret_dims);
			
			for(i = 0, curr_var_node = g_var_list.last;
					i < num_new_vars;
					i++, curr_var_node = curr_var_node->prev)
			{
				mxSetCell(plhs[1], num_new_vars - 1 - i, mxCreateSharedDataCopy(curr_var_node->var));
			}
			
			if(nlhs == 3)
			{
				/* retrieve all segment variables */
				ret_dims[0] = (size_t)(g_seg_list.num_segs != 0);
				ret_dims[1] = g_seg_list.num_segs;
				plhs[2] = mxCreateCellArray(2, ret_dims);
				
				for(i = 0, curr_seg_node = g_seg_list.first;
						i < g_seg_list.num_segs;
						i++, curr_seg_node = curr_seg_node->next)
				{
					mxSetCell(plhs[2], i, mxCreateSharedDataCopy(curr_seg_node->var_node->var));
				}
			}
			else
			{
				ReadMexError("TooManyOutputsError", "Too many outputs.");
			}
		}
		else
		{
			if(g_seg_list.last != NULL && g_seg_list.last->var_node == NULL)
			{
				AddVariableNode(&g_var_list, CreateVariableNode(g_seg_list.last));
			}
		}
		
		if(g_seg_list.last != NULL)
		{
			plhs[0] = mxCreateSharedDataCopy(g_seg_list.last->var_node->var);
		}
		else
		{
			plhs[0] = mxCreateDoubleMatrix(0, 0, mxREAL);
		}
		
	}
	
}


void MshDeepcopy(int nlhs, mxArray** plhs)
{
	
	VariableNode_t* curr_var_node;
	SegmentNode_t* curr_seg_node;
	size_t i, num_new_vars = 0;
	size_t ret_dims[2];
	
	UpdateSharedSegments();
	
	if(s_info->user_def.sharetype == msh_SHARETYPE_COPY)
	{
		RemoveUnusedVariables();
	}
	
	if(nlhs >= 1)
	{
		if(nlhs >= 2)
		{
			
			for(i = 0, curr_seg_node = g_seg_list.first;
			    i < g_seg_list.num_segs;
			    i++, curr_seg_node = curr_seg_node->next)
			{
				if(curr_seg_node->var_node == NULL)
				{
					/* create the variable node if it hasnt been created yet */
					AddVariableNode(&g_var_list, CreateVariableNode(curr_seg_node));
					num_new_vars += 1;
				}
			}
			
			/* return the new variables detected */
			ret_dims[0] = (size_t)(num_new_vars != 0);
			ret_dims[1] = num_new_vars;
			plhs[1] = mxCreateCellArray(2, ret_dims);
			
			for(i = 0, curr_var_node = g_var_list.last;
			    i < num_new_vars;
			    i++, curr_var_node = curr_var_node->prev)
			{
				mxSetCell(plhs[1], num_new_vars - 1 - i, mxDuplicateArray(curr_var_node->var));
			}
			
			if(nlhs == 3)
			{
				/* retrieve all segment variables */
				ret_dims[0] = (size_t)(g_seg_list.num_segs != 0);
				ret_dims[1] = g_seg_list.num_segs;
				plhs[2] = mxCreateCellArray(2, ret_dims);
				
				for(i = 0, curr_seg_node = g_seg_list.first;
				    i < g_seg_list.num_segs;
				    i++, curr_seg_node = curr_seg_node->next)
				{
					mxSetCell(plhs[2], i, mxDuplicateArray(curr_seg_node->var_node->var));
				}
			}
			else
			{
				ReadMexError("TooManyOutputsError", "Too many outputs.");
			}
		}
		else
		{
			if(g_seg_list.last != NULL && g_seg_list.last->var_node == NULL)
			{
				AddVariableNode(&g_var_list, CreateVariableNode(g_seg_list.last));
			}
		}
		
		if(g_seg_list.last != NULL)
		{
			plhs[0] = mxDuplicateArray(g_seg_list.last->var_node->var);
		}
		else
		{
			plhs[0] = mxCreateDoubleMatrix(0, 0, mxREAL);
		}
		
	}
	
}


void MshClear(int num_inputs, const mxArray* in_vars[])
{
	
	VariableNode_t* curr_var_node, * next_var_node;
	mxArray* link;
	int i;
	
	UpdateSharedSegments();
	
	if(num_inputs > 0)
	{
		for(i = 0; i < num_inputs; i++)
		{
			curr_var_node = g_var_list.first;
			while(curr_var_node != NULL)
			{
				next_var_node = curr_var_node->next;
				
				link = curr_var_node->var;
				do
				{
					if(link == in_vars[i])
					{
						DestroySegmentNode(&g_seg_list, curr_var_node->seg_node);
						break;
					}
					link = ((mxArrayStruct*)link)->CrossLink;
				} while(link != NULL && link != curr_var_node->var);
				
				curr_var_node = next_var_node;
			}
		}
		
	}
	else
	{
		DestroySegmentList(&g_seg_list);
	}
}


size_t ShmScan_(const mxArray* in_var)
{
	
	/* counters */
	s_Header_t hdr;
	size_t idx, count, this_obj_sz = 0, sub_obj_sz = 0;
	int field_num;
	
	/* don't use the offset struct members because we are not attached to shm yet */
	
	hdr.elem_size = mxGetElementSize(in_var);
	hdr.num_elems = mxGetNumberOfElements(in_var);
	hdr.nzmax = mxGetNzmax(in_var);
	hdr.num_fields = mxGetNumberOfFields(in_var);
	hdr.classid = mxGetClassID(in_var);
	hdr.is_sparse = mxIsSparse(in_var);
	hdr.is_numeric = mxIsNumeric(in_var);
	
	/* Add space for the header */
	this_obj_sz += sizeof(s_Header_t);
	
	/* Add space for the dimensions */
	this_obj_sz += mxGetNumberOfDimensions(in_var)*sizeof(mwSize);
	
	/* Structure case */
	if(hdr.classid == mxSTRUCT_CLASS)
	{
		
		/* add size of storing the offsets */
		this_obj_sz += (hdr.num_fields*hdr.num_elems)*sizeof(size_t);
		
		/* Add space for the field string */
		this_obj_sz += GetFieldNamesSize(in_var);
		
		/* go through each recursively */
		for(field_num = 0, count = 0; field_num < hdr.num_fields; field_num++)          /* each field */
		{
			for(idx = 0; idx < hdr.num_elems; idx++, count++)                         /* each element */
			{
				/* call recursivley */
				sub_obj_sz += ShmScan_(mxGetFieldByNumber(in_var, idx, field_num));
			}
		}
	}
	else if(hdr.classid == mxCELL_CLASS) /* Cell case */
	{
		
		/* add size of storing the offsets */
		this_obj_sz += hdr.num_elems*sizeof(size_t);
		
		/* go through each recursively */
		for(count = 0; count < hdr.num_elems; count++)
		{
			sub_obj_sz += ShmScan_(mxGetCell(in_var, count));
		}
	}
	else if(hdr.is_numeric || hdr.classid == mxLOGICAL_CLASS || hdr.classid == mxCHAR_CLASS)  /* a matrix containing data *//* base case */
	{
		
		if(hdr.is_sparse)
		{
			/* len(data)==nzmax, len(imag_data)==nzmax, len(ir)=nzmax, len(jc)==N+1 */
			
			/* add the size of the real data */
			AddDataSize(this_obj_sz, hdr.elem_size*hdr.nzmax);
			if(mxIsComplex(in_var))
			{
				/* and the imaginary data */
				AddDataSize(this_obj_sz, hdr.elem_size*hdr.nzmax);
			}
			AddDataSize(this_obj_sz, sizeof(mwIndex)*(hdr.nzmax));          /* ir */
			AddDataSize(this_obj_sz, sizeof(mwIndex)*(mxGetN(in_var) + 1)); /* jc */
			
		}
		else
		{
			/* ensure both pointers are aligned individually */
			if(!mxIsEmpty(in_var))
			{
				/* add the size of the real data */
				AddDataSize(this_obj_sz, hdr.elem_size*hdr.num_elems);
				if(mxIsComplex(in_var))
				{
					/* and the imaginary data */
					AddDataSize(this_obj_sz, hdr.elem_size*hdr.num_elems);
				}
			}
		}
		
	}
	else
	{
		ReadMexError("Internal:UnexpectedError", "Tried to clone an unsupported type.");
	}
	
	/* sub-objects will already be aligned */
	return PadToAlign(this_obj_sz) + sub_obj_sz;
}



size_t ShmCopy_(byte_t* shm_anchor, const mxArray* in_var)
{
	
	s_Header_t hdr;
	SharedMemoryTracker_t shm_tracker = {0, shm_anchor};
	size_t shift, idx, cpy_sz, count;
	
	int field_num;
	const char_t* field_name;
	mwSize* dims;
	size_t* child_hdrs;
	char_t* field_str;
	void* data,* imag_data;
	mwIndex* ir, * jc;
	
	/* convenience macro */
#define DataShift(shm_tracker) shift = GetDataShift((shm_tracker).curr_off);\
                                  MemoryShift((shm_tracker), shift)
	
	/* initialize header info */
	
	/* these are updated when the data is copied over */
	hdr.data_offsets.data = SIZE_MAX;
	hdr.data_offsets.imag_data = SIZE_MAX;
	hdr.data_offsets.ir = SIZE_MAX;
	hdr.data_offsets.jc = SIZE_MAX;
	
	hdr.num_dims = mxGetNumberOfDimensions(in_var);
	hdr.elem_size = mxGetElementSize(in_var);
	hdr.num_elems = mxGetNumberOfElements(in_var);
	hdr.nzmax = mxGetNzmax(in_var);
	hdr.num_fields = mxGetNumberOfFields(in_var);
	hdr.classid = mxGetClassID(in_var);
	hdr.is_sparse = mxIsSparse(in_var);
	hdr.is_numeric = mxIsNumeric(in_var);
	
	MemoryShift(shm_tracker, sizeof(s_Header_t));
	
	/* copy the dimensions */
	dims = (mwSize*)shm_tracker.ptr;
	cpy_sz = hdr.num_dims*sizeof(mwSize);
	memcpy(dims, mxGetDimensions(in_var), cpy_sz);
	MemoryShift(shm_tracker, cpy_sz);
	
	/* Structure case */
	if(hdr.classid == mxSTRUCT_CLASS)
	{
		
		/* store the child header offsets */
		child_hdrs = (size_t*)shm_tracker.ptr, hdr.data_offsets.child_offs = shm_tracker.curr_off;
		MemoryShift(shm_tracker, (hdr.num_fields*hdr.num_elems)*sizeof(size_t));
		
		/* store the field string */
		field_str = (char_t*)shm_tracker.ptr, hdr.data_offsets.field_str = shm_tracker.curr_off;
		MemoryShift(shm_tracker, GetFieldNamesSize(in_var));
		
		/* align this object */
		shm_tracker.curr_off = PadToAlign(shm_tracker.curr_off);
		
		/* copy the children recursively */
		for(field_num = 0, count = 0; field_num < hdr.num_fields; field_num++)                /* the fields */
		{
			
			field_name = mxGetFieldNameByNumber(in_var, field_num);
			cpy_sz = strlen(field_name) + 1;
			memcpy(field_str, field_name, cpy_sz);
			field_str += cpy_sz;
			
			for(idx = 0; idx < hdr.num_elems; idx++, count++)                                   /* the struct array indices */
			{
				/* place relative offset into shared memory */
				child_hdrs[count] = shm_tracker.curr_off;
				
				/* And fill it */
				shm_tracker.curr_off += ShmCopy_(GetChildHeader(shm_anchor, child_hdrs, count), mxGetFieldByNumber(in_var, idx, field_num));
				
			}
			
		}
		
	}
	else if(hdr.classid == mxCELL_CLASS) /* Cell case */
	{
		/* store the child headers here */
		child_hdrs = (size_t*)shm_tracker.ptr, hdr.data_offsets.child_offs = shm_tracker.curr_off;
		shm_tracker.curr_off += hdr.num_elems*sizeof(size_t);
		
		shm_tracker.curr_off = PadToAlign(shm_tracker.curr_off);
		
		/* recurse for each cell element */
		for(count = 0; count < hdr.num_elems; count++)
		{
			/* place relative offset into shared memory */
			child_hdrs[count] = shm_tracker.curr_off;
			
			/* And fill it */
			shm_tracker.curr_off += ShmCopy_(GetChildHeader(shm_anchor, child_hdrs, count), mxGetCell(in_var, count));
		}
		
	}
	else /* base case */
	{
		
		/* and the indices of the sparse data as well */
		if(hdr.is_sparse)
		{
			
			/* note: in this case hdr->is_empty means the sparse is 0x0
			 *  also note that nzmax is still 1 */
			
			/* make room for the mxMalloc signature */
			DataShift(shm_tracker);
			hdr.data_offsets.data = shm_tracker.curr_off, data = shm_tracker.ptr;
			
			/* copy over the data with the signature */
			cpy_sz = (hdr.nzmax)*(hdr.elem_size);
			MemCpyMex(data, mxGetData(in_var), cpy_sz);
			MemoryShift(shm_tracker, cpy_sz);
			
			
			/* copy imag_data */
			if(mxIsComplex(in_var))
			{
				/* make room for the mxMalloc signature */
				DataShift(shm_tracker);
				hdr.data_offsets.imag_data = shm_tracker.curr_off, imag_data = shm_tracker.ptr;
				
				/* copy over the data with the signature */
				MemCpyMex(imag_data, mxGetImagData(in_var), cpy_sz);
				MemoryShift(shm_tracker, cpy_sz);
				
			}
			
			/* copy ir */
			
			/* make room for the mxMalloc signature */
			DataShift(shm_tracker);
			hdr.data_offsets.ir = shm_tracker.curr_off, ir = (mwIndex*)shm_tracker.ptr;
			
			/* copy over the data with the signature */
			cpy_sz = hdr.nzmax*sizeof(mwIndex);
			MemCpyMex((void*)ir, (void*)mxGetIr(in_var), cpy_sz);
			MemoryShift(shm_tracker, cpy_sz);
			
			/* copy jc */
			DataShift(shm_tracker);
			hdr.data_offsets.jc = shm_tracker.curr_off, jc = (mwIndex*)shm_tracker.ptr;
			
			cpy_sz = (dims[1] + 1)*sizeof(mwIndex);
			MemCpyMex((void*)jc, (void*)mxGetJc(in_var), cpy_sz);
			MemoryShift(shm_tracker, cpy_sz);
			
		}
		else
		{
			/* copy data */
			if(!mxIsEmpty(in_var))
			{
				
				/* make room for the mxMalloc signature */
				DataShift(shm_tracker);
				hdr.data_offsets.data = shm_tracker.curr_off, data = shm_tracker.ptr;
				
				/* copy over the data with the signature */
				cpy_sz = hdr.num_elems*hdr.elem_size;
				MemCpyMex(data, mxGetData(in_var), cpy_sz);
				MemoryShift(shm_tracker, cpy_sz);
				
				
				/* copy imag_data */
				if(mxIsComplex(in_var))
				{
					/* make room for the mxMalloc signature */
					DataShift(shm_tracker);
					hdr.data_offsets.imag_data = shm_tracker.curr_off, imag_data = shm_tracker.ptr;
					
					/* copy over the data with the signature */
					MemCpyMex(imag_data, mxGetImagData(in_var), cpy_sz);
					MemoryShift(shm_tracker, cpy_sz);
					
				}
			}
		}
		
	}
	
	memcpy(shm_anchor, &hdr, sizeof(s_Header_t));
	
	return PadToAlign(shm_tracker.curr_off);
	
}


void ShmFetch_(byte_t* shm_anchor, mxArray** ret_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	mxArray* ret_child;
	
	/* for working with payload ... */
	s_Header_t* hdr;
	
	/* for structures */
	int field_num;                /* current field */
	
	const char_t** field_names;
	const char_t* field_name;
	
	/* retrieve the data */
	hdr = (s_Header_t*)shm_anchor;
	SharedDataPointers_t data_ptrs = LocateDataPointers(hdr, shm_anchor);
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		field_names = mxMalloc(hdr->num_fields*sizeof(char_t*));
		field_name = data_ptrs.field_str;
		for(field_num = 0; field_num < hdr->num_fields; field_num++, GetNextFieldName(&field_name))
		{
			field_names[field_num] = field_name;
		}
		*ret_var = mxCreateStructArray(hdr->num_dims, data_ptrs.dims, hdr->num_fields, field_names);
		mxFree((char**)field_names);
		
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < hdr->num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < hdr->num_elems; idx++, count++)
			{
				ShmFetch_(GetChildHeader(shm_anchor, data_ptrs.child_hdrs, count), &ret_child);
				mxSetFieldByNumber(*ret_var, idx, field_num, ret_child);
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		*ret_var = mxCreateCellArray(hdr->num_dims, data_ptrs.dims);
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			ShmFetch_(GetChildHeader(shm_anchor, data_ptrs.child_hdrs, count), &ret_child);
			mxSetCell(*ret_var, count, ret_child);
		}
		
	}
	else     /*base case*/
	{
		
		if(hdr->is_sparse)
		{
			
			if(hdr->classid == mxDOUBLE_CLASS)
			{
				*ret_var = mxCreateSparse(0, 0, 1, MshGetComplexity(hdr));
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateSparseLogicalMatrix(0, 0, 1);
			}
			else
			{
				ReadMexError("UnrecognizedTypeError", "The fetched array was of class 'sparse' but not of type 'double' or 'logical'");
			}
			
			/* free the pointers relating to sparse */
			mxFree(mxGetIr(*ret_var));
			mxFree(mxGetJc(*ret_var));
			
			mxSetNzmax(*ret_var, hdr->nzmax);
			
		}
		else
		{
			
			if(hdr->is_numeric)
			{
				*ret_var = mxCreateNumericArray(0, NULL, hdr->classid, MshGetComplexity(hdr));
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateLogicalArray(0, NULL);
			}
			else
			{
				*ret_var = mxCreateCharArray(0, NULL);
			}
			
			if(MshIsEmpty(hdr))
			{
				data_ptrs.data = NULL;
				data_ptrs.imag_data = NULL;
			}
			
		}
		
		/* free temporary data pointers (may be NULL) */
		mxFree(mxGetData(*ret_var));
		if(MshIsComplex(hdr))
		{
			mxFree(mxGetImagData(*ret_var));
		}
		else
		{
			data_ptrs.imag_data = NULL;
		}
		
		/* set all data */
		SetDataPointers(*ret_var, &data_ptrs);
		mxSetDimensions(*ret_var, data_ptrs.dims, hdr->num_dims);
		
	}
}


void ShmDetach(mxArray* ret_var)
{
	mxArray* link;
	size_t num_dims = 0, idx, num_elems;
	int field_num, num_fields;
	mwSize new_dims[] = {0, 0};
	SharedDataPointers_t new_data_ptrs = {NULL};
	mwSize nzmax = 0;
	
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
				ShmDetach(mxGetFieldByNumber(ret_var, idx, field_num));
			}
		}
	}
	else if(mxIsCell(ret_var))
	{
		/* detach each cell */
		num_elems = mxGetNumberOfElements(ret_var);
		for(idx = 0; idx < num_elems; idx++)
		{
			ShmDetach(mxGetCell(ret_var, idx));
		}
		
	}
	else if(mxIsNumeric(ret_var) || mxIsLogical(ret_var) || mxIsChar(ret_var))  /* a matrix containing data */
	{
		
		/* handle sparse objects */
		if(mxIsSparse(ret_var))
		{
			num_dims = 2;
			nzmax = 1;
			
			/* allocate 1 element */
			new_data_ptrs.data = mxCalloc(nzmax, mxGetElementSize(ret_var));
			if(mxIsComplex(ret_var))
			{
				new_data_ptrs.imag_data = mxCalloc(nzmax, mxGetElementSize(ret_var));
			}
			
			new_data_ptrs.ir = mxCalloc(nzmax, sizeof(mwIndex));
			new_data_ptrs.jc = mxCalloc(new_dims[1] + 1, sizeof(mwIndex));
			
			
		}
		
		
		/** HACK **/
		/* reset all the crosslinks so nothing in MATLAB is pointing to shared data (which will be gone soon) */
		link = ret_var;
		do
		{
			if(mxIsSparse(link))
			{
				mxSetNzmax(ret_var, nzmax);
			}
			
			SetDataPointers(link, &new_data_ptrs);
			mxSetDimensions(link, new_dims, num_dims);
			
			link = ((mxArrayStruct*)link)->CrossLink;
		} while(link != NULL && link != ret_var);
		
	}
	else
	{
		ReadMexError("InvalidTypeError", "Unsupported type. The segment may have been corrupted.");
	}
}



void ShmRewrite_(byte_t* shm_anchor, const mxArray* in_var, mxArray* rewrite_var)
{
	size_t idx, count, cpy_sz;
	
	/* for working with payload */
	s_Header_t* hdr;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (s_Header_t*)shm_anchor;
	SharedDataPointers_t data_ptrs = LocateDataPointers(hdr, shm_anchor);
	
	const mwSize* new_dims = mxGetDimensions(in_var);
	mwSize new_num_dims = mxGetNumberOfDimensions(in_var);
	
	cpy_sz = new_num_dims * sizeof(mwSize);
	
	if(memcmp(data_ptrs.dims, new_dims, cpy_sz) != 0)
	{
		
		memcpy(data_ptrs.dims, new_dims, cpy_sz);
		hdr->num_dims = new_num_dims;
		
		/* begin hack (set the dimensions in all crosslinks) */
		mxArray* curr_var = rewrite_var;
		do
		{
			mxSetDimensions(curr_var, new_dims, new_num_dims);
			curr_var = ((mxArrayStruct*)curr_var)->CrossLink;
		} while(curr_var != NULL && curr_var != rewrite_var);
		/* end hack */
	}
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < hdr->num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < hdr->num_elems; idx++, count++)
			{
				/* And fill it */
				ShmRewrite_(GetChildHeader(shm_anchor, data_ptrs.child_hdrs, count), mxGetFieldByNumber(in_var, idx, field_num), mxGetFieldByNumber(rewrite_var, idx, field_num));
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		for(count = 0; count < hdr->num_elems; count++)
		{
			/* And fill it */
			ShmRewrite_(GetChildHeader(shm_anchor, data_ptrs.child_hdrs, count), mxGetCell(in_var, count), mxGetCell(rewrite_var, count));
		}
	}
	else     /*base case*/
	{
		
		/* if sparse get a list of the elements */
		if(hdr->is_sparse)
		{
			memcpy(data_ptrs.ir, mxGetIr(in_var), (hdr->nzmax)*sizeof(mwIndex));
			memcpy(data_ptrs.jc, mxGetJc(in_var), ((data_ptrs.dims)[1] + 1)*sizeof(mwIndex));
		}

		/* rewrite real data */
		memcpy(data_ptrs.data, mxGetData(in_var), (hdr->num_elems)*(hdr->elem_size));
		
		/* if complex get a pointer to the complex data */
		if(MshIsComplex(hdr))
		{
			memcpy(data_ptrs.imag_data, mxGetImagData(in_var), (hdr->num_elems)*(hdr->elem_size));
		}
	
	}
	
}


bool_t ShmCompareSize_(byte_t* shm_anchor, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	size_t* child_hdrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	s_Header_t* hdr = (s_Header_t*)shm_anchor;
	mwSize* curr_dims;
	
	const char_t* field_name;
	
	/* note: we would have increase shm size if num_dims needs to increase */
	if(mxGetClassID(comp_var) != hdr->classid || mxGetNumberOfDimensions(comp_var) > hdr->num_dims)
	{
		return FALSE;
	}
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		if(hdr->num_fields != mxGetNumberOfFields(comp_var) || hdr->num_elems != mxGetNumberOfElements(comp_var))
		{
			return FALSE;
		}
		
		child_hdrs = (size_t*)(shm_anchor + hdr->data_offsets.child_offs);
		
		/* Go through each element */
		field_name = (char_t*)(shm_anchor + hdr->data_offsets.field_str);
		for(field_num = 0, count = 0; field_num < hdr->num_fields; field_num++)     /* each field */
		{
			
			if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_name) != 0)
			{
				return FALSE;
			}
			
			for(idx = 0; idx < hdr->num_elems; idx++, count++)
			{
				if(!ShmCompareSize_(GetChildHeader(shm_anchor, child_hdrs, count), mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
			
			GetNextFieldName(&field_name);
			
		}
		
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		if(hdr->num_elems != mxGetNumberOfElements(comp_var))
		{
			return FALSE;
		}
		
		child_hdrs = (size_t*)(shm_anchor + hdr->data_offsets.child_offs);
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			if(!ShmCompareSize_(GetChildHeader(shm_anchor, child_hdrs, count), mxGetCell(comp_var, count)))
			{
				return FALSE;
			}
		}
	}
	else     /*base case*/
	{
		
		if(MshIsComplex(hdr) != mxIsComplex(comp_var))
		{
			return FALSE;
		}
		
		if(hdr->is_sparse)
		{
			curr_dims = MshGetDimensions(shm_anchor);
			if(hdr->nzmax != mxGetNzmax(comp_var) || curr_dims[1] != mxGetN(comp_var) || !mxIsSparse(comp_var))
			{
				return FALSE;
			}
		}
		else
		{
			if(hdr->num_elems != mxGetNumberOfElements(comp_var) || mxIsSparse(comp_var))
			{
				return FALSE;
			}
		}
		
	}
	
	return TRUE;
	
}





