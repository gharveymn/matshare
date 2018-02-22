#include "headers/matshare_.hpp"

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{

	/* For inputs */
	const mxArray* mxDirective;               /*  Directive {clone, attach, detach, free}   */
	const mxArray* mxInput;                /*  Input array (for clone)					  */
	
	/* For output */
	mxArray* mxOutput;
	
	/* For storing inputs */
	msh_directive_t directive;
	
	size_t sm_size;                              /* size required by the shared memory */
	
	/* for storing the mxArrays ... */
	header_t hdr;
	data_t dat;
	
	mxLogical* ret_data = nullptr;
	size_t tmp = 0;
	
	segment_info* seg_info;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		mexErrMsgIdAndTxt("MATLAB:SharedMemory", "Minimum input arguments missing; must supply directive and key.");
	}
	
	/* assign inputs */
	mxDirective = prhs[0];
	
	/* get directive */
	directive = (msh_directive_t)*((uint8_t*)(mxGetData(mxDirective)));
	DWORD hi_sz, lo_sz, err;
	
	/* Switch yard {clone, attach, detach, free} */
	switch(directive)
	{
		case msh_INIT:
			init();
			plhs[0] = mxDuplicateArray(global_shared_variable);
			//mexLock();
			break;
		case msh_CLONE:
			/********************/
			/*	Clone case		*/
			/********************/
			
			/* check the inputs */
			if((nrhs < 2) || (mxIsEmpty(prhs[1])))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone",
							   "Required second argument is missing (variable) or empty.");
			}
			
			/* Assign */
			mxInput = prhs[1];
			
			/* scan input data */
			sm_size = deepscan(&hdr, &dat, mxInput, nullptr, &global_shared_variable);

#ifdef DEBUG
			mexPrintf("clone: Debug: deepscan done.\n");
#endif
			
			// get current map number
			seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
			seg_info->segment_number += 1;
			current_segment_info->segment_number = seg_info->segment_number;
			seg_info->segment_size = sm_size;
			current_segment_info->segment_size = sm_size;
			memcpy(MSH_SEGMENT_NAME + MSH_SEG_NAME_PREAMB_LEN - 1, &current_segment_info->segment_number, sizeof(uint32_t));
			
			UnmapViewOfFile(*current_segment_p);
			CloseHandle(*shm_handle);
			
			// create the new mapping
			lo_sz =  (DWORD)(sm_size & 0xFFFFFFFFL);
			hi_sz =  (DWORD)((sm_size >> 32) & 0xFFFFFFFFL);
			*shm_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, MSH_SEGMENT_NAME);
			err = GetLastError();
			if(*shm_handle == nullptr)
			{
				mexPrintf("Error number: %d\n", err);
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
							   "SharedMemory::Could not create the memory segment");
			}
			
			*current_segment_p = (byte_t*)MapViewOfFile(*shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, sm_size);
			
			/* copy data to the shared memory */
			deepcopy(&hdr, &dat, *current_segment_p, nullptr);
			
			/* free temporary allocation */
			deepfree(&dat);

#ifdef DEBUG
			mexPrintf("clone: Debug: shared memory at: 0x%016x\n", pRegion->get_address());
#endif
			
			plhs[0] = global_shared_variable;
			UnmapViewOfFile(seg_info);
			//mexLock();
			break;
		case msh_ATTACH:
			/********************/
			/*	Attach case	*/
			/********************/

			// get current map size
			updateSegmentInfo();
			
			/* Restore the segment if this an initialized region */
			if(current_segment_info->segment_size != NULLSEGMENT_SZ)
			{
				shallowrestore(*current_segment_p, global_shared_variable);
			}
			break;
		case msh_DETACH:
			/********************/
			/*	Dettach case	*/
			/********************/
			
//			mexPrintf("%d", mxGetNumberOfElements(global_shared_variable));
//			mexPrintf("%d", mxIsEmpty(global_shared_variable));
			
			if(!mxIsEmpty(global_shared_variable))
			{
				/* NULL all of the Matlab pointers */
				deepdetach(global_shared_variable);
			}
			else
			{
				mxDestroyArray(global_shared_variable);
			}
			
			global_shared_variable = mxCreateNumericArray(0, nullptr, mxUINT8_CLASS, mxREAL);
			mexMakeArrayPersistent(global_shared_variable);
			plhs[0] = mxDuplicateArray(global_shared_variable);
			
			break;
		case msh_FREE:
			/********************/
			/*	free case		*/
			/********************/
			//CloseHandle(*shm_handle);
			//mexUnlock();
			break;
		case msh_FETCH:
			
			updateSegmentInfo();
			UnmapViewOfFile(*current_segment_p);
			CloseHandle(*shm_handle);
			if(current_segment_info->segment_size != NULLSEGMENT_SZ)
			{
				*shm_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, true, MSH_SEGMENT_NAME);
				*current_segment_p = (byte_t*)MapViewOfFile(*shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, current_segment_info->segment_size);
				shallowfetch(*current_segment_p, &global_shared_variable);
				plhs[0] = global_shared_variable;
			}
			else
			{
				mxDestroyArray(global_shared_variable);
				global_shared_variable = mxCreateNumericArray(0, nullptr, mxUINT8_CLASS, mxREAL);
				mexMakeArrayPersistent(global_shared_variable);
				plhs[0] = mxDuplicateArray(global_shared_variable);
			}
			break;
		case msh_COMPARE:
			
			plhs[0] = mxCreateLogicalMatrix(1,1);
			ret_data = mxGetLogicals(plhs[0]);
			seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
			*ret_data = (current_segment_info->segment_number == seg_info->segment_number);
			UnmapViewOfFile(seg_info);
			break;
		default:
			mexErrMsgIdAndTxt("MATLAB:SharedMemory", "Unrecognized directive.");
			/* unrecognised directive */
			break;
	}
	
	
}


/* ------------------------------------------------------------------------- */
/* deepdetach                                                                */
/*                                                                           */
/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
/*                                                                           */
/* Arguments:                                                                */
/*    Input matrix to remove references.                                     */
/* Returns:                                                                  */
/*    Pointer to start of shared memory segment.                             */
/* ------------------------------------------------------------------------- */
void deepdetach(mxArray* ret_var)
{
	
	/* uses side-effects! */
	mwSize dims[] = {0, 0};
	mwSize nzmax = 0;
	size_t elemsiz;
	size_t i, n;
	
	/*for structure */
	size_t nFields, j;
	
	/* restore matlab  memory */
	if(ret_var == (mxArray*) nullptr || mxIsEmpty(ret_var))
	{
		return;
	}
	else if(mxIsStruct(ret_var))
	{
		/* detach each field for each element */
		n = mxGetNumberOfElements(ret_var);
		nFields = mxGetNumberOfFields(ret_var);
		for(i = 0; i < n; i++)                         /* element */
		{
			for(j = 0; j < nFields; j++)               /* field */
			{
				deepdetach((mxArray*) (mxGetFieldByNumber(ret_var, i, j)));
			}/* detach this one */
		}
	}
	else if(mxIsCell(ret_var))
	{
		/* detach each cell */
		n = mxGetNumberOfElements(ret_var);
		for(i = 0; i < n; i++)
		{
			deepdetach((mxArray*) (mxGetCell(ret_var, i)));
		}/* detach this one */
		
	}
	else if(mxIsNumeric(ret_var) || mxIsLogical(ret_var) || mxIsChar(ret_var))  /* a matrix containing data */
	{
		
		/* In safe mode these entries were allocated so remove them properly */
		
		/* handle sparse objects */
		if(mxIsSparse(ret_var))
		{
			/* I don't seem to be able to give sparse arrays zero size so (nzmax must be 1) */
			dims[0] = dims[1] = 1;
			nzmax = 1;
			if(mxSetDimensions(ret_var, dims, 2))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:Unknown", "detach: unable to resize the array.");
			}
			
			/* allocate 1 element */
			elemsiz = mxGetElementSize(ret_var);
			mxSetData(ret_var, mxMalloc(1));
			if(mxIsComplex(ret_var))
			{
				mxSetImagData(ret_var, mxMalloc(1));
			}
		}
		else
		{
			/* Doesn't allocate or deallocate any space for the pr or pi arrays */
			mxSetDimensions(ret_var, dims, 2);
			mxSetData(ret_var, mxMalloc(0));
			mxSetImagData(ret_var, mxMalloc(0));
		}
	}
	else
	{
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:InvalidInput", "detach: unsupported type.");
	}
}



/* ------------------------------------------------------------------------- */
/* shallowrestore                                                            */
/*                                                                           */
/* Shallow copy matrix from shared memory into Matlab form.                  */
/*                                                                           */
/* Arguments:                                                                */
/*    shared memory segment.                                                 */
/*    pointer to an mxArray pointer.                                         */
/* Returns:                                                                  */
/*    size of shared memory segment.                                         */
/* ------------------------------------------------------------------------- */
size_t shallowrestore(char* shm, mxArray* ret_var)
{
	
	/* for working with shared memory ... */
	size_t i;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* pr = nullptr, * pi = nullptr;  /* real and imaginary data pointers */
	mwIndex* ir = nullptr, * jc = nullptr;  /* sparse matrix data indices */
	
	
	/* for structures */
	size_t field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*) shm;
	shm += pad_to_align(sizeof(header_t));
	
	/* the size pointer */
	pSize = (mwSize*) shm;
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize) * hdr->nDims);
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
#ifdef DEBUG
		mexPrintf("shallowrestore: Debug: found structure %d x %d.\n", pSize[0], pSize[1]);
#endif
		
		/* skip over the stored field string */
		shm += pad_to_align(hdr->strBytes);
		
		/* Go through each element */
		for(i = 0; i < hdr->nzmax; i++)
		{
			for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
			{
				/* And fill it */
				shm += shallowrestore(shm, mxGetFieldByNumber(ret_var, i, field_num));
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		for(i = 0; i < hdr->nzmax; i++)
		{
			/* And fill it */
			shm += shallowrestore(shm, mxGetCell(ret_var,i));
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		shm += pad_to_align(MXMALLOC_SIG_LEN);
		pr = (void*) shm;
		shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));          /* takes us to the end of the real data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			pi = (void*) shm;
			shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));     /* takes us to the end of the complex data */
		}
		
		/* if sparse get a list of the elements */
		if(hdr->isSparse)
		{
			
			ir = (mwIndex*) shm;
			shm += pad_to_align((hdr->nzmax) * sizeof(mwIndex));
			
			jc = (mwIndex*) shm;
			shm += pad_to_align((pSize[1] + 1) * sizeof(mwIndex));
			
			/* set the pointers relating to sparse (set the real and imaginary data later)*/
			mxFree(mxGetIr(ret_var));
			mwIndex* ret_ir = (mwIndex*)mxMalloc(hdr->nzmax * sizeof(mwIndex));
			memcpy(ret_ir, ir, hdr->nzmax * sizeof(mwIndex));
			mxSetIr(ret_var, ret_ir);
			
			mxSetNzmax(ret_var, hdr->nzmax);
			
		}
		else
		{
			
			/*  rebuild constitute the array  */
			mxSetDimensions(ret_var, pSize, hdr->nDims);
		
		}
		
		/* set the real and imaginary data */
		
		
		/* Safe - but takes it out of global memory */
		mxFree(mxGetData(ret_var));
		mxSetData(ret_var, pr);
		if ( hdr->complexity )
		{
			mxFree(mxGetImagData(ret_var));
			mxSetImagData(ret_var, pi);
		}
	
	}
	return hdr->shmsiz;
}


size_t shallowfetch(byte_t* shm, mxArray** ret_var)
{
	/* for working with shared memory ... */
	size_t i;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* pr = nullptr, * pi = nullptr;  /* real and imaginary data pointers */
	mwIndex* ir = nullptr, * jc = nullptr;  /* sparse matrix data indices */
	
	
	/* for structures */
	size_t field_num, shmshift;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*) shm;
	shm += pad_to_align(sizeof(header_t));
	
	
	/* the size pointer */
	pSize = (mwSize*) shm;
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize) * hdr->nDims);
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		char** field_names = (char**)mxCalloc((size_t)hdr->nFields, sizeof(char*));
		PointCharArrayAtString(field_names, shm, hdr->nFields);
		
		/* skip over the stored field string */
		shm += pad_to_align(hdr->strBytes);
		
		*ret_var = mxCreateStructArray(hdr->nDims, pSize, hdr->nFields, (const char**)field_names);
		mexMakeArrayPersistent(*ret_var);
		
		/* Go through each element */
		for(i = 0; i < hdr->nzmax; i++)
		{
			for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
			{
				mxArray* ret_child = mxGetFieldByNumber(*ret_var, i, field_num);
				shm += shallowfetch(shm, &ret_child);
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		*ret_var = mxCreateCellArray(hdr->nDims, pSize);
		mexMakeArrayPersistent(*ret_var);
		for(i = 0; i < hdr->nzmax; i++)
		{
			mxArray* ret_child = mxGetCell(*ret_var, i);
			shm += shallowfetch(shm, &ret_child);
		}
	}
	else     /*base case*/
	{
		shm += pad_to_align(MXMALLOC_SIG_LEN);
		pr = (void*) shm;
		shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));          /* takes us to the end of the real data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			pi = (void*) shm;
			shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));     /* takes us to the end of the complex data */
		}
		
		if(hdr->isSparse)
		{
			
			ir = (mwIndex*) shm;
			shm += pad_to_align((hdr->nzmax) * sizeof(mwIndex));
			
			jc = (mwIndex*) shm;
			shm += pad_to_align((pSize[1] + 1) * sizeof(mwIndex));
			
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateSparse(pSize[0], pSize[1], 1, hdr->complexity);
			}
			else
			{
				*ret_var = mxCreateSparseLogicalMatrix(pSize[0], pSize[1], 1);
			}
			
			mwIndex* ret_jc = mxGetJc(*ret_var);
			memcpy(ret_jc, jc, hdr->nzmax * sizeof(mwIndex));
			ret_jc[pSize[1]] = 0;
			
		}
		else
		{
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateNumericArray(0, nullptr, hdr->classid, hdr->complexity);
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateLogicalArray(0, nullptr);
			}
			else
			{
				*ret_var = mxCreateCharArray(0, nullptr);
			}
		}
		mexMakeArrayPersistent(*ret_var);
		
	}
	return hdr->shmsiz;
}



/* ------------------------------------------------------------------------- */
/* deepscan                                                                  */
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
size_t deepscan(header_t* hdr, data_t* dat, const mxArray* mxInput, header_t* par_hdr, mxArray** ret_var)
{
	/* counter */
	size_t i;
	
	/* for structures */
	size_t field_num, count;
	
	/* initialize data info; _possibly_ update these later */
	dat->pSize = (mwSize*) nullptr;
	dat->pr = (void*) nullptr;
	dat->pi = (void*) nullptr;
	dat->ir = (mwIndex*) nullptr;
	dat->jc = (mwIndex*) nullptr;
	dat->child_dat = (data_t*) nullptr;
	dat->child_hdr = (header_t*) nullptr;
	dat->field_str = (char*) nullptr;
	hdr->par_hdr_off = 0;               /*don't record it here */
	
	if(mxInput == (const mxArray*) nullptr || mxIsEmpty(mxInput))
	{
		/* initialize header info */
		hdr->isNumeric = false;
		hdr->isSparse = false;
		hdr->complexity = mxREAL;
		hdr->classid = mxUNKNOWN_CLASS;
		hdr->nDims = 0;
		hdr->elemsiz = 0;
		hdr->nzmax = 0;
		hdr->nFields = 0;
		hdr->strBytes = 0;
		hdr->shmsiz = pad_to_align(sizeof(header_t));
		
		return hdr->shmsiz;
	}
	
	/* initialize header info */
	hdr->isNumeric = mxIsNumeric(mxInput);
	hdr->isSparse = mxIsSparse(mxInput);
	hdr->complexity = mxIsComplex(mxInput)? mxCOMPLEX : mxREAL;
	hdr->classid = mxGetClassID(mxInput);
	hdr->nDims = mxGetNumberOfDimensions(mxInput);
	hdr->elemsiz = mxGetElementSize(mxInput);
	hdr->nzmax = mxGetNumberOfElements(mxInput);      /* update this later on sparse*/
	hdr->nFields = 0;                                 /* update this later */
	hdr->shmsiz = pad_to_align(sizeof(header_t));     /* update this later */
	hdr->strBytes = 0;							/* update this later */
	
	/* copy the size */
	dat->pSize = (mwSize*) mxGetDimensions(mxInput);     /* some abuse of const for now, fix on deep copy*/
	
	/* Add space for the dimensions */
	hdr->shmsiz += pad_to_align(hdr->nDims * sizeof(mwSize));
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
#ifdef DEBUG
		mexPrintf("deepscan: Debug: found structure.\n");
#endif
		
		/* How many fields to work with? */
		hdr->nFields = mxGetNumberOfFields(mxInput);
		
		/* Find the size required to store the field names */
		hdr->strBytes = (size_t)FieldNamesSize(mxInput);     /* always a multiple of alignment size */
		hdr->shmsiz += pad_to_align(hdr->strBytes);                   /* Add space for the field string */
		
		/* use mxCalloc so mem is freed on error via garbage collection */
		dat->child_hdr = (header_t*) mxCalloc(
				hdr->nzmax * hdr->nFields * (sizeof(header_t) + sizeof(data_t)) + hdr->strBytes,
				1); /* extra space for the field string */
		dat->child_dat = (data_t*) &dat->child_hdr[hdr->nFields * hdr->nzmax];
		dat->field_str = (char*) &dat->child_dat[hdr->nFields * hdr->nzmax];
		
		const char** field_names = (const char**)mxMalloc(hdr->nFields*sizeof(char*));
		
		/* make a record of the field names */
		CopyFieldNames(mxInput, dat->field_str, field_names);
		
		*ret_var = mxCreateStructArray(hdr->nDims, dat->pSize, hdr->nFields, field_names);
		mexMakeArrayPersistent(*ret_var);
		
		/* go through each recursively */
		count = 0;
		for(i = 0; i < hdr->nzmax; i++)                         /* each element */
		{
			for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
			{
				/* call recursivley */
				mxArray* ret_child = mxGetFieldByNumber(*ret_var, i, field_num);
				hdr->shmsiz += deepscan(&(dat->child_hdr[count]), &(dat->child_dat[count]), mxGetFieldByNumber(mxInput, i, field_num), hdr, &ret_child);
				
				count++; /* progress */
#ifdef DEBUG
				mexPrintf("deepscan: Debug: finished element %d field %d.\n",i,field_num);
#endif
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
#ifdef DEBUG
		mexPrintf("deepscan: Debug: found cell.\n");
#endif
		
		/* use mxCalloc so mem is freed on error via garbage collection */
		dat->child_hdr = (header_t*) mxCalloc(hdr->nzmax, sizeof(header_t) + sizeof(data_t));
		dat->child_dat = (data_t*) &dat->child_hdr[hdr->nzmax];
		
		*ret_var = mxCreateCellArray(hdr->nDims, dat->pSize);
		mexMakeArrayPersistent(*ret_var);
		
		/* go through each recursively */
		for(i = 0; i < hdr->nzmax; i++)
		{
			mxArray* ret_child = mxGetCell(*ret_var, i);
			hdr->shmsiz += deepscan(&(dat->child_hdr[i]), &(dat->child_dat[i]), mxGetCell(mxInput, i), hdr, &ret_child);
#ifdef DEBUG
			mexPrintf("deepscan: Debug: finished %d.\n",i);
#endif
		}
		
		/* if its a cell it has to have at least one mom-empty element */
		if(hdr->shmsiz == (1 + hdr->nzmax) * pad_to_align(sizeof(header_t)))
		{
			mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone", "Required (variable) must contain at "
					"least one non-empty item (at all levels).");
		}
	}
	else if(hdr->isNumeric || hdr->classid == mxLOGICAL_CLASS || hdr->classid == mxCHAR_CLASS)  /* a matrix containing data *//* base case */
	{
		dat->pr = mxGetData(mxInput);
		dat->pi = mxGetImagData(mxInput);
		
		if(hdr->isSparse)
		{
			/* len(pr)=nzmax, len(ir)=nzmax, len(jc)=colc+1 */
			hdr->nzmax = (size_t)mxGetNzmax(mxInput);
			dat->ir = mxGetIr(mxInput);
			dat->jc = mxGetJc(mxInput);
			hdr->shmsiz += pad_to_align(
					sizeof(mwIndex) * (hdr->nzmax));      /* ensure both pointers are aligned individually */
			hdr->shmsiz += pad_to_align(sizeof(mwIndex) * (dat->pSize[1] + 1));
			
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateSparse(mxGetM(mxInput), mxGetN(mxInput), 1, hdr->complexity);
			}
			else
			{
				*ret_var = mxCreateSparseLogicalMatrix(mxGetM(mxInput), mxGetN(mxInput), 1);
			}
			
			mwIndex* ret_jc = mxGetJc(*ret_var);
			memcpy(ret_jc, dat->jc, hdr->nzmax * sizeof(mwIndex));
			ret_jc[mxGetN(mxInput)] = 0;
			
		}
		else
		{
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateNumericArray(0, nullptr, hdr->classid, hdr->complexity);
				//mxSetDimensions(*ret_var, dat->pSize, hdr->nDims);
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateLogicalArray(0, nullptr);
			}
			else
			{
				*ret_var = mxCreateCharArray(0, nullptr);
			}
		}
		mexMakeArrayPersistent(*ret_var);
		
		/* ensure both pointers are aligned individually */
		hdr->shmsiz += pad_to_align(MXMALLOC_SIG_LEN);			//this is 32 bytes
		hdr->shmsiz += pad_to_align(hdr->elemsiz * hdr->nzmax);
		if(hdr->complexity == mxCOMPLEX)
		{
			hdr->shmsiz += pad_to_align(MXMALLOC_SIG_LEN);			//this is 32 bytes
			hdr->shmsiz += pad_to_align(hdr->elemsiz * hdr->nzmax);
		}
	}
	else
	{
		mexPrintf("Unknown class found: %s\n", mxGetClassName(mxInput));
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:InvalidInput", "clone: unsupported type.");
	}
	
	
	return hdr->shmsiz;
}


/* ------------------------------------------------------------------------- */
/* deepcopy                                                                  */
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
void deepcopy(header_t* hdr, data_t* dat, byte_t* shm, header_t* par_hdr)
{
	
	header_t* cpy_hdr;          /* the header in its copied location */
	size_t i, n, offset = 0;
	mwSize* pSize;               /* points to the size data */
	
	/* for structures */
	int ret;
	
	/* load up the shared memory */
	
	/* copy the header */
	n = sizeof(header_t);
	memcpy(shm, hdr, n);
	offset = pad_to_align(n);
	
	/* copy the dimensions */
	cpy_hdr = (header_t*) shm;
	pSize = (mwSize*) &shm[offset];
	for(i = 0; i < cpy_hdr->nDims; i++)
	{
		pSize[i] = dat->pSize[i];
	}
	offset += pad_to_align(cpy_hdr->nDims * sizeof(mwSize));
	
	/* assign the parent header  */
	cpy_hdr->par_hdr_off = (nullptr == par_hdr) ? 0 : (int) (par_hdr - cpy_hdr);
	shm += offset;
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
#ifdef DEBUG
		mexPrintf("deepcopy: Debug: found structure.\n");
#endif
		
		/* place the field names next in shared memory */
		
		/* copy the field string */
		for(i = 0; i < hdr->strBytes; i++)
		{
			shm[i] = dat->field_str[i];
		}
		shm += pad_to_align(hdr->strBytes);
		
		/* copy the children recursively */
		for(i = 0; i < hdr->nzmax * hdr->nFields; i++)
		{
			deepcopy(&(dat->child_hdr[i]), &(dat->child_dat[i]), shm, cpy_hdr);
			shm += (dat->child_hdr[i]).shmsiz;
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		/* recurse for each cell element */
#ifdef DEBUG
		mexPrintf("deepcopy: Debug: found cell.\n");
#endif
		
		/* recurse through each cell */
		for(i = 0; i < hdr->nzmax; i++)
		{
			deepcopy(&(dat->child_hdr[i]), &(dat->child_dat[i]), shm, cpy_hdr);
			shm += (dat->child_hdr[i]).shmsiz;
		}
	}
	else /* base case */
	{
		/* copy real data */
		memcpy(shm + (pad_to_align(MXMALLOC_SIG_LEN) - MXMALLOC_SIG_LEN), MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
		shm += pad_to_align(MXMALLOC_SIG_LEN);
		n = (hdr->nzmax) * (hdr->elemsiz);
		memcpy(shm, dat->pr, n);
		shm += pad_to_align(n);
		
		/* copy complex data as well */
		if(hdr->complexity == mxCOMPLEX)
		{
			memcpy(shm + (pad_to_align(MXMALLOC_SIG_LEN) - MXMALLOC_SIG_LEN), MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			n = (hdr->nzmax) * (hdr->elemsiz);
			memcpy(shm, dat->pi, n);
			shm += pad_to_align(n);
		}
		
		/* and the indices of the sparse data as well */
		if(hdr->isSparse)
		{
			n = hdr->nzmax * sizeof(mwIndex);
			memcpy(shm, dat->ir, n);
			shm += pad_to_align(n);
			
			n = (pSize[1] + 1) * sizeof(mwIndex);
			memcpy(shm, dat->jc, n);
			shm += pad_to_align(n);
		}
	}
}


/* ------------------------------------------------------------------------- */
/* deepfree                                                                  */
/*                                                                           */
/* Descend through header and data structure and free the memory.            */
/*                                                                           */
/* Arguments:                                                                */
/*    data container                                                         */
/* Returns:                                                                  */
/*    void                                                                   */
/* ------------------------------------------------------------------------- */
void deepfree(data_t* dat)
{
	
	/* recurse to the bottom */
	if(dat->child_dat)
	{
		deepfree(dat->child_dat);
	}
	
	/* free on the way back up */
	mxFree(dat->child_hdr);
	dat->child_hdr = (header_t*) nullptr;
	dat->child_dat = (data_t*) nullptr;
	dat->field_str = (char*) nullptr;
}


mxLogical deepcompare(byte_t* shm, const mxArray* comp_var, size_t* offset)
{
	/* for working with shared memory ... */
	size_t i;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* pr = nullptr, * pi = nullptr;  /* real and imaginary data pointers */
	mwIndex* ir = nullptr, * jc = nullptr;  /* sparse matrix data indices */
	
	
	/* for structures */
	size_t field_num, shmshift;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*) shm;
	shm += pad_to_align(sizeof(header_t));
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return false;
	}
	
	/* the size pointer */
	pSize = (mwSize*) shm;
	if(hdr->nDims != mxGetNumberOfDimensions(comp_var)
	   		|| memcmp(pSize, mxGetDimensions(comp_var), sizeof(mwSize) * hdr->nDims) != 0
			|| hdr->nzmax != mxGetNumberOfElements(comp_var))
	{
		return false;
	}
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize) * hdr->nDims);
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		char** field_names = (char**)mxCalloc((size_t)hdr->nFields, sizeof(char*));
		PointCharArrayAtString(field_names, shm, hdr->nFields);
		
		/* skip over the stored field string */
		shm += pad_to_align(hdr->strBytes);
		
		/* Go through each element */
		for(i = 0; i < hdr->nzmax; i++)
		{
			for(field_num = 0, shmshift = 0; field_num < hdr->nFields; field_num++, shmshift = 0)     /* each field */
			{
				if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_names[field_num]) != 0)
				{
					return false;
				}
				
				
				if(!deepcompare(shm, mxGetFieldByNumber(comp_var, i, field_num), &shmshift))
				{
					return false;
				}
				else
				{
					shm += shmshift;
				}
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		for(i = 0, shmshift = 0; i < hdr->nzmax; i++, shmshift = 0)
		{
			if(!deepcompare(shm, mxGetCell(comp_var,i), &shmshift))
			{
				return false;
			}
			else
			{
				shm += shmshift;
			}
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		shm += pad_to_align(MXMALLOC_SIG_LEN);
		pr = (void*) shm;
		shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));          /* takes us to the end of the real data */
		
		//these should point to the same location
		if(pr != mxGetData(comp_var))
		{
			return false;
		}
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			pi = (void*) shm;
			shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));     /* takes us to the end of the complex data */
			
			//these should point to the same location
			if(pi != mxGetImagData(comp_var))
			{
				return false;
			}
			
		}
		
		if(hdr->isSparse)
		{
			if(!mxIsSparse(comp_var))
			{
				return false;
			}
			
			ir = (mwIndex*) shm;
			shm += pad_to_align((hdr->nzmax) * sizeof(mwIndex));
			
			jc = (mwIndex*) shm;
			shm += pad_to_align((pSize[1] + 1) * sizeof(mwIndex));
			
			if(memcmp(ir, mxGetIr(comp_var), (hdr->nzmax) * sizeof(mwIndex)) != 0
				|| memcmp(jc, mxGetJc(comp_var), (pSize[1] + 1) * sizeof(mwIndex)))
			{
				return false;
			}
			
		}
		
	}
	*offset = hdr->shmsiz;
}


/* Function to find the number of bytes required to store all of the */
/* field names of a structure									     */
int FieldNamesSize(const mxArray* mxStruct)
{
	const char* pFieldName;
	int nFields;
	int i, j;               /* counters */
	int Bytes = 0;
	
	/* How many fields? */
	nFields = mxGetNumberOfFields(mxStruct);
	
	/* Go through them */
	for(i = 0; i < nFields; i++)
	{
		/* This field */
		pFieldName = mxGetFieldNameByNumber(mxStruct, i);
		j = 0;
		while(pFieldName[j])
		{
			j++;
		}
		j++;          /* for the null termination */
		
		if(i == (nFields - 1))
		{
			j++;
		}     /* add this at the end for an end of string sequence since we use 0 elsewhere (term_char). */
		
		
		/* Pad it out to 8 bytes */
		j = pad_to_align(j);
		
		/* keep the running tally */
		Bytes += j;
		
	}
	
	/* Add an extra alignment size to store the length of the string (in Bytes) */
	Bytes += align_size;
	
	return Bytes;
	
}


/* Function to copy all of the field names to a character array    */
/* Use FieldNamesSize() to allocate the required size of the array */
/* returns the number of bytes used in pList					   */
int CopyFieldNames(const mxArray* mxStruct, char* pList, const char** field_names)
{
	const char* pFieldName;    /* name of the field to add to the list */
	int nFields;                    /* the number of fields */
	int i, j;                         /* counters */
	int Bytes = 0;                    /* number of bytes copied */
	
	/* How many fields? */
	nFields = mxGetNumberOfFields(mxStruct);
	
	/* Go through them */
	for(i = 0; i < nFields; i++)
	{
		/* This field */
		pFieldName = mxGetFieldNameByNumber(mxStruct, i);
		field_names[i] = pFieldName;
		j = 0;
		while(pFieldName[j])
		{
			pList[Bytes++] = pFieldName[j++];
		}
		pList[Bytes++] = 0; /* add the null termination */
		
		/* if this is last entry indicate the end of the string */
		if(i == (nFields - 1))
		{
			pList[Bytes++] = term_char;
			pList[Bytes++] = 0;  /* another null termination just to be safe */
		}
		
		/* Pad it out to 8 bytes */
		while(Bytes % align_size)
		{
			pList[Bytes++] = 0;
		}
	}
	
	/* Add an extra alignment size to store the length of the string (in Bytes) */
	*(unsigned int*) &pList[Bytes] = (unsigned int) (Bytes + align_size);
	Bytes += align_size;
	
	return Bytes;
	
}

/*Function to take point a each element of the char** array at a list of names contained in string */
/*ppCharArray must be allocated to length num_names */
/*names are seperated by null termination characters */
/*each name must start on an aligned address (see CopyFieldNames()) */
/*e.g. pCharArray[0] = &name_1, pCharArray[1] = &name_2 ...         */
/*returns 0 if successful */
int PointCharArrayAtString(char** pCharArray, char* pString, int nFields)
{
	int i = 0;                             /* the index in pString we are up to */
	int field_num = 0;                   /* the field we are up to */
	
	/* and recover them */
	while(field_num < nFields)
	{
		/* scan past null termination chars */
		while(!pString[i])
		{
			i++;
		}
		
		/* Check the address is aligned (assume every forth bytes is aligned) */
		if(i % align_size)
		{
			return -1;
		}
		
		/* grab the address */
		pCharArray[field_num] = &pString[i];
		field_num++;
		
		/* continue until a null termination char */
		while(pString[i])
		{
			i++;
		}
		
	}
	return 0;
}

/* This function finds the number of fields contained within in a string */
/* the string is terminated by term_char, a character that can't be in a field name */
/* returns the number of field names found */
/* returns -1 if unaligned field names are found */
int NumFieldsFromString(const char* pString, size_t* pFields, size_t* pBytes)
{
	unsigned int stored_length;          /* the recorded length of the string */
	bool term_found = false;          /* has the termination character been found? */
	int i = 0;                              /* counter */
	
	pFields[0] = 0;                         /* the number of fields in the string */
	pBytes[0] = 0;
	
	
	/* and recover them */
	while(!term_found)
	{
		/* scan past null termination chars */
		while(!pString[i])
		{
			i++;
		}
		
		/* is this the special termination character? */
		term_found = pString[i] == term_char;
		
		if(!term_found)
		{
			/* Check the address is aligned (assume every forth bytes is aligned) */
			if(i % align_size)
			{
				return -1;
			}
			
			/* a new field */
			pFields[0]++;
			
			/* continue until a null termination char */
			while(pString[i] && (pString[i] != term_char))
			{
				i++;
			}
			
			/* check there wasn't the terminal character immediately after the field */
			if(pString[i] == term_char)
			{
				return -2;
			}
			
		}
	}
	pBytes[0] = i;
	
	/* return the aligned size */
	pBytes[0] = pad_to_align(pBytes[0]);
	
	/* Check what's recovered matches the stored length */
	stored_length = *(unsigned int*) &pString[pBytes[0]];       /* the recorded length of the string */
	pBytes[0] += align_size;                                          /* add the extra space for the record */
	
	/* Check on it */
	if(stored_length != pBytes[0])
	{
		return -3;
	}
	
	return 0;
	
}

/* Function to find the bytes in the string starting from the end of the string (i.e. the last element is pString[-1])*/
/* returns < 0 on error */
int BytesFromStringEnd(const char* pString, size_t* pBytes)
{
	
	int offset = align_size;            /* start from the end of the string */
	pBytes[0] = 0;                           /* default */
	
	/* Grab it directly from the string */
	pBytes[0] = (size_t) (*(unsigned int*) &pString[-offset]);
	
	/* scan for the termination character */
	while((offset < 2 * align_size) && (pString[-offset] != term_char))
	{
		offset++;
	}
	
	if(offset == align_size)
	{
		return -1;
	}/* failure to find the control character */
	else
	{
		return 0;
	}/* happy */
	
}


void init()
{
	
	current_segment_p = (byte_t**)mxMalloc(sizeof(byte_t*));
	mexMakeMemoryPersistent(current_segment_p);
	
	current_segment_info = (segment_info*)mxMalloc(sizeof(segment_info));
	mexMakeMemoryPersistent(current_segment_info);
	
	shm_name_handle = (HANDLE*)mxMalloc(sizeof(HANDLE));
	mexMakeMemoryPersistent(shm_name_handle);
	
	*shm_name_handle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(segment_info), MSH_NAME_SEGMENT_NAME);
	DWORD err = GetLastError();
	if(*shm_name_handle == nullptr)
	{
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
					   "SharedMemory::Could not create the memory segment");
	}
	
	segment_info* seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
	if(seg_info == nullptr)
	{
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
					   "SharedMemory::Could not map the name memory segment");
	}
	else if(err != ERROR_ALREADY_EXISTS)
	{
		seg_info->segment_number = 0;
		seg_info->segment_size = NULLSEGMENT_SZ;
	}
	
	shm_handle = (HANDLE*)mxMalloc(sizeof(HANDLE));
	mexMakeMemoryPersistent(shm_handle);
	
	memcpy(MSH_SEGMENT_NAME + MSH_SEG_NAME_PREAMB_LEN - 1, &seg_info->segment_number, sizeof(uint32_t));
	*shm_handle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, NULLSEGMENT_SZ, MSH_SEGMENT_NAME);
	err = GetLastError();
	if(*shm_handle == nullptr)
	{
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
					   "SharedMemory::Could not create the memory segment");
	}
	*current_segment_p = (byte_t*)MapViewOfFile(*shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, seg_info->segment_size);
	
	current_segment_info->segment_number = 0;
	current_segment_info->segment_size = NULLSEGMENT_SZ;
	UnmapViewOfFile(seg_info);
	
	global_shared_variable = mxCreateNumericArray(0, nullptr, mxUINT8_CLASS, mxREAL);
	mexMakeArrayPersistent(global_shared_variable);
	mexAtExit(onExit);
}


void onExit()
{
	if(!mxIsEmpty(global_shared_variable))
	{
		/* NULL all of the Matlab pointers */
		deepdetach(global_shared_variable);
	}
	else
	{
		mxDestroyArray(global_shared_variable);
	}
	
	UnmapViewOfFile(*current_segment_p);
	CloseHandle(*shm_handle);
	CloseHandle(*shm_name_handle);
	mxFree(shm_handle);
	mxFree(shm_name_handle);
	mxFree(current_segment_info);
	mxFree(current_segment_p);
	
	//mexUnlock();
	
}



/*
 * Possibly useful information/related work:
 *
 * http://www.mathworks.com/matlabcentral/fileexchange/24576
 * http://www.mathworks.in/matlabcentral/newsreader/view_thread/254813
 * http://www.mathworks.com/matlabcentral/newsreader/view_thread/247881#639280
 *
 * http://groups.google.com/group/comp.soft-sys.matlab/browse_thread/thread/c241d8821fb90275/47189d498d1f45b8?lnk=st&q=&rnum=1&hl=en#47189d498d1f45b8
 * http://www.mk.tu-berlin.de/Members/Benjamin/mex_sharedArrays
 */
