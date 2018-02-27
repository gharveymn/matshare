#include "headers/matshare_.h"

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{

	/* For inputs */
	const mxArray* mxDirective;               /*  Directive {clone, attach, detach, free}   */
	const mxArray* mxInput;                /*  Input array (for clone)					  */
	
	/* For storing inputs */
	msh_directive_t directive;
	
	size_t sm_size;                              /* size required by the shared memory */
	
	/* for storing the mxArrays ... */
	header_t hdr;
	data_t dat;
	
	mxLogical* ret_data = NULL;
	
	segment_info* seg_info;
	char* msh_segment_name;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		mexErrMsgIdAndTxt("MATLAB:MatShare", "Minimum input arguments missing; must supply directive and key.");
	}
	
	/* assign inputs */
	mxDirective = prhs[0];
	
	/* get directive */
	directive = (msh_directive_t)*((uint8_t*)(mxGetData(mxDirective)));
	DWORD hi_sz, lo_sz, err;
	mxArrayStruct* arr;
	
	HANDLE temp_handle;
	
	/* Switch yard {clone, attach, detach, free} */
	switch(directive)
	{
		case msh_INIT:
			init();
			mexMakeArrayPersistent(global_shared_variable);
			plhs[0] = mxCreateSharedDataCopy(global_shared_variable);
			mexLock();
			break;
		case msh_CLONE:
			/********************/
			/*	Clone case		*/
			/********************/
			
			/* check the inputs */
			if(nrhs < 2)
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone",
							   "Required second argument is missing (???)");
			}
			
			acquireProcLock();
			
			/* Assign */
			mxInput = prhs[1];
			
			/* scan input data */
			sm_size = deepscan(&hdr, &dat, mxInput, NULL, &global_shared_variable);
			
			// get current map number
			seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
			seg_info->segment_number = seg_info->segment_number == UINT32_MAX? 0 : seg_info->segment_number + 1; // there aren't going to be UINT32_MAX files open, so reset to 0 without checking
			current_segment_info->segment_number = seg_info->segment_number;
			seg_info->segment_size = sm_size;
			current_segment_info->segment_size = sm_size;
			
			msh_segment_name = mxMalloc((MSH_SEG_NAME_LEN + 1)*sizeof(char));
			sprintf(msh_segment_name, MSH_SEGMENT_NAME, seg_info->segment_number);
			
			UnmapViewOfFile(*current_segment_p);
			CloseHandle(*shm_handle);
			
			// create the new mapping
			lo_sz =  (DWORD)(sm_size & 0xFFFFFFFFL);
			hi_sz =  (DWORD)((sm_size >> 32) & 0xFFFFFFFFL);
			temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, msh_segment_name);
			err = GetLastError();
			if(temp_handle == NULL)
			{
				// throw error is already exists because that shouldn't happen
				releaseProcLock();
				mexPrintf("Error number: %d\n", err);
				mexErrMsgIdAndTxt("MATLAB:MatShare:init",
							   "MatShare::Could not create the memory segment");
			}
			
			if(err == ERROR_ALREADY_EXISTS)
			{
				DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), shm_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
			}
			else
			{
				*shm_handle = temp_handle;
			}
			
			*current_segment_p = (byte_t*)MapViewOfFile(*shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, sm_size);
			
			/* copy data to the shared memory */
			deepcopy(&hdr, &dat, *current_segment_p, NULL);
			
			/* free temporary allocation */
			deepfree(&dat);
			
			mxFree(msh_segment_name);
			mexMakeArrayPersistent(global_shared_variable);
			plhs[0] = mxCreateSharedDataCopy(global_shared_variable);
			UnmapViewOfFile(seg_info);
			mexLock();
			releaseProcLock();
			break;
		case msh_ATTACH:
			/********************/
			/*	Attach case	*/
			/********************/
			
			acquireProcLock();

			// get current map size
			seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
			current_segment_info->segment_number = seg_info->segment_number;
			current_segment_info->segment_size = seg_info->segment_size;
			
			/* Restore the segment if this an initialized region */
			shallowrestore(*current_segment_p, global_shared_variable);
			
			releaseProcLock();
			break;
		case msh_DETACH:
			/********************/
			/*	Dettach case	*/
			/********************/
			
			acquireProcLock();
			
			if(!mxIsEmpty(global_shared_variable))
			{
				/* NULL all of the Matlab pointers */
				deepdetach(global_shared_variable);
			}
			mxDestroyArray(global_shared_variable);
			
			global_shared_variable = mxCreateDoubleMatrix(0, 0, mxREAL);
			mexMakeArrayPersistent(global_shared_variable);
			plhs[0] = mxCreateSharedDataCopy(global_shared_variable);
			mexUnlock();
			releaseProcLock();
			break;
		case msh_FREE:
			/********************/
			/*	free case		*/
			/********************/
			//CloseHandle(*shm_handle);
			mexUnlock();
			break;
		case msh_FETCH:
			
			acquireProcLock();
			
			plhs[1] = mxCreateLogicalMatrix(1,1);
			ret_data = mxGetLogicals(plhs[1]);
			
			seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
			*ret_data = (mxLogical)(current_segment_info->segment_number != seg_info->segment_number);
			if(*ret_data != TRUE)
			{
				//return a shallow copy of the variable
				UnmapViewOfFile(seg_info);
				plhs[0] = mxCreateSharedDataCopy(global_shared_variable);
				releaseProcLock();
				break;
			}
			else
			{
				//do a detach operation
				if(!mxIsEmpty(global_shared_variable))
				{
					/* NULL all of the Matlab pointers */
					deepdetach(global_shared_variable);
				}
				mxDestroyArray(global_shared_variable);
				
				current_segment_info->segment_number = seg_info->segment_number;
				current_segment_info->segment_size = seg_info->segment_size;
				
				msh_segment_name = mxMalloc((MSH_SEG_NAME_LEN + 1) * sizeof(char));
				sprintf(msh_segment_name, MSH_SEGMENT_NAME, current_segment_info->segment_number);
				
				UnmapViewOfFile(*current_segment_p);
				CloseHandle(*shm_handle);
				
					temp_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, msh_segment_name);
					err = GetLastError();
					if(temp_handle == NULL)
					{
						releaseProcLock();
						mexPrintf("Error number: %d\n", err);
						mexErrMsgIdAndTxt("MATLAB:MatShare:fetch",
									   "MatShare::Could not open the memory segment");
					}
					
					DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), shm_handle, 0, TRUE,
								 DUPLICATE_SAME_ACCESS);
					
					*current_segment_p = (byte_t*) MapViewOfFile(*shm_handle, FILE_MAP_ALL_ACCESS, 0, 0,
														current_segment_info->segment_size);
					err = GetLastError();
					if(*current_segment_p == NULL)
					{
						releaseProcLock();
						mexPrintf("Error number: %d\n", err);
						mexErrMsgIdAndTxt("MATLAB:MatShare:fetch",
									   "MatShare::Could not fetch the memory segment");
					}
					shallowfetch(*current_segment_p, &global_shared_variable);
				
				mexMakeArrayPersistent(global_shared_variable);
				plhs[0] = mxCreateSharedDataCopy(global_shared_variable);
				mxFree(msh_segment_name);
				UnmapViewOfFile(seg_info);
				releaseProcLock();
			}
			break;
		case msh_COMPARE:
			
			acquireProcLock();
			
			plhs[0] = mxCreateLogicalMatrix(1,1);
			ret_data = mxGetLogicals(plhs[0]);
			seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
			*ret_data = (mxLogical)(current_segment_info->segment_number == seg_info->segment_number);
			UnmapViewOfFile(seg_info);
			
			releaseProcLock();
			break;
		case msh_COPY:
			plhs[0] = mxCreateSharedDataCopy(global_shared_variable);
			break;
		case msh_DEEPCOPY:
			plhs[0] = mxDuplicateArray(global_shared_variable);
			break;
		case msh_DEBUG:
			arr = (mxArrayStruct*)global_shared_variable;
			mexPrintf("%d\n", arr->RefCount);
			break;
		default:
			mexErrMsgIdAndTxt("MATLAB:SharedMemory", "Unrecognized directive.");
			/* unrecognised directive */
			break;
	}
	
//	mexPrintf("%s\n", mexIsLocked()? "TRUE": "FALSE" );


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
	size_t i, n;
	
	/*for structure */
	int nFields, j;
	
	/* restore matlab  memory */
	if(ret_var == (mxArray*) NULL || mxIsEmpty(ret_var))
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
				deepdetach(mxGetFieldByNumber(ret_var, i, j));
			}/* detach this one */
		}
	}
	else if(mxIsCell(ret_var))
	{
		/* detach each cell */
		n = mxGetNumberOfElements(ret_var);
		for(i = 0; i < n; i++)
		{
			deepdetach(mxGetCell(ret_var, i));
		}/* detach this one */
		
	}
	else if(mxIsNumeric(ret_var) || mxIsLogical(ret_var) || mxIsChar(ret_var))  /* a matrix containing data */
	{
		
		/* In safe mode these entries were allocated so remove them properly */
		size_t num_dims = 2;
		void* pr,* pi = NULL;
		
		/* handle sparse objects */
		if(mxIsSparse(ret_var))
		{
			/* I don't seem to be able to give sparse arrays zero size so (nzmax must be 1) */
			dims[0] = dims[1] = 1;
			nzmax = 1;
			if(mxSetDimensions(ret_var, dims, num_dims))
			{
				releaseProcLock();
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:Unknown", "detach: unable to resize the array.");
			}
			
			/* allocate 1 element */
			pr = mxMalloc(1);
			mxSetData(ret_var, pr);
			if(mxIsComplex(ret_var))
			{
				pi = mxMalloc(1);
				mxSetImagData(ret_var, pi);
			}
			mxSetNzmax(ret_var, nzmax);
		}
		else
		{
			mxSetDimensions(ret_var, dims, num_dims);
			pr = mxMalloc(0);
			mxSetData(ret_var, pr);
			if(mxIsComplex(ret_var))
			{
				pi = mxMalloc(0);
				mxSetImagData(ret_var, pi);
			}
		}
		
		// reset all the crosslinks so nothing in MATLAB is pointing to shared data (which will be gone soon)
		mxArray* link;
		for(link = ((mxArrayStruct*)ret_var)->CrossLink; link != NULL && link != ret_var; link = ((mxArrayStruct*)link)->CrossLink)
		{
			mxSetDimensions(link, dims, num_dims);
			mxSetData(link, pr);
			if(mxIsComplex(ret_var))
			{
				mxSetImagData(link, pi);
			}
		}
		
	}
	else
	{
		releaseProcLock();
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
	void* pr = NULL, * pi = NULL;  /* real and imaginary data pointers */
	mwIndex* ir = NULL, * jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*) shm;
	shm += pad_to_align(sizeof(header_t));
	
	/* don't do anything if this is an empty matrix */
	if(hdr->isEmpty)
	{
		return hdr->shmsiz;
	}
	
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
			shm += shallowrestore(shm, mxGetCell(ret_var, i));
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
		
		/* set the real and imaginary data */
		mxFree(mxGetData(ret_var));
		mxSetData(ret_var, pr);
		if ( hdr->complexity )
		{
			mxFree(mxGetImagData(ret_var));
			mxSetImagData(ret_var, pi);
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
		
	
	}
	return hdr->shmsiz;
}


size_t shallowfetch(byte_t* shm, mxArray** ret_var)
{
	/* for working with shared memory ... */
	size_t i;
	mxArray* ret_child;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* pr = NULL, * pi = NULL;  /* real and imaginary data pointers */
	mwIndex* ir = NULL, * jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
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
		if(hdr->isEmpty)
		{
			/* make this a separate case since there are no field names */
			*ret_var = mxCreateStructArray(hdr->nDims, pSize, hdr->nFields, NULL);
		}
		else
		{
			char** field_names = (char**) mxCalloc((size_t) hdr->nFields, sizeof(char*));
			PointCharArrayAtString(field_names, shm, hdr->nFields);
			
			/* skip over the stored field string */
			shm += pad_to_align(hdr->strBytes);
			
			*ret_var = mxCreateStructArray(hdr->nDims, pSize, hdr->nFields, (const char**) field_names);
			
			/* Go through each element */
			for(i = 0; i < hdr->nzmax; i++)
			{
				for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
				{
					shm += shallowfetch(shm, &ret_child);
					mxSetFieldByNumber(*ret_var, i, field_num, ret_child);
				}
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		*ret_var = mxCreateCellArray(hdr->nDims, pSize);
		for(i = 0; i < hdr->nzmax; i++)
		{
			shm += shallowfetch(shm, &ret_child);
			mxSetCell(*ret_var, i, ret_child);
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
				if(hdr->isEmpty)
				{
					*ret_var = mxCreateNumericArray(hdr->nDims, pSize, hdr->classid, hdr->complexity);
				}
				else
				{
					*ret_var = mxCreateNumericArray(0, NULL, hdr->classid, hdr->complexity);
				}
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				if(hdr->isEmpty)
				{
					*ret_var = mxCreateLogicalArray(hdr->nDims, pSize);
				}
				else
				{
					*ret_var = mxCreateLogicalArray(0, NULL);
				}
			}
			else
			{
				if(hdr->isEmpty)
				{
					*ret_var = mxCreateCharArray(hdr->nDims, pSize);
				}
				else
				{
					*ret_var = mxCreateCharArray(0, NULL);
				}
			}
		}
		
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
	size_t i, count;
	mxArray* ret_child;
	
	/* for structures */
	int field_num;
	
	/* initialize data info; _possibly_ update these later */
	dat->pSize = (mwSize*) NULL;
	dat->pr = (void*) NULL;
	dat->pi = (void*) NULL;
	dat->ir = (mwIndex*) NULL;
	dat->jc = (mwIndex*) NULL;
	dat->child_dat = (data_t*) NULL;
	dat->child_hdr = (header_t*) NULL;
	dat->field_str = (char*) NULL;
	hdr->par_hdr_off = 0;               /*don't record it here */
	
	if(mxInput == NULL)
	{
		releaseProcLock();
		mexErrMsgIdAndTxt("MATLAB:MatShare:clone", "Input variable was unexpectedly NULL");
	}
	
	/* initialize header info */
	hdr->isNumeric = mxIsNumeric(mxInput);
	hdr->isSparse = mxIsSparse(mxInput);
	hdr->isEmpty = mxIsEmpty(mxInput);
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
		
		if(hdr->isEmpty)
		{
			if(hdr->nFields)
			{
				releaseProcLock();
				mexErrMsgIdAndTxt("MATLAB:MatShare:clone", "An empty struct array unexpectedly had more than one field");
			}
			*ret_var = mxCreateStructArray(hdr->nDims, dat->pSize, hdr->nFields, NULL);
		}
		else
		{
			
			/* How many fields to work with? */
			hdr->nFields = mxGetNumberOfFields(mxInput);
			
			/* Find the size required to store the field names */
			hdr->strBytes = (size_t) FieldNamesSize(mxInput);     /* always a multiple of alignment size */
			hdr->shmsiz += pad_to_align(hdr->strBytes);                   /* Add space for the field string */
			
			/* use mxCalloc so mem is freed on error via garbage collection */
			dat->child_hdr = (header_t*) mxCalloc(
					hdr->nzmax * hdr->nFields * (sizeof(header_t) + sizeof(data_t)) + hdr->strBytes,
					1); /* extra space for the field string */
			dat->child_dat = (data_t*) &dat->child_hdr[hdr->nFields * hdr->nzmax];
			dat->field_str = (char*) &dat->child_dat[hdr->nFields * hdr->nzmax];
			
			const char** field_names = (const char**) mxMalloc(hdr->nFields * sizeof(char*));
			
			/* make a record of the field names */
			CopyFieldNames(mxInput, dat->field_str, field_names);
			
			*ret_var = mxCreateStructArray(hdr->nDims, dat->pSize, hdr->nFields, field_names);
			
			/* go through each recursively */
			count = 0;
			for(i = 0; i < hdr->nzmax; i++)                         /* each element */
			{
				for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
				{
					/* call recursivley */
					hdr->shmsiz += deepscan(&(dat->child_hdr[count]), &(dat->child_dat[count]),
									    mxGetFieldByNumber(mxInput, i, field_num), hdr, &ret_child);
					mxSetFieldByNumber(*ret_var, i, field_num, ret_child);
					
					count++; /* progress */
				}
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{

		
		if(hdr->isEmpty)
		{
			*ret_var = mxCreateCellArray(hdr->nDims, dat->pSize);
		}
		else
		{
			
			/* use mxCalloc so mem is freed on error via garbage collection */
			dat->child_hdr = (header_t*) mxCalloc(hdr->nzmax, sizeof(header_t) + sizeof(data_t));
			dat->child_dat = (data_t*) &dat->child_hdr[hdr->nzmax];
			
			*ret_var = mxCreateCellArray(hdr->nDims, dat->pSize);
			
			/* go through each recursively */
			for(i = 0; i < hdr->nzmax; i++)
			{
				hdr->shmsiz += deepscan(&(dat->child_hdr[i]), &(dat->child_dat[i]), mxGetCell(mxInput, i), hdr,
								    &ret_child);
				mxSetCell(*ret_var, i, ret_child);
			}
			
			/* if its a cell it has to have at least one mom-empty element */
			if(hdr->shmsiz == (1 + hdr->nzmax) * pad_to_align(sizeof(header_t)))
			{
				releaseProcLock();
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone", "Required (variable) must contain at "
						"least one non-empty item (at all levels).");
			}
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
				if(hdr->isEmpty)
				{
					*ret_var = mxCreateNumericArray(hdr->nDims, dat->pSize, hdr->classid, hdr->complexity);
				}
				else
				{
					*ret_var = mxCreateNumericArray(0, NULL, hdr->classid, hdr->complexity);
				}
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				if(hdr->isEmpty)
				{
					*ret_var = mxCreateLogicalArray(hdr->nDims, dat->pSize);
				}
				else
				{
					*ret_var = mxCreateLogicalArray(0, NULL);
				}
			}
			else
			{
				if(hdr->isEmpty)
				{
					*ret_var = mxCreateCharArray(hdr->nDims, dat->pSize);
				}
				else
				{
					*ret_var = mxCreateCharArray(0, NULL);
				}
			}
		}
		
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
		releaseProcLock();
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
	cpy_hdr->par_hdr_off = (NULL == par_hdr) ? 0 : (int) (par_hdr - cpy_hdr);
	shm += offset;
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
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
	dat->child_hdr = (header_t*) NULL;
	dat->child_dat = (data_t*) NULL;
	dat->field_str = (char*) NULL;
}


mxLogical deepcompare(byte_t* shm, const mxArray* comp_var, size_t* offset)
{
	/* for working with shared memory ... */
	size_t i, shmshift;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* pr = NULL, * pi = NULL;  /* real and imaginary data pointers */
	mwIndex* ir = NULL, * jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
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
				|| memcmp(jc, mxGetJc(comp_var), (pSize[1] + 1) * sizeof(mwIndex)) != 0)
			{
				return false;
			}
			
		}
		
	}
	*offset = hdr->shmsiz;
	return true;
}


/* Function to find the number of bytes required to store all of the */
/* field names of a structure									     */
size_t FieldNamesSize(const mxArray* mxStruct)
{
	const char* pFieldName;
	int nFields;
	int i, j;               /* counters */
	size_t Bytes = 0;
	
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
		j = (int)pad_to_align((size_t)j);
		
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
size_t CopyFieldNames(const mxArray* mxStruct, char* pList, const char** field_names)
{
	const char* pFieldName;    /* name of the field to add to the list */
	int nFields;                    /* the number of fields */
	int i, j;                         /* counters */
	size_t Bytes = 0;                    /* number of bytes copied */
	
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
	bool_t term_found = false;          /* has the termination character been found? */
	size_t i = 0;                              /* counter */
	
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
		term_found = (bool_t)(pString[i] == term_char);
		
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
	
	int offset = (int) align_size;            /* start from the end of the string */
	pBytes[0] = 0;                           /* default */
	
	/* Grab it directly from the string */
	pBytes[0] = (*(size_t*) &pString[-offset]);
	
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
	DWORD err;
	HANDLE temp_handle;
	
	current_segment_p = (byte_t**)mxMalloc(sizeof(byte_t*));
	mexMakeMemoryPersistent(current_segment_p);
	
	current_segment_info = (segment_info*)mxMalloc(sizeof(segment_info));
	mexMakeMemoryPersistent(current_segment_info);
	
	shm_name_handle = (HANDLE*)mxMalloc(sizeof(HANDLE));
	mexMakeMemoryPersistent(shm_name_handle);
	
	shm_handle = (HANDLE*)mxMalloc(sizeof(HANDLE));
	mexMakeMemoryPersistent(shm_handle);
	
	proc_lock = (HANDLE*)mxMalloc(sizeof(HANDLE));
	mexMakeMemoryPersistent(proc_lock);
	
	lock_sec = (SECURITY_ATTRIBUTES*)mxMalloc(sizeof(SECURITY_ATTRIBUTES));
	mexMakeMemoryPersistent(lock_sec);
	lock_sec->nLength = sizeof(SECURITY_ATTRIBUTES);
	lock_sec->lpSecurityDescriptor = NULL;
	lock_sec->bInheritHandle = TRUE;
	
	temp_handle = CreateMutex(lock_sec, FALSE, MSH_LOCK_NAME);
	err = GetLastError();
	if(temp_handle == NULL)
	{
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
					   "SharedMemory::Could not create the memory segment");
	}
	else if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), proc_lock, 0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	else
	{
		*proc_lock = temp_handle;
	}
	
	
	acquireProcLock();
	
	temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(segment_info), MSH_NAME_SEGMENT_NAME);
	err = GetLastError();
	if(temp_handle == NULL)
	{
		releaseProcLock();
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
					   "SharedMemory::Could not create the memory segment");
	}
	
	if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), shm_name_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	else
	{
		*shm_name_handle = temp_handle;
	}
	
	bool copy_info = FALSE;
	header_t hdr;
	segment_info* seg_info = (segment_info*)MapViewOfFile(*shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(segment_info));
	if(seg_info == NULL)
	{
		releaseProcLock();
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
					   "SharedMemory::Could not map the name memory segment");
		exit(1);
	}
	else if(err != ERROR_ALREADY_EXISTS)
	{
		copy_info = TRUE;
		/* this is the first region created */
		hdr.isNumeric = 1;
		hdr.isSparse = 0;
		hdr.isEmpty = 1;
		hdr.complexity = mxREAL;
		hdr.classid = mxDOUBLE_CLASS;
		hdr.nDims = 2;
		hdr.elemsiz = sizeof(mxDouble);
		hdr.nzmax = 0;      /* update this later on sparse*/
		hdr.nFields = 0;                                 /* update this later */
		hdr.shmsiz = pad_to_align(sizeof(header_t));     /* update this later */
		hdr.strBytes = 0;
		hdr.par_hdr_off = 0;
		
		seg_info->segment_number = 0;
		seg_info->segment_size = hdr.shmsiz;
	}
	
	char* msh_segment_name = mxMalloc((MSH_SEG_NAME_LEN + 1)*sizeof(char));
	sprintf(msh_segment_name, MSH_SEGMENT_NAME, seg_info->segment_number);
	
	current_segment_info->segment_number = seg_info->segment_number;
	current_segment_info->segment_size = seg_info->segment_size;
	
	DWORD lo_sz =  (DWORD)(current_segment_info->segment_size & 0xFFFFFFFFL);
	DWORD hi_sz =  (DWORD)((current_segment_info->segment_size >> 32) & 0xFFFFFFFFL);
	temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, msh_segment_name);
	err = GetLastError();
	if(temp_handle == NULL)
	{
		releaseProcLock();
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:SharedMemory:init",
					   "SharedMemory::Could not create the memory segment");
	}
	else if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), shm_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	else
	{
		*shm_handle = temp_handle;
	}
	mxFree(msh_segment_name);
	
	*current_segment_p = (byte_t*)MapViewOfFile(*shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, seg_info->segment_size);
	if(copy_info)
	{
		memcpy(*current_segment_p, &hdr, hdr.shmsiz);
	}
	
	UnmapViewOfFile(seg_info);
	
	global_shared_variable = mxCreateDoubleMatrix(0, 0, mxREAL);
	mexAtExit(onExit);
	
	releaseProcLock();
	
}


void onExit(void)
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
	CloseHandle(*proc_lock);
	mxFree(shm_handle);
	mxFree(shm_name_handle);
	mxFree(proc_lock);
	mxFree(lock_sec);
	mxFree(current_segment_info);
	mxFree(current_segment_p);
	
}

size_t pad_to_align(size_t size)
{
	if(size%align_size)
	{
		size += align_size - (size%align_size);
	}
	return size;
}

void acquireProcLock(void)
{
	DWORD ret = WaitForSingleObject(*proc_lock, INFINITE);
	if(ret == WAIT_ABANDONED)
	{
		mexErrMsgIdAndTxt("MATLAB:MatShare:acquireProcLock",
					   "The wait for process lock was abandoned.");
	}
	else if(ret == WAIT_FAILED)
	{
		mexPrintf("Error number: %d\n", GetLastError());
		mexErrMsgIdAndTxt("MATLAB:MatShare:acquireProcLock",
					   "The wait for process lock failed.");
	}
}


void releaseProcLock(void)
{
	if(ReleaseMutex(*proc_lock) == 0)
	{
		mexPrintf("Error number: %d\n", GetLastError());
		mexErrMsgIdAndTxt("MATLAB:MatShare:releaseProcLock",
					   "The process lock release failed.");
	}
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
