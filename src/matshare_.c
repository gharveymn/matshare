#include "headers/matshare_.h"


GlobalInfo_t* g_info = NULL;

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{

//	mexPrintf("%s\n", mexIsLocked()? "MEX is locked": "MEX is unlocked" );
	
	int num_params;
	
	/* For inputs */
	const mxArray* in_directive;               /* Directive {clone, attach, detach, free} */
	const mxArray* in_var;                    /* Input array (for clone) */
	
	/* For storing inputs */
	mshdirective_t directive;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		ReadErrorMex("NotEnoughInputsError", "Minimum input arguments missing; must supply a directive.");
	}
	
	/* assign inputs */
	in_directive = prhs[0];
	
	/* get the directive */
	directive = ParseDirective(in_directive);
	
	InitializeMatshare();
	
	if(directive != msh_DETACH && directive != msh_INIT && Precheck() != TRUE)
	{
		ReadErrorMex("NotInitializedError", "At least one of the needed shared memory segments has not been initialized. Cannot continue.");
	}
	
	
	switch(directive)
	{
		case msh_SHARE:
			/********************/
			/*	Share case	*/
			/********************/
			
			/* check the inputs */
			if(nrhs < 2)
			{
				ReadErrorMex("NoVariableError", "No variable supplied to clone.");
			}
			
			/* Assign */
			in_var = prhs[1];
			
			if(!(mxIsNumeric(in_var) || mxIsLogical(in_var) || mxIsChar(in_var) || mxIsStruct(in_var) || mxIsCell(in_var)))
			{
				ReadErrorMex("InvalidTypeError", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
			}
			
			MshShare(nlhs, plhs, in_var);
			
			break;
		
		case msh_FETCH:
			
			MshFetch(nlhs, plhs);
			
			break;
		
		case msh_OBJ_DEREGISTER:
			/* deregister an object tracking this function */
			g_info->num_registered_objs -= 1;
			/* fall through to check if we should clear everything */
		case msh_DETACH:
			/********************/
			/*	Dettach case	*/
			/********************/
			
			/* this only actually frees if it is the last object using the function in the current process */
			if(g_info->num_registered_objs == 0)
			{
				OnExit();
			}
			
			break;
		
		case msh_PARAM:
			/* set parameters for matshare to use */
			
			num_params = nrhs - 1;
			if(num_params%2 != 0)
			{
				ReadErrorMex("InvalNumArgsError", "The number of parameters input must be a multiple of two.");
			}
			
			ParseParams(num_params, prhs + 1);
			
			break;
		
		case msh_DEEPCOPY:
			//plhs[0] = mxDuplicateArray(g_var_list_front->var);
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
			UpdateSharedSegments();
			DestroySegmentList(&g_seg_list);
			ReleaseProcessLock();
			break;
		default:
			ReadErrorMex("UnknownDirectiveError", "Unrecognized directive.");
			break;
	}
	
}


void MshShare(int nlhs, mxArray** plhs, const mxArray* in_var)
{
	
	SegmentNode_t* new_seg_node;
	VariableNode_t* new_var_node;
	
	AcquireProcessLock();
	
	UpdateSharedSegments();
	
	if(s_info->sharetype == msh_SHARETYPE_COPY)
	{
		RemoveUnusedVariables(&g_var_list);
	}
	else if(s_info->sharetype == msh_SHARETYPE_OVERWRITE)
	{
		if(g_var_list.num_vars != 0 && (ShmCompareSize((byte_t*)g_seg_list.last->seg_info.s_ptr, in_var) == TRUE))
		{
			/* DON'T INCREMENT THE REVISION NUMBER */
			/* this is an in-place change, so everyone is still fine */
			
			/* do the rewrite after checking because the comparison is cheap */
			ShmRewrite((byte_t*)g_seg_list.last->seg_info.s_ptr, in_var, g_var_list.last->var);
			
			UpdateAll();
			ReleaseProcessLock();
			
			if(nlhs == 1)
			{
				plhs[0] = mxCreateSharedDataCopy(g_var_list.last->var);
			}
			
			/* DON'T DO ANYTHING ELSE */
			return;
		}
	}
	else
	{
		ReleaseProcessLock();
		ReadErrorMex("UnexpectedError", "Invalid sharetype. The shared memory has been corrupted.");
	}
	
	/* scan input data */
	new_seg_node = CreateSegmentNode(CreateSegment(ShmScan(in_var)));
	
	/* copy data to the shared memory */
	ShmCopy(new_seg_node, in_var);
	
	if(s_info->sharetype == msh_SHARETYPE_OVERWRITE)
	{
		DestroySegmentList(&g_seg_list);
	}
	
	/* track the new segment */
	AddSegmentNode(&g_seg_list, new_seg_node);
	
	if(nlhs == 1)
	{
		new_var_node = CreateVariableNode(new_seg_node);
		AddVariableNode(&g_var_list, new_var_node);
		plhs[0] = mxCreateSharedDataCopy(new_var_node->var);
	}
	
	UpdateAll();
	
	ReleaseProcessLock();
	
}


void MshFetch(int nlhs, mxArray** plhs)
{
	
	VariableNode_t* curr_var_node, * first_new_var = NULL;
	SegmentNode_t* curr_seg_node;
	size_t i, num_new_vars = 0;
	size_t ret_dims[2] = {1, 1};
	
	AcquireProcessLock();
	
	UpdateSharedSegments();
	
	if(g_seg_list.num_segs == 0)
	{
		for(i = 0; i < nlhs; i++)
		{
			plhs[i] = mxCreateDoubleMatrix(0, 0, mxREAL);
		}
		return;
	}
	
	switch(s_info->sharetype)
	{
		
		case msh_SHARETYPE_COPY:
			
			RemoveUnusedVariables(&g_var_list);
			
			switch(nlhs)
			{
				case 0:
					/* do nothing */
					return;
				case 1:
					
					if(g_seg_list.last->var_node == NULL)
					{
						AddVariableNode(&g_var_list, CreateVariableNode(g_seg_list.last));
					}
					plhs[0] = mxCreateSharedDataCopy(g_seg_list.last->var_node->var);
					break;
				
				case 2:
				case 3:
					
					curr_seg_node = g_seg_list.first;
					while(curr_seg_node != NULL)
					{
						if(curr_seg_node->var_node == NULL)
						{
							/* create the variable node if it hasnt been created yet */
							AddVariableNode(&g_var_list, CreateVariableNode(curr_seg_node));
							
							if(num_new_vars == 0)
							{
								first_new_var = curr_seg_node->var_node;
							}
							
							num_new_vars += 1;
							
						}
						curr_seg_node = curr_seg_node->next;
					}
					
					g_var_list.num_vars = g_seg_list.num_segs;
					
					plhs[0] = mxCreateSharedDataCopy(g_seg_list.last->var_node->var);
					
					/* return the new variables detected */
					ret_dims[1] = num_new_vars;
					plhs[1] = mxCreateCellArray(2, ret_dims);
					curr_var_node = first_new_var;
					for(i = 0; i < num_new_vars; i++, curr_var_node = curr_var_node->next)
					{
						mxSetCell(plhs[1], i, mxCreateSharedDataCopy(curr_var_node->var));
					}
					
					if(nlhs == 3)
					{
						/* retrieve all segment variables */
						ret_dims[1] = g_seg_list.num_segs;
						plhs[2] = mxCreateCellArray(2, ret_dims);
						curr_seg_node = g_seg_list.first;
						for(i = 0; i < g_seg_list.num_segs; i++, curr_seg_node = curr_seg_node->next)
						{
							mxSetCell(plhs[2], i, mxCreateSharedDataCopy(curr_seg_node->var_node->var));
						}
					}
					
					break;
				
				default:
					ReadErrorMex("OutputError", "Too many outputs.");
			}
			
			break;
		case msh_SHARETYPE_OVERWRITE:
			
			if(nlhs == 0)
			{
				/* in this case MshFetch is just used to clear out unused variables and update this process */
				return;
			}
			else if(nlhs > 1)
			{
				ReadErrorMex("OutputError", "Too many outputs.");
			}
			
			/* check if the current revision number is the same as the shm revision number
			 * if it hasn't been changed then the segment was subject to an inplace change
			 */
			if(g_seg_list.last->var_node == NULL)
			{
				
				/* remove all other variable nodes */
				CleanVariableList(&g_var_list);
				
				/* add the new variable */
				AddVariableNode(&g_var_list, CreateVariableNode(g_seg_list.last));
				
			}
			plhs[0] = mxCreateSharedDataCopy(g_var_list.last->var);
			break;
		default:
			ReleaseProcessLock();
			ReadErrorMex("UnexpectedError", "Invalid sharetype. The shared memory has been corrupted.");
		
	}
	
	ReleaseProcessLock();
}


/**
 * Update the local segment list from shared memory
 */
void UpdateSharedSegments(void)
{
	
	if(g_info->rev_num == s_info->rev_num)
	{
		/* we should be up to date if this is true */
		return;
	}
	else
	{
		/* make sure that subfunctions don't get stuck in a loop by resetting this immediately */
		g_info->rev_num = s_info->rev_num;
	}
	
	seg_num_t curr_seg_num;
	bool_t is_fetched;
	
	SegmentNode_t* curr_seg_node, * next_seg_node, * new_seg_node = NULL;
	SegmentList_t new_seg_list = {NULL, NULL, 0};
	
	curr_seg_node = g_seg_list.first;
	while(curr_seg_node != NULL)
	{
		
		next_seg_node = curr_seg_node->next;
		
		/* Check if the data segment is ready for deletion.
		 * Segment should not be hooked into the shared linked list */
		if(curr_seg_node->seg_info.s_ptr->is_invalid)
		{
			CloseSegmentNode(curr_seg_node);
		}
		
		curr_seg_node = next_seg_node;
	}
	
	/* construct a new list of segments, reusing what we can */
	for(new_seg_list.num_segs = 0, curr_seg_num = s_info->first_seg_num;
	    new_seg_list.num_segs < s_info->num_shared_vars;
	    new_seg_list.num_segs++, curr_seg_num = new_seg_node->seg_info.s_ptr->next_seg_num)
	{
		
		curr_seg_node = g_seg_list.first;
		is_fetched = FALSE;
		while(curr_seg_node != NULL)
		{
			
			/* check if the current segment has been fetched */
			if(curr_seg_node->seg_info.s_ptr->seg_num == curr_seg_num)
			{
				/* hook the currently used node into the list */
				new_seg_node = curr_seg_node;
				
				is_fetched = TRUE;
				break;
			}
			curr_seg_node = curr_seg_node->next;
		}
		
		if(!is_fetched)
		{
			new_seg_node = CreateSegmentNode(OpenSegment(curr_seg_num));
		}
		
		/* take advantage of double link and only set pointers in one direction for now for a split structure*/
		new_seg_node->prev = new_seg_list.last;
		new_seg_list.last = new_seg_node;
		
		new_seg_node->parent_seg_list = &g_seg_list;
		
	}
	
	/* set the new pointers in place */
	if(new_seg_list.num_segs > 0)
	{
		curr_seg_node = new_seg_list.last;
		while(curr_seg_node->prev != NULL)
		{
			curr_seg_node->prev->next = curr_seg_node;
			curr_seg_node = curr_seg_node->prev;
		}
		new_seg_list.first = curr_seg_node;
	}
	
	g_seg_list = new_seg_list;
	
}


size_t ShmScan(const mxArray* in_var)
{
	return ShmScan_(in_var) + PadToAlign(sizeof(SegmentMetadata_t));
}


/* ------------------------------------------------------------------------- */
/* ShmScan_                                                                  */
/*                                                                           */
/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
/*                                                                           */
/* Arguments:                                                                */
/*    header to populate.                                                    */
/*    data container to populate.                                            */
/*    mxArray to analyze.                                                    */
/* Returns:                                                                  */
/*    size that shared memory segment will need to be.                       */
/* ------------------------------------------------------------------------- */
size_t ShmScan_(const mxArray* in_var)
{
	
	if(in_var == NULL)
	{
		ReleaseProcessLock();
		ReadErrorMex("UnexpectedError", "Input variable was unexpectedly NULL.");
	}
	
	/* counters */
	Header_t hdr;
	size_t idx, count, cml_sz = 0, metadata_sz = 0;
	int field_num;
	
	const size_t padded_mxmalloc_sig_len = PadToAlign(MXMALLOC_SIG_LEN);
	
	hdr.num_dims = mxGetNumberOfDimensions(in_var);
	hdr.elem_size = mxGetElementSize(in_var);
	hdr.num_elems = mxGetNumberOfElements(in_var);
	hdr.nzmax = mxGetNzmax(in_var);
	hdr.num_fields = mxGetNumberOfFields(in_var);
	hdr.classid = mxGetClassID(in_var);
	hdr.complexity = mxIsComplex(in_var)? mxCOMPLEX : mxREAL;
	hdr.is_sparse = mxIsSparse(in_var);
	hdr.is_numeric = mxIsNumeric(in_var);
	
	/* Add space for the header */
	metadata_sz += sizeof(Header_t);
	
	/* Add space for the dimensions */
	metadata_sz += mxGetNumberOfDimensions(in_var)*sizeof(mwSize);
	
	/* Structure case */
	if(hdr.classid == mxSTRUCT_CLASS)
	{
		
		/* add size of storing the offsets */
		metadata_sz += (hdr.num_fields*hdr.num_elems)*sizeof(size_t);
		
		/* Add space for the field string */
		metadata_sz += GetFieldNamesSize(in_var);
		
		cml_sz += PadToAlign(metadata_sz);
		
		/* go through each recursively */
		for(field_num = 0, count = 0; field_num < hdr.num_fields; field_num++)          /* each field */
		{
			for(idx = 0; idx < hdr.num_elems; idx++, count++)                         /* each element */
			{
				/* call recursivley */
				cml_sz += ShmScan_(mxGetFieldByNumber(in_var, idx, field_num));
			}
			
		}
	}
	else if(hdr.classid == mxCELL_CLASS) /* Cell case */
	{
		
		/* add size of storing the offsets */
		metadata_sz += hdr.num_elems*sizeof(size_t);
		
		cml_sz += PadToAlign(metadata_sz);
		
		/* go through each recursively */
		for(count = 0; count < hdr.num_elems; count++)
		{
			cml_sz += ShmScan_(mxGetCell(in_var, count));
		}
	}
	else if(hdr.is_numeric || hdr.classid == mxLOGICAL_CLASS || hdr.classid == mxCHAR_CLASS)  /* a matrix containing data *//* base case */
	{
		
		cml_sz += PadToAlign(metadata_sz);
		
		if(hdr.is_sparse)
		{
			/* len(data)==nzmax, len(imag_data)==nzmax, len(ir)=nzmax, len(jc)==N+1 */
			
			/* ensure both pointers are aligned individually */
#ifdef MX_HAS_INTERLEAVED_COMPLEX
			cml_sz += padded_mxmalloc_sig_len + PadToAlign((1 + mxIsComplex(in_var))*hdr.elem_size*hdr.nzmax);
#else
			cml_sz += padded_mxmalloc_sig_len + PadToAlign(hdr.elem_size*hdr.nzmax);
			if(hdr.complexity == mxCOMPLEX)
			{
				cml_sz += padded_mxmalloc_sig_len + PadToAlign(hdr.elem_size*hdr.nzmax);
			}
#endif
			cml_sz += padded_mxmalloc_sig_len + PadToAlign(sizeof(mwIndex)*(hdr.nzmax));              /* ir */
			cml_sz += padded_mxmalloc_sig_len + PadToAlign(sizeof(mwIndex)*(mxGetN(in_var) + 1));     /* jc */
			
		}
		else
		{
			/* ensure both pointers are aligned individually */
			if(!MshIsEmpty(hdr))
			{
#ifdef MX_HAS_INTERLEAVED_COMPLEX
				cml_sz += padded_mxmalloc_sig_len + PadToAlign((1 + mxIsComplex(in_var))*hdr.elem_size*hdr.num_elems);
#else
				cml_sz += padded_mxmalloc_sig_len + PadToAlign(hdr.elem_size*hdr.num_elems);
				if(hdr.complexity == mxCOMPLEX)
				{
					cml_sz += padded_mxmalloc_sig_len + PadToAlign(hdr.elem_size*hdr.num_elems);
				}
#endif
			}
		}
		
	}
	else
	{
		ReleaseProcessLock();
		ReadErrorMex("Internal:UnexpectedError", "Tried to clone an unsupported type.");
	}
	
	return cml_sz;
}


void ShmCopy(SegmentNode_t* seg_node, const mxArray* in_var)
{
	ShmCopy_(((byte_t*)seg_node->seg_info.s_ptr) + PadToAlign(sizeof(SegmentMetadata_t)), in_var);
}


/* ------------------------------------------------------------------------- */
/* ShmCopy_                                                                  */
/*                                                                           */
/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
/*                                                                           */
/* Arguments:                                                                */
/*    header to serialize.                                                   */
/*    data container of pointers to data to serialize.                       */
/*    pointer to shared memory segment.                                      */
/* Returns:                                                                  */
/*    void                                                                   */
/* ------------------------------------------------------------------------- */
size_t ShmCopy_(byte_t* shm_anchor, const mxArray* in_var)
{
	
	const size_t padded_mxmalloc_sig_len = PadToAlign(MXMALLOC_SIG_LEN);
	
	Header_t hdr;
	byte_t* shm_ptr = shm_anchor;
	
	size_t cml_off = 0, metadata_off = 0, shift, idx, cpy_sz, count;
	
	int field_num;
	const char_t* field_name;
	mwSize* dims;
	size_t* child_hdrs;
	char_t* field_str;
	void* data;
#ifndef MX_HAS_INTERLEAVED_COMPLEX
	void* imag_data;
#endif
	mwIndex* ir, * jc;
	
	/* initialize header info */
	
	/* these are updated when the data is copied over */
	hdr.data_offsets.data = SIZE_MAX;
#ifndef MX_HAS_INTERLEAVED_COMPLEX
	hdr.data_offsets.imag_data = SIZE_MAX;
#endif
	hdr.data_offsets.ir = SIZE_MAX;
	hdr.data_offsets.jc = SIZE_MAX;
	
	hdr.num_dims = mxGetNumberOfDimensions(in_var);
	hdr.elem_size = mxGetElementSize(in_var);
	hdr.num_elems = mxGetNumberOfElements(in_var);
	hdr.nzmax = mxGetNzmax(in_var);
	hdr.num_fields = mxGetNumberOfFields(in_var);
	hdr.classid = mxGetClassID(in_var);
	hdr.complexity = mxIsComplex(in_var)? mxCOMPLEX : mxREAL;
	hdr.is_sparse = mxIsSparse(in_var);
	hdr.is_numeric = mxIsNumeric(in_var);
	
	shift = sizeof(Header_t);
	metadata_off += shift, shm_ptr += shift;
	
	/* copy the dimensions */
	cpy_sz = hdr.num_dims*sizeof(mwSize);
	dims = (mwSize*)shm_ptr;
	memcpy(dims, mxGetDimensions(in_var), cpy_sz);
	
	shift = hdr.num_dims*sizeof(mwSize);
	metadata_off += shift, shm_ptr += shift;
	
	/* Structure case */
	if(hdr.classid == mxSTRUCT_CLASS)
	{
		
		/* store the child headers here */
		hdr.data_offsets.child_offs = metadata_off;
		child_hdrs = (size_t*)shm_ptr;
		shift = (hdr.num_fields*hdr.num_elems)*sizeof(size_t);
		metadata_off += shift, shm_ptr += shift;
		
		/* store the field string here */
		hdr.data_offsets.field_str = metadata_off;
		field_str = (char_t*)shm_ptr;
		
		/*shift past the field string */
		shift = GetFieldNamesSize(in_var);
		metadata_off += shift; /* , shm_ptr += shift; */
		
		cml_off += PadToAlign(metadata_off);
		
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
				child_hdrs[count] = cml_off;
				
				/* And fill it */
				cml_off += ShmCopy_(shm_anchor + child_hdrs[count], mxGetFieldByNumber(in_var, idx, field_num));
				
			}
			
		}
		
	}
	else if(hdr.classid == mxCELL_CLASS) /* Cell case */
	{
		
		/* store the child headers here */
		hdr.data_offsets.child_offs = metadata_off;
		child_hdrs = (size_t*)shm_ptr;
		metadata_off += hdr.num_elems*sizeof(size_t);
		
		cml_off += PadToAlign(metadata_off);
		
		/* recurse for each cell element */
		for(count = 0; count < hdr.num_elems; count++)
		{
			/* place relative offset into shared memory */
			child_hdrs[count] = cml_off;
			
			cml_off += ShmCopy_(shm_anchor + child_hdrs[count], mxGetCell(in_var, count));
		}
		
	}
	else /* base case */
	{
		
		cml_off += PadToAlign(metadata_off), shm_ptr = shm_anchor + cml_off;
		
		/* and the indices of the sparse data as well */
		if(hdr.is_sparse)
		{
			
			/* note: in this case hdr->is_empty means the sparse is 0x0
			 *  also note that nzmax is still 1 */
			
			/* copy data */
#ifdef MX_HAS_INTERLEAVED_COMPLEX
			cpy_sz = (1 + mxIsComplex(in_var))*(hdr.nzmax)*(hdr.elem_size);
			
			/* add the size of the mxMalloc signature */
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.data = cml_off, data = shm_ptr;
			
			/* copy over the data with the signature */
			MemCpyMex(data, mxGetData(in_var), cpy_sz);
			
			shift = PadToAlign(cpy_sz);
			cml_off += shift, shm_ptr += shift;
#else
			cpy_sz = (hdr.nzmax)*(hdr.elem_size);
			
			/* add the size of the mxMalloc signature */
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.data = cml_off, data = shm_ptr;
			
			/* copy over the data with the signature */
			MemCpyMex(data, mxGetData(in_var), cpy_sz);
			
			shift = PadToAlign(cpy_sz);
			cml_off += shift, shm_ptr += shift;
			
			/* copy imag_data */
			if(hdr.complexity == mxCOMPLEX)
			{
				cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
				hdr.data_offsets.imag_data = cml_off, imag_data = shm_ptr;
				
				MemCpyMex(imag_data, mxGetImagData(in_var), cpy_sz);
				
				shift = PadToAlign(cpy_sz);
				cml_off += shift, shm_ptr += shift;
				
			}
#endif
			
			/* copy ir */
			cpy_sz = hdr.nzmax*sizeof(mwIndex);
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.ir = cml_off, ir = (mwIndex*)shm_ptr;
			
			MemCpyMex((void*)ir, (void*)mxGetIr(in_var), cpy_sz);
			
			shift = PadToAlign(cpy_sz);
			cml_off += shift, shm_ptr += shift;
			
			/* copy jc */
			cpy_sz = (dims[1] + 1)*sizeof(mwIndex);
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.jc = cml_off, jc = (mwIndex*)shm_ptr;
			
			MemCpyMex((void*)jc, (void*)mxGetJc(in_var), cpy_sz);
			
			shift = PadToAlign(cpy_sz);
			cml_off += shift;/*, shm_ptr += shift;*/
			
		}
		else
		{
			/* copy data */
			if(!MshIsEmpty(hdr))
			{
#ifdef MX_HAS_INTERLEAVED_COMPLEX
				cpy_sz = (1 + mxIsComplex(in_var))*(hdr.num_elems)*(hdr.elem_size);
			
				/* add the size of the mxMalloc signature */
				cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
				hdr.data_offsets.data = cml_off, data = shm_ptr;
				
				/* copy over the data with the signature */
				MemCpyMex(data, mxGetData(in_var), cpy_sz);
				
				shift = PadToAlign(cpy_sz);
				cml_off += shift, shm_ptr += shift;
#else
				cpy_sz = hdr.num_elems*hdr.elem_size;
				
				/* add the size of the mxMalloc signature */
				cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
				hdr.data_offsets.data = cml_off, data = shm_ptr;
				
				/* copy over the data with the signature */
				MemCpyMex(data, mxGetData(in_var), cpy_sz);
				
				shift = PadToAlign(cpy_sz);
				cml_off += shift, shm_ptr += shift;
				
				/* copy imag_data */
				if(hdr.complexity == mxCOMPLEX)
				{
					cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
					hdr.data_offsets.imag_data = cml_off, imag_data = shm_ptr;
					
					MemCpyMex(imag_data, mxGetImagData(in_var), cpy_sz);
					
					shift = PadToAlign(cpy_sz);
					cml_off += shift;/* , shm_ptr += shift; */
					
				}
#endif
			}
		}
		
	}
	
	memcpy(shm_anchor, &hdr, sizeof(Header_t));
	
	return cml_off;
	
}


void ShmFetch(byte_t* shm_anchor, mxArray** ret_var)
{
	ShmFetch_(shm_anchor + PadToAlign(sizeof(SegmentMetadata_t)), ret_var);
	mexMakeArrayPersistent(*ret_var);
}


void ShmFetch_(byte_t* shm_anchor, mxArray** ret_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	mxArray* ret_child;
	
	/* for working with payload ... */
	Header_t* hdr;
	
	/* for structures */
	int field_num;                /* current field */
	
	const char_t** field_names;
	const char_t* field_name;
	
	/* retrieve the data */
	hdr = (Header_t*)shm_anchor;
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
				ShmFetch_(shm_anchor + data_ptrs.child_hdrs[count], &ret_child);
				mxSetFieldByNumber(*ret_var, idx, field_num, ret_child);
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		*ret_var = mxCreateCellArray(hdr->num_dims, data_ptrs.dims);
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			ShmFetch_(shm_anchor + data_ptrs.child_hdrs[count], &ret_child);
			mxSetCell(*ret_var, count, ret_child);
		}
		
	}
	else     /*base case*/
	{
		
		if(hdr->is_sparse)
		{
			
			if(hdr->classid == mxDOUBLE_CLASS)
			{
				*ret_var = mxCreateSparse(0, 0, 1, hdr->complexity);
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateSparseLogicalMatrix(0, 0, 1);
			}
			else
			{
				ReleaseProcessLock();
				ReadErrorMex("UnrecognizedTypeError", "The fetched array was of class 'sparse' but not of type 'double' or 'logical'");
			}
			
			/* free the real and imaginary data */
			mxFree(mxGetData(*ret_var));

#ifndef MX_HAS_INTERLEAVED_COMPLEX
			if(hdr->complexity)
			{
				mxFree(mxGetImagData(*ret_var));
			}
#endif
			
			/* free the pointers relating to sparse */
			mxFree(mxGetIr(*ret_var));
			mxFree(mxGetJc(*ret_var));
			
			mxSetNzmax(*ret_var, hdr->nzmax);
			
		}
		else
		{
			
			if(hdr->is_numeric)
			{
				*ret_var = mxCreateNumericArray(0, NULL, hdr->classid, hdr->complexity);
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateLogicalArray(0, NULL);
			}
			else
			{
				*ret_var = mxCreateCharArray(0, NULL);
			}
			
		}
		
		/* set all data */
		SetDataPointers(*ret_var, &data_ptrs);
		mxSetDimensions(*ret_var, data_ptrs.dims, hdr->num_dims);
		
	}
}


/* ------------------------------------------------------------------------- */
/* ShmDetach                                                                */
/*                                                                           */
/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
/*                                                                           */
/* Arguments:                                                                */
/*    Input matrix to remove references.                                     */
/* Returns:                                                                  */
/*    Pointer to start of shared memory segment.                             */
/* ------------------------------------------------------------------------- */
void ShmDetach(mxArray* ret_var)
{
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
#ifdef MX_HAS_INTERLEAVED_COMPLEX
			new_data_ptrs.data = mxCalloc((1 + mxIsComplex(ret_var))*nzmax, mxGetElementSize(ret_var));
#else
			new_data_ptrs.data = mxCalloc(nzmax, mxGetElementSize(ret_var));
			if(mxIsComplex(ret_var))
			{
				new_data_ptrs.imag_data = mxCalloc(nzmax, mxGetElementSize(ret_var));
			}
#endif
			
			new_data_ptrs.ir = mxCalloc(nzmax, sizeof(mwIndex));
			new_data_ptrs.jc = mxCalloc(new_dims[1] + 1, sizeof(mwIndex));
			
			mxSetNzmax(ret_var, nzmax);
			
		}
		
		SetDataPointers(ret_var, &new_data_ptrs);
		mxSetDimensions(ret_var, new_dims, num_dims);
		
		/** HACK **/
		/* reset all the crosslinks so nothing in MATLAB is pointing to shared data (which will be gone soon) */
		mxArray* link;
		for(link = ((mxArrayStruct*)ret_var)->CrossLink; link != NULL && link != ret_var; link = ((mxArrayStruct*)link)->CrossLink)
		{
			
			if(mxIsSparse(link))
			{
				mxSetNzmax(ret_var, nzmax);
			}
			
			SetDataPointers(link, &new_data_ptrs);
			mxSetDimensions(link, new_dims, num_dims);
			
		}
		
	}
	else
	{
		ReleaseProcessLock();
		ReadErrorMex("InvalidTypeError", "Unsupported type. The segment may have been corrupted.");
	}
}


void ShmRewrite(byte_t* shm_anchor, const mxArray* in_var, mxArray* rewrite_var)
{
	ShmRewrite_(shm_anchor + PadToAlign(sizeof(SegmentMetadata_t)), in_var, rewrite_var);
}


void ShmRewrite_(byte_t* shm_anchor, const mxArray* in_var, mxArray* rewrite_var)
{
	size_t idx, count;
	
	/* for working with payload */
	Header_t* hdr;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*)shm_anchor;
	SharedDataPointers_t data_ptrs = LocateDataPointers(hdr, shm_anchor);
	
	const mwSize* dims = mxGetDimensions(in_var);
	mwSize num_dims = mxGetNumberOfDimensions(in_var);
	
	/* begin hack (set the dimensions in all crosslinks) */
	mxArray* curr_var = rewrite_var;
	do
	{
		mxSetDimensions(curr_var, dims, num_dims);
		curr_var = ((mxArrayStruct*)curr_var)->CrossLink;
	} while(curr_var != NULL && curr_var != rewrite_var);
	/* end hack */
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < hdr->num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < hdr->num_elems; idx++, count++)
			{
				/* And fill it */
				ShmRewrite_(shm_anchor + data_ptrs.child_hdrs[count], mxGetFieldByNumber(in_var, idx, field_num), mxGetFieldByNumber(rewrite_var, idx, field_num));
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		for(count = 0; count < hdr->num_elems; count++)
		{
			/* And fill it */
			ShmRewrite_(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(in_var, count), mxGetCell(rewrite_var, count));
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

#ifdef MX_HAS_INTERLEAVED_COMPLEX
		/* rewrite all data */
		memcpy(data_ptrs.data, mxGetData(in_var), (1 + mxIsComplex(in_var))*(hdr->num_elems)*(hdr->elem_size));
#else
		/* rewrite real data */
		memcpy(data_ptrs.data, mxGetData(in_var), (hdr->num_elems)*(hdr->elem_size));
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			memcpy(data_ptrs.imag_data, mxGetImagData(in_var), (hdr->num_elems)*(hdr->elem_size));
		}
#endif
	
	}
	
}


bool_t ShmCompareSize(byte_t* shm_anchor, const mxArray* comp_var)
{
	return ShmCompareSize_(shm_anchor + PadToAlign(sizeof(SegmentMetadata_t)), comp_var);
}


bool_t ShmCompareSize_(byte_t* shm_anchor, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	Header_t* hdr = (Header_t*)shm_anchor;
	SharedDataPointers_t data_ptrs = LocateDataPointers(hdr, shm_anchor);
	
	const char_t* field_name;
	
	if(mxGetClassID(comp_var) != hdr->classid)
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
				if(!ShmCompareSize_(shm_anchor + data_ptrs.child_hdrs[count], mxGetFieldByNumber(comp_var, idx, field_num)))
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
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			if(!ShmCompareSize_(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(comp_var, count)))
			{
				return FALSE;
			}
		}
	}
	else     /*base case*/
	{
		
		/* this is the address of the first data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity != mxIsComplex(comp_var))
		{
			return FALSE;
		}
		
		if(hdr->is_sparse)
		{
			if(!mxIsSparse(comp_var) || hdr->nzmax != mxGetNzmax(comp_var) || data_ptrs.dims[1] != mxGetN(comp_var))
			{
				return FALSE;
			}
		}
		else
		{
			if(mxIsSparse(comp_var) || hdr->num_elems != mxGetNumberOfElements(comp_var))
			{
				return FALSE;
			}
		}
		
	}
	
	return TRUE;
	
}


#ifdef MSH_CMP_CONT

mxLogical ShmCompareContent(byte_t* shm_anchor, const mxArray* comp_var)
{
	return ShmCompareContent_(shm_anchor + PadToAlign(sizeof(SegmentMetadata_t)), comp_var);
}


mxLogical ShmCompareContent_(byte_t* shm_anchor, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	
	/* for working with payload ... */
	Header_t* hdr;
	SharedDataPointers_t data_ptrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*) shm_anchor;
	LocateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	const char_t* field_name;
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	if(hdr->num_dims != mxGetNumberOfDimensions(comp_var) ||
	   memcmp((data_ptrs.dims), mxGetDimensions(comp_var), sizeof(mwSize) * hdr->num_dims) != 0
	   || hdr->num_elems != mxGetNumberOfElements(comp_var))
	{
		return FALSE;
	}
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		if(hdr->num_fields != mxGetNumberOfFields(comp_var))
		{
			return FALSE;
		}
		
		/* Go through each element */
		field_name = data_ptrs.field_str;
		for(field_num = 0, count = 0; field_num < hdr->num_fields; field_num++)     /* each field */
		{
			
			if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_name) != 0)
			{
				return FALSE;
			}
			
			for(idx = 0; idx < hdr->num_elems; idx++, count++)
			{
				
				if(!ShmCompareContent_(shm_anchor + data_ptrs.child_offs[count], mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
			
			GetNextFieldName(&field_name);
			
		}
		
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			if(!ShmCompareContent_(shm_anchor + data_ptrs.child_offs[count], mxGetCell(comp_var, count)))
			{
				return FALSE;
			}
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		
		//these should point to the same location
		if(data_ptrs.data != mxGetData(comp_var))
		{
			return FALSE;
		}
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			if(mxIsComplex(comp_var) != TRUE)
			{
				return FALSE;
			}
			
			
			//these should point to the same location
			if(data_ptrs.imag_data != mxGetImagData(comp_var))
			{
				return FALSE;
			}
			
		}
		
		if(hdr->is_sparse)
		{
			
			if(!mxIsSparse(comp_var))
			{
				return FALSE;
			}
			
			if(data_ptrs.ir != mxGetIr(comp_var) || data_ptrs.jc != mxGetJc(comp_var))
			{
				return FALSE;
			}
			
		}
		
	}
	
	return TRUE;
	
}

#endif






