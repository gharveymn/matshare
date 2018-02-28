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
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		mexErrMsgIdAndTxt("MATLAB:MatShare", "Minimum input arguments missing; must supply directive and key.");
	}
	
	/* assign inputs */
	mxDirective = prhs[0];
	
	/* get directive */
	directive = parseDirective(mxDirective);
	DWORD hi_sz, lo_sz, err;
	mxArrayStruct* arr;
	
	HANDLE temp_handle;
	
	/* Switch yard {clone, attach, detach, free} */
	switch(directive)
	{
		case msh_INIT:
			init();
			glob_info->is_mem_safe = (bool_t)(nrhs <= 1);
			makeDummyVar(&plhs[0]);
			break;
		
		case msh_CLONE:
			/********************/
			/*	Clone case		*/
			/********************/
			
			//TODO if every all dimensions are the same size, just copy over the data and don't reattach etc.
			
			/* check the inputs */
			if(nrhs < 2)
			{
				mexErrMsgIdAndTxt("MATLAB:MatShare:clone", "Required second argument is missing.");
			}
			
			/* Assign */
			mxInput = prhs[1];
			
			if(!(mxIsNumeric(mxInput) || mxIsLogical(mxInput) || mxIsChar(mxInput) || mxIsStruct(mxInput) || mxIsCell(mxInput)))
			{
				mexErrMsgIdAndTxt("MATLAB:MatShare:clone", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
			}
			
			acquireProcLock();
			
			/* clear the previous variable */
			if(!mxIsEmpty(glob_shm_var))
			{
				/* if the current shared variable shares all the same dimensions, etc. then just copy over the memory */
				/* copy over the data at the same time since otherwise it will be gone */
				if(shallowcompare(glob_info->cur_seg_ptr, mxInput, &sm_size) == TRUE)
				{
					/* update the revision number and indicate our info is current */
					glob_info->shm_seg_info->rev_num += 1;
					glob_info->cur_seg_info.rev_num = glob_info->shm_seg_info->rev_num;
					
					/* tell everyone to come back to this segment */
					glob_info->shm_seg_info->seg_num = glob_info->cur_seg_info.seg_num;
					glob_info->shm_seg_info->seg_sz = glob_info->cur_seg_info.seg_sz;
					
					shallowrewrite(glob_info->cur_seg_ptr, mxInput);
					
					releaseProcLock();
					break;
				}
				else
				{
					deepdetach(glob_shm_var);
				}
			}
			mxDestroyArray(glob_shm_var);
			
			/* scan input data */
			sm_size = deepscan(&hdr, &dat, mxInput, NULL, &glob_shm_var);
			
			/* update the revision number and indicate our info is current */
			glob_info->shm_seg_info->rev_num += 1;
			glob_info->cur_seg_info.rev_num = glob_info->shm_seg_info->rev_num;
			if(sm_size <= glob_info->cur_seg_info.seg_sz)
			{
				/* reuse the current segment, which we should still be mapped to */
				
				/* tell everyone to come back to this segment */
				glob_info->shm_seg_info->seg_num = glob_info->cur_seg_info.seg_num;
				glob_info->shm_seg_info->seg_sz = glob_info->cur_seg_info.seg_sz;
				
				/* don't close the current file handle (obviously) */
				
			}
			else
			{
				
				// get current map number
				
				/* warning: this may cause a collision in the rare case where one process is still at seg_num = 0 and lead_num = UINT32_MAX; very unlikely, so the overhead isn't worth it */
				glob_info->shm_seg_info->lead_num += 1;
				
				glob_info->cur_seg_info.seg_num = glob_info->shm_seg_info->lead_num;
				glob_info->shm_seg_info->seg_num = glob_info->cur_seg_info.seg_num;
				glob_info->shm_seg_info->seg_sz = sm_size;
				glob_info->cur_seg_info.seg_sz = sm_size;
				
				sprintf(glob_info->cur_seg_info.seg_name, MSH_SEGMENT_NAME, glob_info->shm_seg_info->seg_num);
				
				/* decrement the kernel handle count and tell onExit not to do this twice */
				UnmapViewOfFile(glob_info->cur_seg_ptr);
				CloseHandle(glob_info->shm_handle);
				glob_info->shm_is_used = FALSE;
				
				// create the new mapping
				lo_sz = (DWORD)(sm_size & 0xFFFFFFFFL);
				hi_sz = (DWORD)((sm_size >> 32) & 0xFFFFFFFFL);
				temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, glob_info->cur_seg_info.seg_name);
				err = GetLastError();
				if(temp_handle == NULL)
				{
					// throw error if it already exists because that shouldn't happen
					makeDummyVar(&plhs[0]);
					releaseProcLock();
					mexPrintf("Error number: %d\n", err);
					mexErrMsgIdAndTxt("MATLAB:MatShare:init", "MatShare::Could not create the memory segment");
				}
				else if(err == ERROR_ALREADY_EXISTS)
				{
					DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->shm_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
				}
				else
				{
					glob_info->shm_handle = temp_handle;
				}
				glob_info->cur_seg_ptr = (byte_t*)MapViewOfFile(glob_info->shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, sm_size);
				glob_info->shm_is_used = TRUE;
				
			}
			
			/* copy data to the shared memory */
			deepcopy(&hdr, &dat, glob_info->cur_seg_ptr, NULL, glob_shm_var);
			
			/* free temporary allocation */
			deepfree(&dat);
			
			mexMakeArrayPersistent(glob_shm_var);
			
			releaseProcLock();
			
			break;
		
		case msh_ATTACH:
			/********************/
			/*	Attach case	*/
			/********************/
			
			
			/* Restore the segment if this an initialized region */
			shallowrestore(glob_info->cur_seg_ptr, glob_shm_var);
			
			/* proc_lock was acquired either in CLONE or FETCH */
//			releaseProcLock();
			
			break;
		
		case msh_DETACH:
			/********************/
			/*	Dettach case	*/
			/********************/
			
			if(!mxIsEmpty(glob_shm_var))
			{
				/* NULL all of the Matlab pointers */
				deepdetach(glob_shm_var);
			}
			mxDestroyArray(glob_shm_var);
			
			makeDummyVar(&plhs[0]);
			
			break;
		
		case msh_FREE:
			/********************/
			/*	free case		*/
			/********************/
			
			/* this is the same as DETACH, but unlocks the mex file, and closes the segment */
			
			if(!mxIsEmpty(glob_shm_var))
			{
				/* NULL all of the Matlab pointers */
				deepdetach(glob_shm_var);
			}
			mxDestroyArray(glob_shm_var);
			
			if(!glob_info->is_freed)
			{
				
				/* decrement the kernel handle count and tell onExit not to do this twice */
				if(glob_info->shm_is_used)
				{
					UnmapViewOfFile(glob_info->cur_seg_ptr);
					CloseHandle(glob_info->shm_handle);
					glob_info->shm_is_used = FALSE;
				}
				
				glob_info->shm_seg_info->num_procs -= 1;
				
				/* if the function is process locked be sure to release it (if not needed, but for clarity)*/
				if(glob_info->is_proc_locked)
				{
					releaseProcLock();
				}
				
				glob_info->is_freed = TRUE;
				
			}
			
			mexUnlock();
			makeDummyVar(&plhs[0]);
			
			break;
		case msh_FETCH:
			
			acquireProcLock();
			
			/* check if the current revision number is the same as the shm revision number */
			if(glob_info->cur_seg_info.rev_num == glob_info->shm_seg_info->rev_num)
			{
				//return a shallow copy of the variable
				plhs[0] = mxCreateSharedDataCopy(glob_shm_var);
			}
			else
			{
				//do a detach operation
				if(!mxIsEmpty(glob_shm_var))
				{
					/* NULL all of the Matlab pointers */
					deepdetach(glob_shm_var);
				}
				mxDestroyArray(glob_shm_var);
				
				glob_info->cur_seg_info.rev_num = glob_info->shm_seg_info->rev_num;
				glob_info->cur_seg_info.seg_num = glob_info->shm_seg_info->seg_num;
				glob_info->cur_seg_info.seg_sz = glob_info->shm_seg_info->seg_sz;
				
				sprintf(glob_info->cur_seg_info.seg_name, MSH_SEGMENT_NAME, glob_info->cur_seg_info.seg_num);
				
				UnmapViewOfFile(glob_info->cur_seg_ptr);
				CloseHandle(glob_info->shm_handle);
				
				temp_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, glob_info->cur_seg_info.seg_name);
				err = GetLastError();
				if(temp_handle == NULL)
				{
					makeDummyVar(&plhs[0]);
					releaseProcLock();
					mexPrintf("Error number: %d\n", err);
					mexErrMsgIdAndTxt("MATLAB:MatShare:fetch", "MatShare::Could not open the memory segment");
				}
				
				DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->shm_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
				
				glob_info->cur_seg_ptr = (byte_t*)MapViewOfFile(glob_info->shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, glob_info->cur_seg_info.seg_sz);
				err = GetLastError();
				if(glob_info->cur_seg_ptr == NULL)
				{
					makeDummyVar(&plhs[0]);
					releaseProcLock();
					mexPrintf("Error number: %d\n", err);
					mexErrMsgIdAndTxt("MATLAB:MatShare:fetch", "MatShare::Could not fetch the memory segment");
				}
				shallowfetch(glob_info->cur_seg_ptr, &glob_shm_var);
				
				mexMakeArrayPersistent(glob_shm_var);
				plhs[0] = mxCreateSharedDataCopy(glob_shm_var);
				
			}
			
			releaseProcLock();
			
			break;
		
		case msh_COMPARE:
			acquireProcLock();
			plhs[0] = mxCreateLogicalScalar((mxLogical)(glob_info->cur_seg_info.seg_num != glob_info->shm_seg_info->seg_num));
			releaseProcLock();
			break;
		case msh_COPY:
			plhs[0] = mxCreateSharedDataCopy(glob_shm_var);
			break;
		case msh_DEEPCOPY:
			plhs[0] = mxDuplicateArray(glob_shm_var);
			break;
		case msh_DEBUG:
			arr = (mxArrayStruct*)glob_shm_var;
			mexPrintf("%d\n", arr->RefCount);
			break;
		default:
			mexErrMsgIdAndTxt("MATLAB:MatShare", "Unrecognized directive.");
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
	if(ret_var == (mxArray*)NULL || mxIsEmpty(ret_var))
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
		void* pr, * pi = NULL;
		
		/* handle sparse objects */
		if(mxIsSparse(ret_var))
		{
			/* I don't seem to be able to give sparse arrays zero size so (nzmax must be 1) */
			dims[0] = dims[1] = 1;
			nzmax = 1;
			if(mxSetDimensions(ret_var, dims, num_dims))
			{
				releaseProcLock();
				mexErrMsgIdAndTxt("MATLAB:MatShare:Unknown", "detach: unable to resize the array.");
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
		
		/** HACK **/
		/* reset all the crosslinks so nothing in MATLAB is pointing to shared data (which will be gone soon) */
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
		mexErrMsgIdAndTxt("MATLAB:MatShare:InvalidInput", "Detach: Unsupported type. The segment may have been corrupted.");
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
size_t shallowrestore(byte_t* shm, mxArray* ret_var)
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
	hdr = (header_t*)shm;
	shm += pad_to_align(sizeof(header_t));
	
	/* don't do anything if this is an empty matrix */
	if(hdr->isEmpty)
	{
		return hdr->shmsiz;
	}
	
	/* the size pointer */
	pSize = (mwSize*)shm;
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize)*hdr->nDims);
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
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
		pr = (void*)shm;
		shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));          /* takes us to the end of the real data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			pi = (void*)shm;
			shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));     /* takes us to the end of the complex data */
		}
		
		/* set the real and imaginary data */
		mxFree(mxGetData(ret_var));
		mxSetData(ret_var, pr);
		if(hdr->complexity)
		{
			mxFree(mxGetImagData(ret_var));
			mxSetImagData(ret_var, pi);
		}
		
		/* if sparse get a list of the elements */
		if(hdr->isSparse)
		{
			
			ir = (mwIndex*)shm;
			shm += pad_to_align((hdr->nzmax)*sizeof(mwIndex));
			
			jc = (mwIndex*)shm;
			shm += pad_to_align((pSize[1] + 1)*sizeof(mwIndex));
			
			/* set the pointers relating to sparse (set the real and imaginary data later)*/
			mxFree(mxGetIr(ret_var));
			mwIndex* ret_ir = (mwIndex*)mxMalloc(hdr->nzmax*sizeof(mwIndex));
			memcpy(ret_ir, ir, hdr->nzmax*sizeof(mwIndex));
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
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	shm += pad_to_align(sizeof(header_t));
	
	
	/* the size pointer */
	pSize = (mwSize*)shm;
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize)*hdr->nDims);
	
	
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
			char** field_names = (char**)mxCalloc((size_t)hdr->nFields, sizeof(char*));
			PointCharArrayAtString(field_names, shm, hdr->nFields);
			
			/* skip over the stored field string */
			shm += pad_to_align(hdr->strBytes);
			
			*ret_var = mxCreateStructArray(hdr->nDims, pSize, hdr->nFields, (const char**)field_names);
			
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
		shm_pr = (void*)shm;
		shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));          /* takes us to the end of the real data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			shm_pi = (void*)shm;
			shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));     /* takes us to the end of the complex data */
		}
		
		if(hdr->isSparse)
		{
			
			shm_ir = (mwIndex*)shm;
			shm += pad_to_align((hdr->nzmax)*sizeof(mwIndex));
			
			shm_jc = (mwIndex*)shm;
			shm += pad_to_align((pSize[1] + 1)*sizeof(mwIndex));
			
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateSparse(pSize[0], pSize[1], 1, hdr->complexity);
			}
			else
			{
				*ret_var = mxCreateSparseLogicalMatrix(pSize[0], pSize[1], 1);
			}
			
			mwIndex* ret_jc = mxGetJc(*ret_var);
			memcpy(ret_jc, shm_jc, hdr->nzmax*sizeof(mwIndex));
			ret_jc[pSize[1]] = 0;
			
			/* set the pointers relating to sparse (set the real and imaginary data later)*/
			mxFree(mxGetIr(*ret_var));
			mwIndex* ret_ir = (mwIndex*)mxMalloc(hdr->nzmax*sizeof(mwIndex));
			memcpy(ret_ir, shm_ir, hdr->nzmax*sizeof(mwIndex));
			mxSetIr(*ret_var, ret_ir);
			
			mxSetNzmax(*ret_var, hdr->nzmax);
			
		}
		else
		{
			
			if(hdr->isEmpty)
			{
				if(hdr->isNumeric)
				{
					*ret_var = mxCreateNumericArray(hdr->nDims, pSize, hdr->classid, hdr->complexity);
				}
				else if(hdr->classid == mxLOGICAL_CLASS)
				{
					*ret_var = mxCreateLogicalArray(hdr->nDims, pSize);
				}
				else
				{
					*ret_var = mxCreateCharArray(hdr->nDims, pSize);
				}
			}
			else
			{
				if(hdr->isNumeric)
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
				
				/* set the real and imaginary data */
				mxFree(mxGetData(*ret_var));
				mxSetData(*ret_var, shm_pr);
				if(hdr->complexity)
				{
					mxFree(mxGetImagData(*ret_var));
					mxSetImagData(*ret_var, shm_pi);
				}
				
				mxSetDimensions(*ret_var, pSize, hdr->nDims);
				
			}
			
		}
		
	}
	return hdr->shmsiz;
}


bool_t shallowcompare(byte_t* shm, const mxArray* comp_var, size_t* offset)
{
	/* for working with shared memory ... */
	size_t i, shmshift;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	shm += pad_to_align(sizeof(header_t));
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	pSize = (mwSize*)shm;
	if(hdr->nDims != mxGetNumberOfDimensions(comp_var) || memcmp(pSize, mxGetDimensions(comp_var), sizeof(mwSize)*hdr->nDims) != 0)
	{
		return FALSE;
	}
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize)*hdr->nDims);
	
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
					return FALSE;
				}
				
				
				if(!shallowcompare(shm, mxGetFieldByNumber(comp_var, i, field_num), &shmshift))
				{
					return FALSE;
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
			if(!shallowcompare(shm, mxGetCell(comp_var, i), &shmshift))
			{
				return FALSE;
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
		shm_pr = (void*)shm;
		shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));          /* takes us to the end of the real data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			
			if(mxIsComplex(comp_var) != TRUE)
			{
				return FALSE;
			}
			
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			shm_pi = (void*)shm;
			shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));     /* takes us to the end of the complex data */
			
		}
		
		if(hdr->isSparse)
		{
			if(!mxIsSparse(comp_var))
			{
				return FALSE;
			}
			
			shm_ir = (mwIndex*)shm;
			shm += pad_to_align((hdr->nzmax)*sizeof(mwIndex));
			
			shm_jc = (mwIndex*)shm;
			shm += pad_to_align((pSize[1] + 1)*sizeof(mwIndex));
			
			if(hdr->nzmax != mxGetNzmax(comp_var))
			{
				return FALSE;
			}
			
		}
		else
		{
			if(hdr->nzmax != mxGetNumberOfElements(comp_var))
			{
				return FALSE;
			}
		}
		
	}
	
	*offset = hdr->shmsiz;
	return TRUE;
}


size_t shallowrewrite(byte_t* shm, const mxArray* input_var)
{
	
	/* for working with shared memory ... */
	size_t i;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	shm += pad_to_align(sizeof(header_t));
	
	/* the size pointer */
	pSize = (mwSize*)shm;
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize)*hdr->nDims);
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		/* skip over the stored field string */
		shm += pad_to_align(hdr->strBytes);
		
		/* Go through each element */
		for(i = 0; i < hdr->nzmax; i++)
		{
			for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
			{
				/* And fill it */
				shm += shallowrewrite(shm, mxGetFieldByNumber(input_var, i, field_num));
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		for(i = 0; i < hdr->nzmax; i++)
		{
			/* And fill it */
			shm += shallowrewrite(shm, mxGetCell(input_var, i));
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		shm += pad_to_align(MXMALLOC_SIG_LEN);
		shm_pr = (void*)shm;
		memcpy(shm_pr, mxGetData(input_var), (hdr->nzmax)*(hdr->elemsiz));
		shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));          /* takes us to the end of the real data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			shm_pi = (void*)shm;
			memcpy(shm_pi, mxGetImagData(input_var), (hdr->nzmax)*(hdr->elemsiz));
			shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));     /* takes us to the end of the complex data */
		}
		
		/* if sparse get a list of the elements */
		if(hdr->isSparse)
		{
			
			shm_ir = (mwIndex*)shm;
			shm += pad_to_align((hdr->nzmax)*sizeof(mwIndex));
			
			shm_jc = (mwIndex*)shm;
			shm += pad_to_align((pSize[1] + 1)*sizeof(mwIndex));
			
			memcpy(shm_ir, mxGetIr(input_var), (hdr->nzmax)*sizeof(mwIndex));
			memcpy(shm_jc, mxGetJc(input_var), (pSize[1] + 1)*sizeof(mwIndex));
			
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
	dat->pSize = (mwSize*)NULL;
	dat->pr = (void*)NULL;
	dat->pi = (void*)NULL;
	dat->ir = (mwIndex*)NULL;
	dat->jc = (mwIndex*)NULL;
	dat->child_dat = (data_t*)NULL;
	dat->child_hdr = (header_t*)NULL;
	dat->field_str = (char*)NULL;
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
	hdr->strBytes = 0;                                   /* update this later */
	
	/* copy the size */
	dat->pSize = (mwSize*)mxGetDimensions(mxInput);     /* some abuse of const for now, fix on deep copy*/
	
	/* Add space for the dimensions */
	hdr->shmsiz += pad_to_align(hdr->nDims*sizeof(mwSize));
	
	
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
			hdr->strBytes = (size_t)FieldNamesSize(mxInput);     /* always a multiple of alignment size */
			hdr->shmsiz += pad_to_align(hdr->strBytes);                   /* Add space for the field string */
			
			/* use mxCalloc so mem is freed on error via garbage collection */
			dat->child_hdr = (header_t*)mxCalloc(hdr->nzmax*hdr->nFields*(sizeof(header_t) + sizeof(data_t)) + hdr->strBytes, 1); /* extra space for the field string */
			dat->child_dat = (data_t*)&dat->child_hdr[hdr->nFields*hdr->nzmax];
			dat->field_str = (char*)&dat->child_dat[hdr->nFields*hdr->nzmax];
			
			const char** field_names = (const char**)mxMalloc(hdr->nFields*sizeof(char*));
			
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
					hdr->shmsiz += deepscan(&(dat->child_hdr[count]), &(dat->child_dat[count]), mxGetFieldByNumber(mxInput, i, field_num), hdr, &ret_child);
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
			dat->child_hdr = (header_t*)mxCalloc(hdr->nzmax, sizeof(header_t) + sizeof(data_t));
			dat->child_dat = (data_t*)&dat->child_hdr[hdr->nzmax];
			
			*ret_var = mxCreateCellArray(hdr->nDims, dat->pSize);
			
			/* go through each recursively */
			for(i = 0; i < hdr->nzmax; i++)
			{
				hdr->shmsiz += deepscan(&(dat->child_hdr[i]), &(dat->child_dat[i]), mxGetCell(mxInput, i), hdr, &ret_child);
				mxSetCell(*ret_var, i, ret_child);
			}
			
			/* if its a cell it has to have at least one mom-empty element */
			if(hdr->shmsiz == (1 + hdr->nzmax)*pad_to_align(sizeof(header_t)))
			{
				releaseProcLock();
				mexErrMsgIdAndTxt("MATLAB:MatShare:clone", "Required (variable) must contain at "
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
			hdr->shmsiz += pad_to_align(sizeof(mwIndex)*(hdr->nzmax));      /* ensure both pointers are aligned individually */
			hdr->shmsiz += pad_to_align(sizeof(mwIndex)*(dat->pSize[1] + 1));
			
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateSparse(mxGetM(mxInput), mxGetN(mxInput), 1, hdr->complexity);
			}
			else
			{
				*ret_var = mxCreateSparseLogicalMatrix(mxGetM(mxInput), mxGetN(mxInput), 1);
			}
			
			mwIndex* ret_jc = mxGetJc(*ret_var);
			memcpy(ret_jc, dat->jc, hdr->nzmax*sizeof(mwIndex));
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
		hdr->shmsiz += pad_to_align(MXMALLOC_SIG_LEN);               //this is 32 bytes
		hdr->shmsiz += pad_to_align(hdr->elemsiz*hdr->nzmax);
		if(hdr->complexity == mxCOMPLEX)
		{
			hdr->shmsiz += pad_to_align(MXMALLOC_SIG_LEN);               //this is 32 bytes
			hdr->shmsiz += pad_to_align(hdr->elemsiz*hdr->nzmax);
		}
	}
	else
	{
		releaseProcLock();
		mexPrintf("Unknown class found: %s\n", mxGetClassName(mxInput));
		mexErrMsgIdAndTxt("MATLAB:MatShare:InvalidInput", "clone: unsupported type.");
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
void deepcopy(header_t* hdr, data_t* dat, byte_t* shm, header_t* par_hdr, mxArray* ret_var)
{
	
	header_t* cpy_hdr;          /* the header in its copied location */
	size_t i, n, offset = 0;
	mwSize* pSize;               /* points to the size data */
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	/* load up the shared memory */
	
	/* copy the header */
	n = sizeof(header_t);
	memcpy(shm, hdr, n);
	offset = pad_to_align(n);
	
	/* for structures */
	int field_num;                /* current field */
	
	/* copy the dimensions */
	cpy_hdr = (header_t*)shm;
	pSize = (mwSize*)&shm[offset];
	for(i = 0; i < cpy_hdr->nDims; i++)
	{
		pSize[i] = dat->pSize[i];
	}
	offset += pad_to_align(cpy_hdr->nDims*sizeof(mwSize));
	
	/* assign the parent header  */
	cpy_hdr->par_hdr_off = (NULL == par_hdr)? 0 : (int)(par_hdr - cpy_hdr);
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
		for(i = 0; i < hdr->nzmax; i++)
		{
			for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
			{
				/* And fill it */
				size_t idx = i*hdr->nzmax + field_num;
				deepcopy(&(dat->child_hdr[idx]), &(dat->child_dat[idx]), shm, cpy_hdr, mxGetFieldByNumber(ret_var, i, field_num));
				shm += (dat->child_hdr[idx]).shmsiz;
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		/* recurse for each cell element */
		
		/* recurse through each cell */
		for(i = 0; i < hdr->nzmax; i++)
		{
			deepcopy(&(dat->child_hdr[i]), &(dat->child_dat[i]), shm, cpy_hdr, mxGetCell(ret_var, i));
			shm += (dat->child_hdr[i]).shmsiz;
		}
	}
	else /* base case */
	{
		/* copy real data */
		memcpy(shm + (pad_to_align(MXMALLOC_SIG_LEN) - MXMALLOC_SIG_LEN), MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
		shm += pad_to_align(MXMALLOC_SIG_LEN);
		shm_pr = (void*)shm;
		n = (hdr->nzmax)*(hdr->elemsiz);
		memcpy(shm_pr, dat->pr, n);
		shm += pad_to_align(n);
		
		mxFree(mxGetData(ret_var));
		mxSetData(ret_var, shm_pr);
		
		/* copy complex data as well */
		if(hdr->complexity == mxCOMPLEX)
		{
			memcpy(shm + (pad_to_align(MXMALLOC_SIG_LEN) - MXMALLOC_SIG_LEN), MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			shm_pi = (void*)shm;
			n = (hdr->nzmax)*(hdr->elemsiz);
			memcpy(shm_pi, dat->pi, n);
			shm += pad_to_align(n);
			
			mxFree(mxGetImagData(ret_var));
			mxSetImagData(ret_var, shm_pi);
			
		}
		
		/* and the indices of the sparse data as well */
		if(hdr->isSparse)
		{
			shm_ir = (void*)shm;
			n = hdr->nzmax*sizeof(mwIndex);
			memcpy(shm_ir, dat->ir, n);
			shm += pad_to_align(n);
			
			shm_jc = (void*)shm;
			n = (pSize[1] + 1)*sizeof(mwIndex);
			memcpy(shm_jc, dat->jc, n);
			shm += pad_to_align(n);
			
			/* set the pointers relating to sparse (set the real and imaginary data later)*/
			mxFree(mxGetIr(ret_var));
			mwIndex* ret_ir = (mwIndex*)mxMalloc(hdr->nzmax*sizeof(mwIndex));
			memcpy(ret_ir, shm_ir, hdr->nzmax*sizeof(mwIndex));
			mxSetIr(ret_var, ret_ir);
			
			mxSetNzmax(ret_var, hdr->nzmax);
			
		}
		else
		{
			
			mxSetDimensions(ret_var, pSize, hdr->nDims);
			
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
	dat->child_hdr = (header_t*)NULL;
	dat->child_dat = (data_t*)NULL;
	dat->field_str = (char*)NULL;
}


mxLogical deepcompare(byte_t* shm, const mxArray* comp_var, size_t* offset)
{
	/* for working with shared memory ... */
	size_t i, shmshift;
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	shm += pad_to_align(sizeof(header_t));
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	pSize = (mwSize*)shm;
	if(hdr->nDims != mxGetNumberOfDimensions(comp_var) || memcmp(pSize, mxGetDimensions(comp_var), sizeof(mwSize)*hdr->nDims) != 0 || hdr->nzmax != mxGetNumberOfElements(comp_var))
	{
		return FALSE;
	}
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize)*hdr->nDims);
	
	
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
					return FALSE;
				}
				
				
				if(!deepcompare(shm, mxGetFieldByNumber(comp_var, i, field_num), &shmshift))
				{
					return FALSE;
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
			if(!deepcompare(shm, mxGetCell(comp_var, i), &shmshift))
			{
				return FALSE;
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
		shm_pr = (void*)shm;
		shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));          /* takes us to the end of the real data */
		
		//these should point to the same location
		if(shm_pr != mxGetData(comp_var))
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
			
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			shm_pi = (void*)shm;
			shm += pad_to_align((hdr->nzmax)*(hdr->elemsiz));     /* takes us to the end of the complex data */
			
			//these should point to the same location
			if(shm_pi != mxGetImagData(comp_var))
			{
				return FALSE;
			}
			
		}
		
		if(hdr->isSparse)
		{
			if(!mxIsSparse(comp_var))
			{
				return FALSE;
			}
			
			shm_ir = (mwIndex*)shm;
			shm += pad_to_align((hdr->nzmax)*sizeof(mwIndex));
			
			shm_jc = (mwIndex*)shm;
			shm += pad_to_align((pSize[1] + 1)*sizeof(mwIndex));
			
			if(memcmp(shm_ir, mxGetIr(comp_var), (hdr->nzmax)*sizeof(mwIndex)) != 0 || memcmp(shm_jc, mxGetJc(comp_var), (pSize[1] + 1)*sizeof(mwIndex)) != 0)
			{
				return FALSE;
			}
			
		}
		
	}
	*offset = hdr->shmsiz;
	return TRUE;
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
		while(Bytes%align_size)
		{
			pList[Bytes++] = 0;
		}
	}
	
	/* Add an extra alignment size to store the length of the string (in Bytes) */
	*(unsigned int*)&pList[Bytes] = (unsigned int)(Bytes + align_size);
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
		if(i%align_size)
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
	bool_t term_found = FALSE;          /* has the termination character been found? */
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
			if(i%align_size)
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
	stored_length = *(unsigned int*)&pString[pBytes[0]];       /* the recorded length of the string */
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
	
	int offset = (int)align_size;            /* start from the end of the string */
	pBytes[0] = 0;                           /* default */
	
	/* Grab it directly from the string */
	pBytes[0] = (*(size_t*)&pString[-offset]);
	
	/* scan for the termination character */
	while((offset < 2*align_size) && (pString[-offset] != term_char))
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
	
	mexLock();
	
	DWORD err;
	HANDLE temp_handle;
	
	bool_t is_global_init;
	
	glob_info = (mex_info*)mxMalloc(sizeof(mex_info));
	mexMakeMemoryPersistent(glob_info);
	
	glob_info->lock_sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	glob_info->lock_sec.lpSecurityDescriptor = NULL;
	glob_info->lock_sec.bInheritHandle = TRUE;
	
	temp_handle = CreateMutex(&glob_info->lock_sec, FALSE, MSH_LOCK_NAME);
	err = GetLastError();
	if(temp_handle == NULL)
	{
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:MatShare:init", "MatShare::Could not create the memory segment");
	}
	else if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->proc_lock, 0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	else
	{
		glob_info->proc_lock = temp_handle;
	}
	
	temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(segment_info), MSH_NAME_SEGMENT_NAME);
	err = GetLastError();
	if(temp_handle == NULL)
	{
		releaseProcLock();
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:MatShare:init", "MatShare::Could not create the memory segment");
	}
	
	if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->shm_name_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
		is_global_init = FALSE;
	}
	else
	{
		/* will initialize */
		glob_info->shm_name_handle = temp_handle;
		is_global_init = TRUE;
	}
	
	header_t hdr;
	glob_info->shm_seg_info = (shm_segment_info*)MapViewOfFile(glob_info->shm_name_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(shm_segment_info));
	if(glob_info->shm_seg_info == NULL)
	{
		releaseProcLock();
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:MatShare:init", "MatShare::Could not map the name memory segment");
		exit(1);
	}
	
	if(is_global_init)
	{
		/* this is the first region created */
		/* this info shouldn't ever actually be used */
		/* but make sure the memory segment is consistent */
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
		
		glob_info->shm_seg_info->num_procs = 1;
		glob_info->shm_seg_info->seg_num = 0;
		glob_info->shm_seg_info->rev_num = 0;
		glob_info->shm_seg_info->lead_num = 0;
		glob_info->shm_seg_info->seg_sz = hdr.shmsiz;
	}
	else
	{
		glob_info->shm_seg_info->num_procs += 1;
	}
	
	/* make sure to init local proc_lock flag as FALSE */
	glob_info->is_proc_locked = FALSE;
	glob_info->is_mem_safe = TRUE;
	acquireProcLock();
	
	/* in any case we want to make sure that the current segment is either
	 * the null case, or differs from the current shared segment number */
	glob_info->cur_seg_info.rev_num = 0;
	glob_info->cur_seg_info.seg_num = 0;
	glob_info->cur_seg_info.seg_sz = 0;
	
	sprintf(glob_info->cur_seg_info.seg_name, MSH_SEGMENT_NAME, glob_info->shm_seg_info->seg_num);
	
	DWORD lo_sz = (DWORD)(glob_info->shm_seg_info->seg_sz & 0xFFFFFFFFL);
	DWORD hi_sz = (DWORD)((glob_info->shm_seg_info->seg_sz >> 32) & 0xFFFFFFFFL);
	temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, glob_info->cur_seg_info.seg_name);
	err = GetLastError();
	if(temp_handle == NULL)
	{
		releaseProcLock();
		mexPrintf("Error number: %d\n", err);
		mexErrMsgIdAndTxt("MATLAB:MatShare:init", "MatShare::Could not create the memory segment");
	}
	else if(err == ERROR_ALREADY_EXISTS)
	{
		DuplicateHandle(GetCurrentProcess(), temp_handle, GetCurrentProcess(), &glob_info->shm_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	else
	{
		glob_info->shm_handle = temp_handle;
	}
	glob_info->cur_seg_ptr = (byte_t*)MapViewOfFile(glob_info->shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, glob_info->shm_seg_info->seg_sz);
	glob_info->shm_is_used = TRUE;
	glob_info->is_freed = FALSE;
	
	if(is_global_init)
	{
		memcpy(glob_info->cur_seg_ptr, &hdr, hdr.shmsiz);
	}
	
	mexAtExit(onExit);
	
	releaseProcLock();
	
}


void onExit(void)
{
	
	if(!mxIsEmpty(glob_shm_var))
	{
		/* NULL all of the Matlab pointers */
		deepdetach(glob_shm_var);
	}
	mxDestroyArray(glob_shm_var);
	
	if(!glob_info->is_freed)
	{
		glob_info->shm_seg_info->num_procs -= 1;
	}
	
	if(glob_info->shm_is_used)
	{
		UnmapViewOfFile(glob_info->cur_seg_ptr);
		CloseHandle(glob_info->shm_handle);
		glob_info->shm_is_used = FALSE;
	}
	
	UnmapViewOfFile(glob_info->shm_seg_info);
	CloseHandle(glob_info->shm_name_handle);
	
	if(glob_info->is_proc_locked)
	{
		releaseProcLock();
	}
	CloseHandle(glob_info->proc_lock);
	
	mxFree(glob_info);
	
}


size_t pad_to_align(size_t size)
{
	/* bitwise expression is equiv to (size % align_size) since align_size is a power of 2 */
	return size + align_size - (size | ~align_size);
}


void acquireProcLock(void)
{
	/* only request a lock if there is more than one process */
	if(glob_info->shm_seg_info->num_procs > 1 && glob_info->is_mem_safe)
	{
		DWORD ret = WaitForSingleObject(glob_info->proc_lock, INFINITE);
		if(ret == WAIT_ABANDONED)
		{
			mexErrMsgIdAndTxt("MATLAB:MatShare:acquireProcLock", "The wait for process lock was abandoned.");
		}
		else if(ret == WAIT_FAILED)
		{
			mexPrintf("Error number: %d\n", GetLastError());
			mexErrMsgIdAndTxt("MATLAB:MatShare:acquireProcLock", "The wait for process lock failed.");
		}
		glob_info->is_proc_locked = TRUE;
	}
}


void releaseProcLock(void)
{
	if(glob_info->is_proc_locked)
	{
		if(ReleaseMutex(glob_info->proc_lock) == 0)
		{
			mexPrintf("Error number: %d\n", GetLastError());
			mexErrMsgIdAndTxt("MATLAB:MatShare:releaseProcLock", "The process lock release failed.");
		}
		glob_info->is_proc_locked = FALSE;
	}
}


void makeDummyVar(mxArray** out)
{
	glob_shm_var = mxCreateDoubleMatrix(0, 0, mxREAL);
	mexMakeArrayPersistent(glob_shm_var);
	*out = mxCreateSharedDataCopy(glob_shm_var);
}

msh_directive_t parseDirective(const mxArray* in)
{
	
	if(mxIsNumeric(in))
	{
		return (msh_directive_t)*((uint8_t*)(mxGetData(in)));
	}
	else if(mxIsChar(in))
	{
		char dir_str[9];
		mxGetString(in, dir_str, 9);
		for(int i = 0; dir_str[i]; i++)
		{
			dir_str[i] = (char)tolower(dir_str[i]);
		}
		
		if(strcmp(dir_str, "init") == 0)
		{
			return msh_INIT;
		}
		else if(strcmp(dir_str, "clone") == 0)
		{
			return msh_CLONE;
		}
		else if(strcmp(dir_str, "attach") == 0)
		{
			return msh_ATTACH;
		}
		else if(strcmp(dir_str, "detach") == 0)
		{
			return msh_DETACH;
		}
		else if(strcmp(dir_str, "free") == 0)
		{
			return msh_FREE;
		}
		else if(strcmp(dir_str, "fetch") == 0)
		{
			return msh_FETCH;
		}
		else if(strcmp(dir_str, "compare") == 0)
		{
			return msh_COMPARE;
		}
		else if(strcmp(dir_str, "copy") == 0)
		{
			return msh_COPY;
		}
		else if(strcmp(dir_str, "deepcopy") == 0)
		{
			return msh_DEEPCOPY;
		}
		else if(strcmp(dir_str, "debug") == 0)
		{
			return msh_DEBUG;
		}
		else if(strcmp(dir_str, "share") == 0)
		{
			return msh_CLONE;
		}
		else
		{
			mexErrMsgIdAndTxt("MATLAB:MatShare:parseDirective", "Directive not recognized.");
		}
		
	}
	else
	{
		mexErrMsgIdAndTxt("MATLAB:MatShare:parseDirective", "Directive must either be 'uint8' or 'char'.");
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
