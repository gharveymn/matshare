#include "headers/matshare_.h"
#include "headers/msh_types.h"

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{

//	mexPrintf("%s\n", mexIsLocked()? "MEX is locked": "MEX is unlocked" );
	
	if(!mexIsLocked())
	{
		/*then this is the process initializer (and maybe the global initializer; we'll find out later) */
		mexLock();
		init();
	}
	
	/* For inputs */
	const mxArray* mxDirective;			/* Directive {clone, attach, detach, free} */
	const mxArray* mxInput;           		/* Input array (for clone) */
	
	/* hack */
	mxArrayStruct* arr;
	
	/* For storing inputs */
	msh_directive_t directive;
	
	size_t sm_size;					/* size required by the shared memory */
	
	/* for storing the mxArrays */
	header_t hdr;
	data_t dat;
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		readMXError("NotEnoughInputsError", "Minimum input arguments missing; must supply a directive.");
	}
	
	/* assign inputs */
	mxDirective = prhs[0];
	
	/* get the directive */
	directive = parseDirective(mxDirective);

#ifdef MSH_WIN
	DWORD hi_sz, lo_sz, err;
#endif
	
	if(directive != msh_DETACH && precheck() != TRUE)
	{
		readMXError("NotInitializedError", "At least one of the needed shared memory segments has not been initialized. Cannot continue.");
	}
	
	/* Switch yard {clone, attach, detach, free} */
	switch(directive)
	{
		case msh_SHARE:
			/********************/
			/*	Share case	*/
			/********************/
			
			/* check the inputs */
			if(nrhs < 2)
			{
				readMXError("NoVariableError", "No variable supplied to clone.");
			}
			
			/* Assign */
			mxInput = prhs[1];
			
			if(!(mxIsNumeric(mxInput) || mxIsLogical(mxInput) || mxIsChar(mxInput) || mxIsStruct(mxInput) || mxIsCell(mxInput)))
			{
				readMXError("InvalidTypeError", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
			}
			
			acquireProcLock();
			
			shm_update_info->upd_pid = glob_info->this_pid;
			
			/* clear the previous variable if needed */
			if(!mxIsEmpty(glob_shm_var))
			{
				/* if the current shared variable shares all the same dimensions, etc. then just copy over the memory */
				if(shallowcompare(shm_data_ptr, mxInput, &sm_size) == TRUE)
				{
					/* DON'T INCREMENT THE REVISION NUMBER */
					/* this is an in-place change, so everyone is still fine */
					
					/* tell everyone to come back to this segment */
					shm_update_info->seg_num = glob_info->cur_seg_info.seg_num;
					shm_update_info->seg_sz = glob_info->cur_seg_info.seg_sz;
					shm_update_info->rev_num = glob_info->cur_seg_info.rev_num;
					
					/* do the rewrite after checking because the comparison is cheap */
					shallowrewrite(shm_data_ptr, mxInput);
					
					releaseProcLock();
					break;
				}
				else
				{
					/* otherwise we'll detach and start over */
					deepdetach(glob_shm_var);
				}
			}
			mxDestroyArray(glob_shm_var);
			glob_info->flags.is_glob_shm_var_init = FALSE;
			
			/* scan input data */
			sm_size = deepscan(&hdr, &dat, mxInput, NULL, &glob_shm_var);
			
			/* update the revision number and indicate our info is current */
			shm_update_info->lead_rev_num += 1;
			shm_update_info->rev_num = shm_update_info->lead_rev_num;
			glob_info->cur_seg_info.rev_num = shm_update_info->lead_rev_num;
			if(sm_size <= glob_info->cur_seg_info.seg_sz)
			{
				/* reuse the current segment, which we should still be mapped to */
				
				/* tell everyone to come back to this segment */
				shm_update_info->seg_num = glob_info->cur_seg_info.seg_num;
				shm_update_info->seg_sz = glob_info->cur_seg_info.seg_sz;
				
				/* don't close the current file handle (obviously) */
				
			}
			else
			{
				
				/* update the current map number if we can't reuse the current segment */
				
				/* warning: this may cause a collision in the rare case where one process is still at seg_num == 0
				 * and lead_seg_num == UINT64_MAX; very unlikely, so the overhead isn't worth it */
				shm_update_info->lead_seg_num += 1;
				
				glob_info->cur_seg_info.seg_num = shm_update_info->lead_seg_num;
				shm_update_info->seg_num = shm_update_info->lead_seg_num;
				shm_update_info->seg_sz = sm_size;
				glob_info->cur_seg_info.seg_sz = sm_size;
				
				sprintf(glob_info->shm_data_reg.name, MSH_SEGMENT_NAME, (unsigned long long)shm_update_info->seg_num);
				
#ifdef MSH_WIN
				
				/* decrement the kernel handle count and tell onExit not to do this twice */
				UnmapViewOfFile(shm_data_ptr);
				glob_info->shm_data_reg.is_mapped = FALSE;
				
				CloseHandle(glob_info->shm_data_reg.handle);
				glob_info->shm_data_reg.is_init = FALSE;
				
				// create the new mapping
				lo_sz = (uint32_t)(sm_size & 0xFFFFFFFFL);
				hi_sz = (uint32_t)((sm_size >> 32) & 0xFFFFFFFFL);
				glob_info->shm_data_reg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, glob_info->shm_data_reg.name);
				err = GetLastError();
				if(glob_info->shm_data_reg.handle == NULL)
				{
					// throw error if it already exists because that shouldn't happen
					makeDummyVar(&plhs[0]);
					releaseProcLock();
					readMXError("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
				}
				glob_info->shm_data_reg.is_init = TRUE;
				
				glob_info->shm_data_reg.ptr = MapViewOfFile(glob_info->shm_data_reg.handle, FILE_MAP_ALL_ACCESS, 0, 0, sm_size);
				glob_info->shm_data_reg.is_mapped = TRUE;
				
#else
				
				/* decrement the kernel handle count and tell onExit not to do this twice */
				munmap(shm_data_ptr, glob_info->shm_data_reg.reg_sz);
				glob_info->shm_data_reg.is_mapped = FALSE;
				
				shm_unlink(glob_info->shm_data_reg.name);
				glob_info->shm_data_reg.is_init = FALSE;
				
				/* create the new mapping */
				glob_info->shm_data_reg.handle = shm_open(glob_info->shm_data_reg.name, O_RDWR | O_CREAT, S_IRWXU);
				if(glob_info->shm_data_reg.handle == -1)
				{
					releaseProcLock();
					readShmError(errno);
				}
				glob_info->shm_data_reg.is_init = TRUE;
				
				if(ftruncate(glob_info->shm_data_reg.handle, shm_update_info->seg_sz) != 0)
				{
					releaseProcLock();
					readFtruncateError(errno);
				}
				
				glob_info->shm_data_reg.ptr = mmap(NULL, shm_update_info->seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, glob_info->shm_data_reg.handle, 0);
				if(shm_data_ptr == MAP_FAILED)
				{
					releaseProcLock();
					readMmapError(errno);
				}
				glob_info->shm_data_reg.is_mapped = TRUE;
#endif
			
			}
			
			/* copy data to the shared memory */
			deepcopy(&hdr, &dat, shm_data_ptr, NULL, glob_shm_var);
			
			/* free temporary allocation */
			deepfreetmp(&dat);
			
			mexMakeArrayPersistent(glob_shm_var);
			glob_info->flags.is_glob_shm_var_init = TRUE;
			
			releaseProcLock();
			
			break;
			
		case msh_FETCH:
			
			acquireProcLock();
			
			/* check if the current revision number is the same as the shm revision number */
			if(glob_info->cur_seg_info.rev_num == shm_update_info->rev_num || glob_info->this_pid == shm_update_info->upd_pid)
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
				
				glob_info->cur_seg_info.rev_num = shm_update_info->rev_num;
				glob_info->cur_seg_info.seg_sz = shm_update_info->seg_sz;
				
				/* Update the mapping if this is on a new segment */
				if(glob_info->cur_seg_info.seg_num != shm_update_info->seg_num)
				{
					glob_info->cur_seg_info.seg_num = shm_update_info->seg_num;
					sprintf(glob_info->shm_data_reg.name, MSH_SEGMENT_NAME, (unsigned long long)glob_info->cur_seg_info.seg_num);
					
#ifdef MSH_WIN
					
					UnmapViewOfFile(shm_data_ptr);
					glob_info->shm_data_reg.is_mapped = FALSE;
					
					CloseHandle(glob_info->shm_data_reg.handle);
					glob_info->shm_data_reg.is_init = FALSE;
					
					glob_info->shm_data_reg.handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, glob_info->shm_data_reg.name);
					err = GetLastError();
					if(glob_info->shm_data_reg.handle == NULL)
					{
						makeDummyVar(&plhs[0]);
						releaseProcLock();
						readMXError("OpenFileError", "Error opening the file mapping (Error Number %u)", err);
					}
					glob_info->shm_data_reg.is_init = TRUE;
					
					glob_info->shm_data_reg.ptr = MapViewOfFile(glob_info->shm_data_reg.handle, FILE_MAP_ALL_ACCESS, 0, 0, glob_info->cur_seg_info.seg_sz);
					err = GetLastError();
					if(shm_data_ptr == NULL)
					{
						makeDummyVar(&plhs[0]);
						releaseProcLock();
						readMXError("MappingError", "Could not fetch the memory segment (Error number: %d).", err);
					}
					glob_info->shm_data_reg.is_mapped = TRUE;
					
#else
					
					munmap(shm_data_ptr, glob_info->shm_data_reg.reg_sz);
					glob_info->shm_data_reg.is_mapped = FALSE;
					
					shm_unlink(glob_info->shm_data_reg.name);
					glob_info->shm_data_reg.is_init = FALSE;
					
					// create the new mapping
					glob_info->shm_data_reg.handle = shm_open(glob_info->shm_data_reg.name, O_RDWR | O_CREAT, S_IRWXU);
					if(glob_info->shm_data_reg.handle == -1)
					{
						releaseProcLock();
						readShmError(errno);
					}
					glob_info->shm_data_reg.is_init = TRUE;
					
					if(ftruncate(glob_info->shm_data_reg.handle, shm_update_info->seg_sz) != 0)
					{
						releaseProcLock();
						readFtruncateError(errno);
					}
					
					glob_info->shm_data_reg.ptr = mmap(NULL, shm_update_info->seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, glob_info->shm_data_reg.handle, 0);
					if(shm_data_ptr == MAP_FAILED)
					{
						releaseProcLock();
						readMmapError(errno);
					}
					glob_info->shm_data_reg.is_mapped = TRUE;
#endif
				
				}
				
				shallowfetch(shm_data_ptr, &glob_shm_var);
				
				mexMakeArrayPersistent(glob_shm_var);
				plhs[0] = mxCreateSharedDataCopy(glob_shm_var);
				
			}
			
			releaseProcLock();
			
			break;
		
		case msh_DETACH:
			/********************/
			/*	Dettach case	*/
			/********************/
			
			/* this only actually frees if it is the last object using the function in the current process */
			
			
			onExit();
			mexUnlock();
			
			break;
			
		case msh_PARAM:
			/* set parameters for matshare to use */
			
			break;
		
		case msh_DEEPCOPY:
			plhs[0] = mxDuplicateArray(glob_shm_var);
			break;
		case msh_DEBUG:
			arr = (mxArrayStruct*)glob_shm_var;
			mexPrintf("%d\n", arr->RefCount);
			break;
		default:
			readMXError("UnknownDirectiveError", "Unrecognized directive.");
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
	mwSize null_dims[] = {0, 0};
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
			null_dims[0] = null_dims[1] = 1;
			nzmax = 1;
			if(mxSetDimensions(ret_var, null_dims, num_dims))
			{
				//error for debugging---this is kinda iffy
				releaseProcLock();
				readMXError("DetachDimensionsError", "Unable to resize the array.");
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
			mxSetDimensions(ret_var, null_dims, num_dims);
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
			mxSetDimensions(link, null_dims, num_dims);
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
		readMXError("InvalidTypeError", "Unsupported type. The segment may have been corrupted.");
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
	mwSize* dims;             /* pointer to the array dimensions */
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
	dims = (mwSize*)shm;
	
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
			shm += pad_to_align((dims[1] + 1)*sizeof(mwIndex));
			
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
			mxSetDimensions(ret_var, dims, hdr->nDims);
			
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
	mwSize* dims;             /* pointer to the array dimensions */
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	shm += pad_to_align(sizeof(header_t));
	
	
	/* the size pointer */
	dims = (mwSize*)shm;
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize)*hdr->nDims);
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		if(hdr->isEmpty)
		{
			/* make this a separate case since there are no field names */
			*ret_var = mxCreateStructArray(hdr->nDims, dims, hdr->nFields, NULL);
		}
		else
		{
			char** field_names = (char**)mxCalloc((size_t)hdr->nFields, sizeof(char*));
			PointCharArrayAtString(field_names, shm, hdr->nFields);
			
			/* skip over the stored field string */
			shm += pad_to_align(hdr->strBytes);
			
			*ret_var = mxCreateStructArray(hdr->nDims, dims, hdr->nFields, (const char**)field_names);
			
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
		*ret_var = mxCreateCellArray(hdr->nDims, dims);
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
			shm += pad_to_align((dims[1] + 1)*sizeof(mwIndex));
			
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateSparse(dims[0], dims[1], 1, hdr->complexity);
			}
			else
			{
				*ret_var = mxCreateSparseLogicalMatrix(dims[0], dims[1], 1);
			}
			
			mwIndex* ret_jc = mxGetJc(*ret_var);
			memcpy(ret_jc, shm_jc, (dims[1] + 1)*sizeof(mwIndex));
			
			/* set the pointers relating to sparse (set the real and imaginary data later)*/
			mxFree(mxGetIr(*ret_var));
			mwIndex* ret_ir = (mwIndex*)mxMalloc(hdr->nzmax*sizeof(mwIndex));
			memcpy(ret_ir, shm_ir, hdr->nzmax*sizeof(mwIndex));
			mxSetIr(*ret_var, ret_ir);
			
			mxSetNzmax(*ret_var, hdr->nzmax);
			
			if(!hdr->isEmpty)
			{
				/* set the real and imaginary data */
				mxFree(mxGetData(*ret_var));
				mxSetData(*ret_var, shm_pr);
				if(hdr->complexity)
				{
					mxFree(mxGetImagData(*ret_var));
					mxSetImagData(*ret_var, shm_pi);
				}
			}
			
			
		}
		else
		{
			
			if(hdr->isEmpty)
			{
				if(hdr->isNumeric)
				{
					*ret_var = mxCreateNumericArray(hdr->nDims, dims, hdr->classid, hdr->complexity);
				}
				else if(hdr->classid == mxLOGICAL_CLASS)
				{
					*ret_var = mxCreateLogicalArray(hdr->nDims, dims);
				}
				else
				{
					*ret_var = mxCreateCharArray(hdr->nDims, dims);
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
				
				mxSetDimensions(*ret_var, dims, hdr->nDims);
				
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
	mwSize* dims;             /* pointer to the array dimensions */
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
	dims = (mwSize*)shm;
	if(hdr->nDims != mxGetNumberOfDimensions(comp_var) || memcmp(dims, mxGetDimensions(comp_var), sizeof(mwSize)*hdr->nDims) != 0)
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
			shm += pad_to_align((dims[1] + 1)*sizeof(mwIndex));
			
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
	mwSize* dims;             /* pointer to the array dimensions */
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	shm += pad_to_align(sizeof(header_t));
	
	/* the size pointer */
	dims = (mwSize*)shm;
	
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
			shm += pad_to_align((dims[1] + 1)*sizeof(mwIndex));
			
			memcpy(shm_ir, mxGetIr(input_var), (hdr->nzmax)*sizeof(mwIndex));
			memcpy(shm_jc, mxGetJc(input_var), (dims[1] + 1)*sizeof(mwIndex));
			
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
	dat->dims = (mwSize*)NULL;
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
		readMXError("UnexpectedError", "Input variable was unexpectedly NULL.");
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
	dat->dims = (mwSize*)mxGetDimensions(mxInput);     /* some abuse of const for now, fix on deep copy*/
	
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
				readMXError("UnexpectedError", "An empty struct array unexpectedly had more than one field. This is undefined behavior.");
			}
			*ret_var = mxCreateStructArray(hdr->nDims, dat->dims, hdr->nFields, NULL);
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
			
			*ret_var = mxCreateStructArray(hdr->nDims, dat->dims, hdr->nFields, field_names);
			
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
			*ret_var = mxCreateCellArray(hdr->nDims, dat->dims);
		}
		else
		{
			
			/* use mxCalloc so mem is freed on error via garbage collection */
			dat->child_hdr = (header_t*)mxCalloc(hdr->nzmax, sizeof(header_t) + sizeof(data_t));
			dat->child_dat = (data_t*)&dat->child_hdr[hdr->nzmax];
			
			*ret_var = mxCreateCellArray(hdr->nDims, dat->dims);
			
			/* go through each recursively */
			for(i = 0; i < hdr->nzmax; i++)
			{
				hdr->shmsiz += deepscan(&(dat->child_hdr[i]), &(dat->child_dat[i]), mxGetCell(mxInput, i), hdr, &ret_child);
				mxSetCell(*ret_var, i, ret_child);
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
			hdr->shmsiz += pad_to_align(sizeof(mwIndex)*(dat->dims[1] + 1));
			
			if(hdr->isNumeric)
			{
				*ret_var = mxCreateSparse(mxGetM(mxInput), mxGetN(mxInput), 1, hdr->complexity);
			}
			else
			{
				*ret_var = mxCreateSparseLogicalMatrix(mxGetM(mxInput), mxGetN(mxInput), 1);
			}
			
			mwIndex* ret_jc = mxGetJc(*ret_var);
			memcpy(ret_jc, dat->jc, (dat->dims[1] + 1)*sizeof(mwIndex));
			
		}
		else
		{
			if(hdr->isNumeric)
			{
				if(hdr->isEmpty)
				{
					*ret_var = mxCreateNumericArray(hdr->nDims, dat->dims, hdr->classid, hdr->complexity);
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
					*ret_var = mxCreateLogicalArray(hdr->nDims, dat->dims);
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
					*ret_var = mxCreateCharArray(hdr->nDims, dat->dims);
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
		readMXError("Internal:UnexpectedError", "Tried to clone an unsupported type.");
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
	size_t i, cpy_sz, offset = 0;
	mwSize* dims;               /* points to the size data */
	void* shm_pr = NULL, * shm_pi = NULL;  /* real and imaginary data pointers */
	mwIndex* shm_ir = NULL, * shm_jc = NULL;  /* sparse matrix data indices */
	
	/* load up the shared memory */
	
	/* copy the header */
	cpy_sz = sizeof(header_t);
	memcpy(shm, hdr, cpy_sz);
	offset = pad_to_align(cpy_sz);
	
	/* for structures */
	int field_num;                /* current field */
	
	/* copy the dimensions */
	cpy_hdr = (header_t*)shm;
	dims = (mwSize*)&shm[offset];
	for(i = 0; i < cpy_hdr->nDims; i++)
	{
		dims[i] = dat->dims[i];
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
		cpy_sz = (hdr->nzmax)*(hdr->elemsiz);
		
		uint8_t mxmalloc_sig[MXMALLOC_SIG_LEN];
		make_mxmalloc_signature(mxmalloc_sig, cpy_sz);
		
		memcpy(shm + (pad_to_align(MXMALLOC_SIG_LEN) - MXMALLOC_SIG_LEN), mxmalloc_sig, MXMALLOC_SIG_LEN);
		shm += pad_to_align(MXMALLOC_SIG_LEN);
		shm_pr = (void*)shm;
		memcpy(shm_pr, dat->pr, cpy_sz);
		shm += pad_to_align(cpy_sz);
		
		mxFree(mxGetData(ret_var));
		mxSetData(ret_var, shm_pr);
		
		/* copy complex data as well */
		if(hdr->complexity == mxCOMPLEX)
		{
			memcpy(shm + (pad_to_align(MXMALLOC_SIG_LEN) - MXMALLOC_SIG_LEN), mxmalloc_sig, MXMALLOC_SIG_LEN);
			shm += pad_to_align(MXMALLOC_SIG_LEN);
			shm_pi = (void*)shm;
			memcpy(shm_pi, dat->pi, cpy_sz);
			shm += pad_to_align(cpy_sz);
			
			mxFree(mxGetImagData(ret_var));
			mxSetImagData(ret_var, shm_pi);
			
		}
		
		/* and the indices of the sparse data as well */
		if(hdr->isSparse)
		{
			shm_ir = (void*)shm;
			cpy_sz = hdr->nzmax*sizeof(mwIndex);
			memcpy(shm_ir, dat->ir, cpy_sz);
			shm += pad_to_align(cpy_sz);
			
			shm_jc = (void*)shm;
			cpy_sz = (dims[1] + 1)*sizeof(mwIndex);
			memcpy(shm_jc, dat->jc, cpy_sz);
			shm += pad_to_align(cpy_sz);
			
			/* set the pointers relating to sparse (set the real and imaginary data later)*/
			mxFree(mxGetIr(ret_var));
			mwIndex* ret_ir = (mwIndex*)mxMalloc(hdr->nzmax*sizeof(mwIndex));
			memcpy(ret_ir, shm_ir, hdr->nzmax*sizeof(mwIndex));
			mxSetIr(ret_var, ret_ir);
			
			mxSetNzmax(ret_var, hdr->nzmax);
			
		}
		else
		{
			
			mxSetDimensions(ret_var, dims, hdr->nDims);
			
		}
	}
}


/* ------------------------------------------------------------------------- */
/* deepfreetmp                                                                  */
/*                                                                           */
/* Descend through header and data structure and free the memory.            */
/*                                                                           */
/* Arguments:                                                                */
/*    data container                                                         */
/* Returns:                                                                  */
/*    void                                                                   */
/* ------------------------------------------------------------------------- */
void deepfreetmp(data_t* dat)
{
	
	/* recurse to the bottom */
	if(dat->child_dat)
	{
		deepfreetmp(dat->child_dat);
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
	mwSize* dims;             /* pointer to the array dimensions */
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
	dims = (mwSize*)shm;
	if(hdr->nDims != mxGetNumberOfDimensions(comp_var) || memcmp(dims, mxGetDimensions(comp_var), sizeof(mwSize)*hdr->nDims) != 0 || hdr->nzmax != mxGetNumberOfElements(comp_var))
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
			shm += pad_to_align((dims[1] + 1)*sizeof(mwIndex));
			
			if(memcmp(shm_ir, mxGetIr(comp_var), (hdr->nzmax)*sizeof(mwIndex)) != 0 || memcmp(shm_jc, mxGetJc(comp_var), (dims[1] + 1)*sizeof(mwIndex)) != 0)
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


void onExit(void)
{
	
	if(glob_info->flags.is_glob_shm_var_init)
	{
		if(!mxIsEmpty(glob_shm_var))
		{
			/* NULL all of the Matlab pointers */
			deepdetach(glob_shm_var);
		}
		mxDestroyArray(glob_shm_var);
	}

#ifdef MSH_WIN
	
	if(glob_info->shm_data_reg.is_mapped)
	{
		UnmapViewOfFile(shm_data_ptr);
		glob_info->shm_data_reg.is_mapped = FALSE;
	}
	
	if(glob_info->shm_data_reg.is_init)
	{
		CloseHandle(glob_info->shm_data_reg.handle);
		glob_info->shm_data_reg.is_init = FALSE;
	}
	
	if(glob_info->shm_update_reg.is_mapped)
	{
		shm_update_info->num_procs -= 1;
		UnmapViewOfFile(shm_update_info);
		glob_info->shm_update_reg.is_mapped = FALSE;
	}
	
	if(glob_info->shm_update_reg.is_init)
	{
		CloseHandle(glob_info->shm_update_reg.handle);
		glob_info->shm_update_reg.is_init = FALSE;
	}
	
	if(glob_info->flags.is_proc_lock_init)
	{
		if(glob_info->flags.is_proc_locked)
		{
			releaseProcLock();
		}
		CloseHandle(glob_info->proc_lock);
		glob_info->flags.is_proc_lock_init = FALSE;
	}
	
	if(glob_info->startup_flag.is_init)
	{
		CloseHandle(glob_info->startup_flag.handle);
		glob_info->startup_flag.is_init = FALSE;
	}
	
#else
	
	if(glob_info->shm_data_reg.is_mapped)
	{
		munmap(shm_data_ptr, glob_info->shm_data_reg.reg_sz);
		glob_info->shm_data_reg.is_mapped = FALSE;
	}
	
	if(glob_info->shm_data_reg.is_init)
	{
		shm_unlink(glob_info->shm_data_reg.name);
		glob_info->shm_data_reg.is_init = FALSE;
	}
	
	if(glob_info->shm_update_reg.is_mapped)
	{
		shm_update_info->num_procs -= 1;
		munmap(shm_update_info, glob_info->shm_update_reg.reg_sz);
		glob_info->shm_update_reg.is_mapped = FALSE;
	}
	
	if(glob_info->shm_update_reg.is_init)
	{
		shm_unlink(glob_info->shm_update_reg.name);
		glob_info->shm_update_reg.is_init = FALSE;
	}
	
	if(glob_info->flags.is_proc_lock_init)
	{
		if(glob_info->flags.is_proc_locked)
		{
			releaseProcLock();
		}
		
		if(sem_close(glob_info->proc_lock) != 0)
		{
			readMXError("SemCloseInvalidError", "The sem argument is not a valid semaphore descriptor.");
		}
		glob_info->flags.is_proc_lock_init = FALSE;
	}
	
#endif
	
	mxFree(glob_info);
	glob_info = NULL;
	
	mexAtExit(nullfcn);
	
}


size_t pad_to_align(size_t size)
{
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	return size + align_size - (size & (align_size-1));
}


void make_mxmalloc_signature(uint8_t sig[MXMALLOC_SIG_LEN], size_t seg_size)
{
	/*
	 * MXMALLOC SIGNATURE INFO:
	 * 	mxMalloc adjusts the size of memory for 32 byte alignment (shifts either 16 or 32 bytes)
	 * 	it appends the following 16 byte header immediately before data segment
	 *
	 * HEADER:
	 * 		bytes 0-7 - size iterator, increases as (size/16+1)*16 mod 256
	 * 			byte 0 = (((size+16-1)/2^4)%2^4)*2^4
	 * 			byte 1 = (((size+16-1)/2^8)%2^8)
	 * 			byte 2 = (((size+16-1)/2^16)%2^16)
	 * 			byte n = (((size+16-1)/2^(2^(2+n)))%2^(2^(2+n)))
	 *
	 * 		bytes  8-11 - a signature for the vector check
	 * 		bytes 12-13 - the alignment (should be 32 bytes for new MATLAB)
	 * 		bytes 14-15 - the offset from the original pointer to the newly aligned pointer (should be 16 or 32)
	 */
	
	memcpy(sig, MXMALLOC_SIGNATURE, MXMALLOC_SIG_LEN);
	size_t multi = 1 << 4;
	
	/* note: (x % 2^n) == (x & (2^n - 1)) */
	sig[0] = (uint8_t)((((seg_size + 0x0F)/multi) & (multi - 1))*multi);
	
	/* note: this only does bits 1 to 4 because of 64 bit precision limit (maybe implement bit 5 in the future?)*/
	for(int i = 1; i < 5; i++)
	{
		multi = (size_t)1 << (1 << (2 + i));
		sig[i] = (uint8_t)(((seg_size + 0x0F)/multi) & (multi - 1));
	}
	
}


void acquireProcLock(void)
{
	/* only request a lock if there is more than one process */
	if(shm_update_info->num_procs > 1 && glob_info->flags.is_mem_safe)
	{
#ifdef MSH_WIN
		uint32_t ret = WaitForSingleObject(glob_info->proc_lock, INFINITE);
		if(ret == WAIT_ABANDONED)
		{
			readMXError("WaitProcLockAbandonedError", "One of the processes failed while using the lock. Cannot safely continue (Error number: %u).", GetLastError());
		}
		else if(ret == WAIT_FAILED)
		{
			readMXError("WaitProcLockFailedError", "The wait for process lock failed (Error number: %u).", GetLastError());
		}
#else
		if(sem_wait(glob_info->proc_lock) != 0)
		{
			switch(errno)
			{
				case EINVAL:
					readMXError("SemWaitInvalid", "The sem argument does not refer to a valid semaphore.");
				case ENOSYS:
					readMXError("SemWaitNotSupportedError", "The functions sem_wait() and sem_trywait() are not supported by this implementation.");
				case EDEADLK:
					readMXError("SemWaitDeadlockError", "A deadlock condition was detected.");
				case EINTR:
					readMXError("SemWaitInterruptError", "A signal interrupted this function.");
				default:
					readMXError("SemWaitUnknownError", "An unknown error occurred (Error number: %i)", errno);
			}
		}
#endif
		glob_info->flags.is_proc_locked = TRUE;
	}
}


void releaseProcLock(void)
{
#ifdef MSH_WIN
	/* not sure if this is required on windows, but it doesn't hurt */
	FlushViewOfFile(shm_update_info, glob_info->shm_update_reg.reg_sz);
	FlushViewOfFile(shm_data_ptr, glob_info->shm_data_reg.reg_sz);
#else
	/* yes, this is required to ensure the changes are written (mmap creates a virtual address space)
	 * no, I don't know how this is possible without doubling the actual amount of RAM needed */
	msync(shm_update_info, glob_info->shm_update_reg.reg_sz, MS_SYNC|MS_INVALIDATE);
	msync(shm_data_ptr, glob_info->shm_data_reg.reg_sz, MS_SYNC|MS_INVALIDATE);
#endif
	if(glob_info->flags.is_proc_locked)
	{

#ifdef MSH_WIN
		if(ReleaseMutex(glob_info->proc_lock) == 0)
		{
			readMXError("ReleaseMutexError", "The process lock release failed (Error number: %u).", GetLastError());
		}
#else
		if(sem_post(glob_info->proc_lock) != 0)
		{
			switch(errno)
			{
				case EINVAL:
					readMXError("SemPostInvalid", "The sem argument does not refer to a valid semaphore.");
				case ENOSYS:
					readMXError("SemPostError", "The function sem_post() is not supported by this implementation.");
				default:
					readMXError("SemPostUnknownError", "An unknown error occurred (Error number: %i)", errno);
			}
		}
#endif
		glob_info->flags.is_proc_locked = FALSE;
	}
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
		
		if(strcmp(dir_str, "share") == 0)
		{
			return msh_SHARE;
		}
		else if(strcmp(dir_str, "detach") == 0)
		{
			return msh_DETACH;
		}
		else if(strcmp(dir_str, "fetch") == 0)
		{
			return msh_FETCH;
		}
		else if(strcmp(dir_str, "deepcopy") == 0)
		{
			return msh_DEEPCOPY;
		}
		else if(strcmp(dir_str, "debug") == 0)
		{
			return msh_DEBUG;
		}
		else if(strcmp(dir_str, "clone") == 0)
		{
			return msh_SHARE;
		}
		else
		{
			readMXError("InvalidDirectiveError", "Directive not recognized.");
		}
		
	}
	else
	{
		readMXError("InvalidDirectiveError", "Directive must either be 'uint8' or 'char'.");
	}
	
	return msh_DEBUG;
	
}

/* checks if everything is in working order before doing the operation */
bool_t precheck(void)
{
	return glob_info->flags.is_proc_lock_init
			& glob_info->shm_update_reg.is_init
			& glob_info->shm_update_reg.is_mapped
			& glob_info->shm_data_reg.is_init
			& glob_info->shm_data_reg.is_mapped
			& glob_info->flags.is_glob_shm_var_init;
}

void makeDummyVar(mxArray** out)
{
	glob_shm_var = mxCreateDoubleMatrix(0, 0, mxREAL);
	mexMakeArrayPersistent(glob_shm_var);
	*out = mxCreateSharedDataCopy(glob_shm_var);
	glob_info->flags.is_glob_shm_var_init = TRUE;
}

void nullfcn(void)
{
	// does nothing
}

