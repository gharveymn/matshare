#include "headers/matshare_.h"
#include "headers/mshtypes.h"

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{

//	mexPrintf("%s\n", mexIsLocked()? "MEX is locked": "MEX is unlocked" );
	
	/* For inputs */
	const mxArray* mxDirective;			/* Directive {clone, attach, detach, free} */
	const mxArray* mxInput;           		/* Input array (for clone) */
	
	int num_params;
	
	/* hack */
	mxArrayStruct* arr;
	
	/* For storing inputs */
	msh_directive_t directive;
	
	size_t sm_size;					/* size required by the shared memory */
	
	/* for storing the mxArrays */
	header_t hdr;
	data_t dat;

#ifdef MSH_WIN
	DWORD hi_sz, lo_sz, err;
#endif
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		readErrorMex("NotEnoughInputsError", "Minimum input arguments missing; must supply a directive.");
	}
	
	/* assign inputs */
	mxDirective = prhs[0];
	
	/* get the directive */
	directive = parseDirective(mxDirective);
	
	char init_check_name[MSH_MAX_NAME_LEN];
#ifdef MSH_WIN
	snprintf(init_check_name, MSH_MAX_NAME_LEN, MSH_INIT_CHECK_NAME, GetProcessId(GetCurrentProcess()));
	HANDLE temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1, init_check_name);
	err = GetLastError();
	if(temp_handle == NULL)
	{
		readErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
	}
	else if(err != ERROR_ALREADY_EXISTS)
	{
		if(directive == msh_DETACH || directive == msh_OBJ_DEREGISTER)
		{
			if(CloseHandle(temp_handle) == 0)
			{
				readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
			}
			return;
		}
		
		/*then this is the process initializer (and maybe the global initializer; we'll find out later) */
		mexLock();
		init();
		g_info->init_seg.handle = temp_handle;
		memcpy(g_info->init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->init_seg.is_init = TRUE;
	}
	else
	{
		if(CloseHandle(temp_handle) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
		}
	}
#else
	snprintf(init_check_name, MSH_MAX_NAME_LEN, MSH_INIT_CHECK_NAME, getpid());
	int temp_handle;
	if((temp_handle = shm_open(init_check_name, O_RDWR|O_CREAT|O_EXCL, S_IRWXU))  == -1)
	{
		/* we want this to error if it does exist */
		if(errno != EEXIST)
		{
			readShmOpenError(errno);
		}
	}
	else
	{
		if(directive == msh_DETACH || directive == msh_OBJ_DEREGISTER)
		{
			if(shm_unlink(init_check_name) != 0)
			{
				readShmUnlinkError(errno);
			}
			return;
		}
		
		/*then this is the process initializer (and maybe the global initializer; we'll find out later) */
		mexLock();
		init();
		g_info->init_seg.handle = temp_handle;
		memcpy(g_info->init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->init_seg.is_init = TRUE;
	}
#endif
	
	if(directive != msh_DETACH && precheck() != TRUE)
	{
		readErrorMex("NotInitializedError", "At least one of the needed shared memory segments has not been initialized. Cannot continue.");
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
				readErrorMex("NoVariableError", "No variable supplied to clone.");
			}
			
			/* Assign */
			mxInput = prhs[1];
			
			if(!(mxIsNumeric(mxInput) || mxIsLogical(mxInput) || mxIsChar(mxInput) || mxIsStruct(mxInput) || mxIsCell(mxInput)))
			{
				readErrorMex("InvalidTypeError", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
			}
			
			acquireProcLock();
			
			/* clear the previous variable if needed */
			if(!mxIsEmpty(g_shm_var))
			{
				/* if the current shared variable shares all the same dimensions, etc. then just copy over the memory */
				if(shmCompareSize(shm_data_ptr, mxInput, &sm_size) == TRUE)
				{
					/* DON'T INCREMENT THE REVISION NUMBER */
					/* this is an in-place change, so everyone is still fine */
					
					/* do the rewrite after checking because the comparison is cheap */
					shmRewrite(shm_data_ptr, mxInput);
					
					/* tell everyone to come back to this segment */
					updateAll();
					
					releaseProcLock();
					
					/* BREAK DON'T DO ANYTHING ELSE */
					break;
				}
				else
				{
					/* otherwise we'll detach and start over */
					shmDetach(g_shm_var);
				}
			}
			mxDestroyArray(g_shm_var);
			g_info->flags.is_glob_shm_var_init = FALSE;
			
			/* scan input data */
			sm_size = shmScan(&hdr, &dat, mxInput, NULL, &g_shm_var);
			
			/* update the revision number and indicate our info is current */
			g_info->cur_seg_info.rev_num = shm_update_info->rev_num + 1;
			if(sm_size > g_info->shm_data_seg.seg_sz)
			{
				
				/* update the current map number if we can't reuse the current segment */
				/* else we reuse the current segment */
				
				/* create a unique new segment */
				g_info->cur_seg_info.seg_num = shm_update_info->lead_seg_num + 1;
				
#ifdef MSH_WIN
				
				/* decrement the kernel handle count and tell onExit not to do this twice */
				if(UnmapViewOfFile(shm_data_ptr) == 0)
				{
					err = GetLastError();
					releaseProcLock();
					readErrorMex("UnmapFileError", "Error unmapping the file (Error Number %u).", err);
				}
				g_info->shm_data_seg.is_mapped = FALSE;
				
				if(CloseHandle(g_info->shm_data_seg.handle) == 0)
				{
					err = GetLastError();
					releaseProcLock();
					readErrorMex("CloseHandleError", "Error closing the file handle (Error Number %u).", err);
				}
				g_info->shm_data_seg.is_init = FALSE;
				
				/* change the map size */
				g_info->shm_data_seg.seg_sz = sm_size;
				
				/* change the file name */
				snprintf(g_info->shm_data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, (unsigned long long)g_info->cur_seg_info.seg_num);
				
				/* create the new mapping */
				lo_sz = (DWORD)(g_info->shm_data_seg.seg_sz & 0xFFFFFFFFL);
				hi_sz = (DWORD)((g_info->shm_data_seg.seg_sz >> 32) & 0xFFFFFFFFL);
				g_info->shm_data_seg.handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, g_info->shm_data_seg.name);
				if(g_info->shm_data_seg.handle == NULL)
				{
					/* throw error if it already exists because that shouldn't happen */
					err = GetLastError();
					releaseProcLock();
					readErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
				}
				g_info->shm_data_seg.is_init = TRUE;
				
				g_info->shm_data_seg.ptr = MapViewOfFile(g_info->shm_data_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_info->shm_data_seg.seg_sz);
				if(g_info->shm_data_seg.ptr == NULL)
				{
					err = GetLastError();
					releaseProcLock();
					readErrorMex("MapDataSegError", "Could not map the data memory segment (Error number %u)", err);
				}
				g_info->shm_data_seg.is_mapped = TRUE;
				
#else
				
				/* unmap in preparation for size change */
				if(munmap(shm_data_ptr, g_info->shm_data_seg.seg_sz) != 0)
				{
					releaseProcLock();
					readMunmapError(errno);
				}
				g_info->shm_data_seg.is_mapped = FALSE;
				
				g_info->shm_data_seg.seg_sz = sm_size;
				
				/* change the map size */
				if(ftruncate(g_info->shm_data_seg.handle, g_info->shm_data_seg.seg_sz) != 0)
				{
					releaseProcLock();
					readFtruncateError(errno);
				}
				
				/* remap the shared memory */
				g_info->shm_data_seg.ptr = mmap(NULL, g_info->shm_data_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_info->shm_data_seg.handle, 0);
				if(shm_data_ptr == MAP_FAILED)
				{
					releaseProcLock();
					readMmapError(errno);
				}
				g_info->shm_data_seg.is_mapped = TRUE;
#endif
				
				/* warning: this may cause a collision in the rare case where one process is still at seg_num == 0
				 * and lead_seg_num == UINT64_MAX; very unlikely, so the overhead isn't worth it */
				shm_update_info->lead_seg_num = g_info->cur_seg_info.seg_num;
				
			}
			
			/* copy data to the shared memory */
			shmCopy(&hdr, &dat, shm_data_ptr, NULL, g_shm_var);
			
			/* free temporary allocation */
			freeTmp(&dat);
			
			mexMakeArrayPersistent(g_shm_var);
			g_info->flags.is_glob_shm_var_init = TRUE;
			
			/* make sure all of above is actually done before telling everyone to move to the new segment */
			updateAll();
			
			releaseProcLock();
			
			break;
			
		case msh_FETCH:
			
			acquireProcLock();
			
			/* check if the current revision number is the same as the shm revision number */
			if(g_info->cur_seg_info.rev_num != shm_update_info->rev_num && g_info->this_pid != shm_update_info->upd_pid)
			{
				//do a detach operation
				if(!mxIsEmpty(g_shm_var))
				{
					/* NULL all of the Matlab pointers */
					shmDetach(g_shm_var);
				}
				mxDestroyArray(g_shm_var);
				
				g_info->cur_seg_info.rev_num = shm_update_info->rev_num;
				
				/* Update the mapping if this is on a new segment */
				if(g_info->cur_seg_info.seg_num != shm_update_info->seg_num)
				{
					g_info->cur_seg_info.seg_num = shm_update_info->seg_num;
					
#ifdef MSH_WIN
					
					if(UnmapViewOfFile(shm_data_ptr) == 0)
					{
						makeDummyVar(&plhs[0]);
						releaseProcLock();
						readErrorMex("UnmapFileError", "Error unmapping the file (Error Number %u)", err);
					}
					g_info->shm_data_seg.is_mapped = FALSE;
					
					if(CloseHandle(g_info->shm_data_seg.handle) == 0)
					{
						makeDummyVar(&plhs[0]);
						releaseProcLock();
						readErrorMex("CloseHandleError", "Error closing the file handle (Error Number %u)", err);
					}
					g_info->shm_data_seg.is_init = FALSE;
					
					/* update the size */
					g_info->shm_data_seg.seg_sz = shm_update_info->seg_sz;
					
					/* update the region name */
					snprintf(g_info->shm_data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, (unsigned long long)g_info->cur_seg_info.seg_num);
					
					g_info->shm_data_seg.handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, g_info->shm_data_seg.name);
					err = GetLastError();
					if(g_info->shm_data_seg.handle == NULL)
					{
						makeDummyVar(&plhs[0]);
						releaseProcLock();
						readErrorMex("OpenFileError", "Error opening the file mapping (Error Number %u)", err);
					}
					g_info->shm_data_seg.is_init = TRUE;
					
					g_info->shm_data_seg.ptr = MapViewOfFile(g_info->shm_data_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0, g_info->shm_data_seg.seg_sz);
					err = GetLastError();
					if(shm_data_ptr == NULL)
					{
						makeDummyVar(&plhs[0]);
						releaseProcLock();
						readErrorMex("MappingError", "Could not fetch the memory segment (Error number: %d).", err);
					}
					g_info->shm_data_seg.is_mapped = TRUE;
					
#else
					
					/* unmap the current mapping */
					if(munmap(shm_data_ptr, g_info->shm_data_seg.seg_sz) != 0)
					{
						releaseProcLock();
						readMunmapError(errno);
					}
					g_info->shm_data_seg.is_mapped = FALSE;
					
					/* update the size */
					g_info->shm_data_seg.seg_sz = shm_update_info->seg_sz;
					
					/* remap with the updated size */
					g_info->shm_data_seg.ptr = mmap(NULL, g_info->shm_data_seg.seg_sz, PROT_READ|PROT_WRITE, MAP_SHARED, g_info->shm_data_seg.handle, 0);
					if(shm_data_ptr == MAP_FAILED)
					{
						releaseProcLock();
						readMmapError(errno);
					}
					g_info->shm_data_seg.is_mapped = TRUE;
#endif
				
				}
				
				shmFetch(shm_data_ptr, &g_shm_var);
				
				mexMakeArrayPersistent(g_shm_var);
				
			}
			
			plhs[0] = mxCreateSharedDataCopy(g_shm_var);
			
			releaseProcLock();
			
			break;
		
		case msh_OBJ_DEREGISTER:
			/* deregister an object tracking this function */
			g_info->num_lcl_objs -= 1;
			/* fall through to check if we should clear everything */
		case msh_DETACH:
			/********************/
			/*	Dettach case	*/
			/********************/
			
			/* this only actually frees if it is the last object using the function in the current process */
			if(g_info->num_lcl_objs == 0)
			{
				onExit();
				mexUnlock();
			}
			
			break;
			
		case msh_PARAM:
			/* set parameters for matshare to use */
			
			num_params = nrhs - 1;
			
			if(num_params % 2 != 0)
			{
				readErrorMex("InvalNumArgsError", "The number of parameters input must be a multiple of two.");
			}
			
			parseParams(num_params, prhs + 1);
			
			break;
		
		case msh_DEEPCOPY:
			plhs[0] = mxDuplicateArray(g_shm_var);
			break;
		case msh_DEBUG:
			arr = (mxArrayStruct*)g_shm_var;
			mexPrintf("%d\n", arr->RefCount);
			break;
		case msh_OBJ_REGISTER:
			/* tell matshare to register a new object */
			
			g_info->num_lcl_objs += 1;
			break;
		default:
			readErrorMex("UnknownDirectiveError", "Unrecognized directive.");
			break;
	}


}


/* ------------------------------------------------------------------------- */
/* shmDetach                                                                */
/*                                                                           */
/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
/*                                                                           */
/* Arguments:                                                                */
/*    Input matrix to remove references.                                     */
/* Returns:                                                                  */
/*    Pointer to start of shared memory segment.                             */
/* ------------------------------------------------------------------------- */
void shmDetach(mxArray* ret_var)
{
	
	/* uses side-effects! */
	size_t num_dims = 0;
	mwSize null_dims[] = {0, 0};
	void* new_pr = NULL, * new_pi = NULL;
	void* new_ir = NULL, * new_jc = NULL;
	mwSize nzmax = 0;
	mwIndex i, n;
	
	/*for structure */
	int num_fields, j;
	
	/* restore matlab  memory */
	if(ret_var == (mxArray*)NULL || mxIsEmpty(ret_var))
	{
		return;
	}
	else if(mxIsStruct(ret_var))
	{
		/* detach each field for each element */
		n = mxGetNumberOfElements(ret_var);
		num_fields = mxGetNumberOfFields(ret_var);
		for(i = 0; i < n; i++)                         /* element */
		{
			for(j = 0; j < num_fields; j++)               /* field */
			{
				shmDetach(mxGetFieldByNumber(ret_var, i, j));
			}/* detach this one */
		}
	}
	else if(mxIsCell(ret_var))
	{
		/* detach each cell */
		n = mxGetNumberOfElements(ret_var);
		for(i = 0; i < n; i++)
		{
			shmDetach(mxGetCell(ret_var, i));
		}/* detach this one */
		
	}
	else if(mxIsNumeric(ret_var) || mxIsLogical(ret_var) || mxIsChar(ret_var))  /* a matrix containing data */
	{
		
		/* handle sparse objects */
		if(mxIsSparse(ret_var))
		{
			/* I don't seem to be able to give sparse arrays zero size so (num_elems must be 1) */
			num_dims = 2;
			nzmax = 1;
			if(mxSetDimensions(ret_var, null_dims, num_dims))
			{
				/* error for debugging---this is kinda iffy */
				releaseProcLock();
				readErrorMex("DetachDimensionsError", "Unable to resize the array.");
			}
			
			mxSetNzmax(ret_var, nzmax);
			
			/* allocate 1 element */
			new_pr = mxCalloc(nzmax, mxGetElementSize(ret_var));
			mexMakeMemoryPersistent(new_pr);
			mxSetData(ret_var, new_pr);
			if(mxIsComplex(ret_var))
			{
				new_pi = mxCalloc(nzmax, mxGetElementSize(ret_var));
				mexMakeMemoryPersistent(new_pi);
				mxSetImagData(ret_var, new_pi);
			}
			
			new_ir = mxCalloc(nzmax, sizeof(mwIndex));
			mexMakeMemoryPersistent(new_ir);
			mxSetIr(ret_var, new_ir);
			
			new_jc = mxCalloc(null_dims[1] + 1, sizeof(mwIndex));
			mexMakeMemoryPersistent(new_jc);
			mxSetJc(ret_var, new_jc);
			
		}
		else
		{
			mxSetDimensions(ret_var, null_dims, num_dims);
			mxSetData(ret_var, new_pr);
			if(mxIsComplex(ret_var))
			{
				mxSetImagData(ret_var, new_pi);
			}
		}
		
		/** HACK **/
		/* reset all the crosslinks so nothing in MATLAB is pointing to shared data (which will be gone soon) */
		mxArray* link;
		for(link = ((mxArrayStruct*)ret_var)->CrossLink; link != NULL && link != ret_var; link = ((mxArrayStruct*)link)->CrossLink)
		{
			mxSetDimensions(link, null_dims, num_dims);
			mxSetData(link, new_pr);
			if(mxIsComplex(link))
			{
				mxSetImagData(link, new_pi);
			}
			
			if(mxIsSparse(link))
			{
				mxSetNzmax(ret_var, nzmax);
				mxSetIr(link, new_ir);
				mxSetJc(link, new_jc);
			}
		}
		
	}
	else
	{
		releaseProcLock();
		readErrorMex("InvalidTypeError", "Unsupported type. The segment may have been corrupted.");
	}
}


size_t shmFetch(byte_t* shm_anchor, mxArray** ret_var)
{
	
	byte_t* shm = shm_anchor;
	
	/* for working with shared memory ... */
	size_t i;
	mxArray* ret_child;
	
	/* for working with payload ... */
	header_t* hdr;
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		if(hdr->is_empty)
		{
			/* make this a separate case since there are no field names */
			*ret_var = mxCreateStructArray(hdr->num_dims, (mwSize*)(shm_anchor + hdr->data_off.dims), hdr->num_fields, NULL);
		}
		else
		{
			char_t** field_names = (char_t**)mxMalloc(hdr->num_fields * sizeof(char_t*));
			pointCharArrayAtString(field_names, shm_anchor + hdr->data_off.field_str, hdr->num_fields);
			
			*ret_var = mxCreateStructArray(hdr->num_dims, (mwSize*)(shm_anchor + hdr->data_off.dims), hdr->num_fields, (const char_t**)field_names);
			
			shm = shm_anchor + hdr->data_off.child_hdr;
			/* Go through each element */
			for(i = 0; i < hdr->num_elems; i++)
			{
				for(field_num = 0; field_num < hdr->num_fields; field_num++)     /* each field */
				{
					shm += shmFetch(shm, &ret_child);
					mxSetFieldByNumber(*ret_var, i, field_num, ret_child);
				}
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		*ret_var = mxCreateCellArray(hdr->num_dims, (mwSize*)(shm_anchor + hdr->data_off.dims));
		shm = shm_anchor + hdr->data_off.child_hdr;
		for(i = 0; i < hdr->num_elems; i++)
		{
			shm += shmFetch(shm, &ret_child);
			mxSetCell(*ret_var, i, ret_child);
		}
	}
	else     /*base case*/
	{
		
		if(hdr->is_sparse)
		{
			
			if(hdr->is_numeric)
			{
				*ret_var = mxCreateSparse(0, 0, 1, hdr->complexity);
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateSparseLogicalMatrix(0, 0, 1);
			}
			else
			{
				releaseProcLock();
				readErrorMex("UnrecognizedTypeError", "The fetched array was of class 'sparse' but not of type 'numeric' or 'logical'");
			}
			
			if(!hdr->is_empty)
			{
				/* set the real and imaginary data */
				mxFree(mxGetData(*ret_var));
				mxSetData(*ret_var, shm_anchor + hdr->data_off.pr);
				if(hdr->complexity)
				{
					mxFree(mxGetImagData(*ret_var));
					mxSetImagData(*ret_var, shm_anchor + hdr->data_off.pi);
				}
			}
			
			/* set the pointers relating to sparse */
			mxFree(mxGetIr(*ret_var));
			mxSetIr(*ret_var, (mwIndex*)(shm_anchor + hdr->data_off.ir));
			
			mxFree(mxGetJc(*ret_var));
			mxSetJc(*ret_var, (mwIndex*)(shm_anchor + hdr->data_off.jc));
			
			mxSetNzmax(*ret_var, hdr->num_elems);
			mxSetDimensions(*ret_var, (mwSize*)(shm_anchor + hdr->data_off.dims), hdr->num_dims);
			
		}
		else
		{
			
			if(hdr->is_empty)
			{
				if(hdr->is_numeric)
				{
					*ret_var = mxCreateNumericArray(hdr->num_dims, (mwSize*)(shm_anchor + hdr->data_off.dims), hdr->classid, hdr->complexity);
				}
				else if(hdr->classid == mxLOGICAL_CLASS)
				{
					*ret_var = mxCreateLogicalArray(hdr->num_dims, (mwSize*)(shm_anchor + hdr->data_off.dims));
				}
				else
				{
					*ret_var = mxCreateCharArray(hdr->num_dims, (mwSize*)(shm_anchor + hdr->data_off.dims));
				}
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
				
				/* set the real and imaginary data */
				mxSetData(*ret_var, shm_anchor + hdr->data_off.pr);
				if(hdr->complexity)
				{
					mxSetImagData(*ret_var, shm_anchor + hdr->data_off.pi);
				}
				
				mxSetDimensions(*ret_var, (mwSize*)(shm_anchor + hdr->data_off.dims), hdr->num_dims);
				
			}
			
		}
		
	}
	return hdr->shm_sz;
}


bool_t shmCompareSize(byte_t* shm_anchor, const mxArray* comp_var, size_t* offset)
{
	
	byte_t* shm = shm_anchor;
	
	/* for working with shared memory ... */
	size_t i, shmshift;
	
	/* for working with payload ... */
	header_t* hdr;
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	if(hdr->num_dims != mxGetNumberOfDimensions(comp_var)
	   || memcmp((mwSize*)(shm_anchor + hdr->data_off.dims), mxGetDimensions(comp_var), sizeof(mwSize)*hdr->num_dims) != 0)
	{
		return FALSE;
	}
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		char_t** field_names = (char_t**)mxCalloc((size_t)hdr->num_fields, sizeof(char_t*));
		pointCharArrayAtString(field_names, shm_anchor + hdr->data_off.field_str, hdr->num_fields);
		
		
		shm = shm_anchor + hdr->data_off.child_hdr;
		/* Go through each element */
		for(i = 0; i < hdr->num_elems; i++)
		{
			for(field_num = 0, shmshift = 0; field_num < hdr->num_fields; field_num++, shmshift = 0)     /* each field */
			{
				if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_names[field_num]) != 0)
				{
					return FALSE;
				}
				
				
				if(!shmCompareSize(shm, mxGetFieldByNumber(comp_var, i, field_num), &shmshift))
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
		
		shm = shm_anchor + hdr->data_off.child_hdr;
		for(i = 0, shmshift = 0; i < hdr->num_elems; i++, shmshift = 0)
		{
			if(!shmCompareSize(shm, mxGetCell(comp_var, i), &shmshift))
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
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity != mxIsComplex(comp_var))
		{
			return FALSE;
		}
		
		if(hdr->is_sparse)
		{
			if(mxIsSparse(comp_var) != TRUE)
			{
				return FALSE;
			}

			if(hdr->num_elems != mxGetNzmax(comp_var)
			   || ((mwSize*)(shm_anchor + hdr->data_off.dims))[1] != mxGetN(comp_var))
			{
				return FALSE;
			}
			
		
		}
		else
		{
			if(mxIsSparse(comp_var) == TRUE)
			{
				return FALSE;
			}

			if(hdr->num_elems != mxGetNumberOfElements(comp_var))
			{
				return FALSE;
			}
		}
		
	}
	
	*offset = hdr->shm_sz;
	return TRUE;
}


size_t shmRewrite(byte_t* shm_anchor, const mxArray* in_var)
{
	byte_t* shm;
	
	/* for working with shared memory ... */
	size_t i;
	
	/* for working with payload ... */
	header_t* hdr;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm_anchor;
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		shm = shm_anchor + hdr->data_off.child_hdr;
		
		/* Go through each element */
		for(i = 0; i < hdr->num_elems; i++)
		{
			for(field_num = 0; field_num < hdr->num_fields; field_num++)     /* each field */
			{
				/* And fill it */
				shm += shmRewrite(shm, mxGetFieldByNumber(in_var, i, field_num));
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		shm = shm_anchor + hdr->data_off.child_hdr;
		
		for(i = 0; i < hdr->num_elems; i++)
		{
			/* And fill it */
			shm += shmRewrite(shm, mxGetCell(in_var, i));
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		memcpy(shm_anchor + hdr->data_off.pr, mxGetData(in_var), (hdr->num_elems)*(hdr->elem_size));
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			memcpy(shm_anchor + hdr->data_off.pi, mxGetImagData(in_var), (hdr->num_elems)*(hdr->elem_size));
		}
		
		/* if sparse get a list of the elements */
		if(hdr->is_sparse)
		{
			memcpy(shm_anchor + hdr->data_off.ir, mxGetIr(in_var), (hdr->num_elems)*sizeof(mwIndex));
			memcpy(shm_anchor + hdr->data_off.jc, mxGetJc(in_var), (((mwSize*)(shm_anchor + hdr->data_off.dims))[1] + 1)*sizeof(mwIndex));
		}
		
		
		
	}
	return hdr->shm_sz;
}


/* ------------------------------------------------------------------------- */
/* shmScan                                                                  */
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
size_t shmScan(header_t* hdr, data_t* dat, const mxArray* mxInput, header_t* par_hdr, mxArray** ret_var)
{
	/* counter */
	size_t i, count;
	mxArray* ret_child;
	
	/* for structures */
	int field_num;
	
	/* initialize data info; _possibly_ update these later */
	dat->dims = NULL;
	dat->pr = NULL;
	dat->pi = NULL;
	dat->ir = NULL;
	dat->jc = NULL;
	dat->num_children = 0;
	dat->child_dat = NULL;
	dat->child_hdr = NULL;
	dat->field_str = NULL;
	
	if(mxInput == NULL)
	{
		releaseProcLock();
		readErrorMex("UnexpectedError", "Input variable was unexpectedly NULL.");
	}
	
	/* initialize header info */
	
	/* these are updated when the data is copied over */
	hdr->data_off.dims = SIZE_MAX;
	hdr->data_off.pr = SIZE_MAX;
	hdr->data_off.pi = SIZE_MAX;
	hdr->data_off.ir = SIZE_MAX;
	hdr->data_off.jc = SIZE_MAX;
	hdr->data_off.field_str = SIZE_MAX;
	hdr->data_off.child_hdr = SIZE_MAX;
	hdr->data_off.parent_hdr = INT_MAX;
	
	hdr->is_numeric = mxIsNumeric(mxInput);
	hdr->is_sparse = mxIsSparse(mxInput);
	hdr->is_empty = mxIsEmpty(mxInput);
	hdr->complexity = mxIsComplex(mxInput)? mxCOMPLEX : mxREAL;
	hdr->classid = mxGetClassID(mxInput);
	hdr->num_dims = mxGetNumberOfDimensions(mxInput);
	hdr->elem_size = mxGetElementSize(mxInput);
	hdr->num_elems = mxGetNumberOfElements(mxInput);      /* update this later on sparse*/
	hdr->num_fields = 0;                                 /* update this later */
	hdr->shm_sz = padToAlign(sizeof(header_t));     /* update this later */
	hdr->str_sz = 0;                                   /* update this later */
	
	/* copy the size */
	dat->dims = (mwSize*)mxGetDimensions(mxInput);     /* some abuse of const for now, fix on deep copy*/
	
	/* Add space for the dimensions */
	hdr->shm_sz += padToAlign(hdr->num_dims * sizeof(mwSize));
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		if(hdr->is_empty)
		{
			if(hdr->num_fields)
			{
				releaseProcLock();
				readErrorMex("UnexpectedError", "An empty struct array unexpectedly had more than one field. This is undefined behavior.");
			}
			*ret_var = mxCreateStructArray(hdr->num_dims, dat->dims, hdr->num_fields, NULL);
		}
		else
		{
			
			/* How many fields to work with? */
			hdr->num_fields = mxGetNumberOfFields(mxInput);
			
			/* Find the size required to store the field names */
			hdr->str_sz = (size_t) fieldNamesSize(mxInput);     /* always a multiple of alignment size */
			hdr->shm_sz += padToAlign(hdr->str_sz);                   /* Add space for the field string */
			
			/* use mxMalloc so mem is freed on error via garbage collection */
			dat->num_children = hdr->num_elems*hdr->num_fields;
			dat->child_hdr = mxMalloc(dat->num_children*sizeof(header_t*)); /* extra space for the field string */
			dat->child_dat = mxMalloc(dat->num_children*sizeof(data_t*));
			dat->field_str = mxMalloc(hdr->str_sz*sizeof(char));
			
			const char_t** field_names = mxMalloc(hdr->num_fields*sizeof(char_t*));
			
			/* make a record of the field names */
			copyFieldNames(mxInput, dat->field_str, field_names);
			
			*ret_var = mxCreateStructArray(hdr->num_dims, dat->dims, hdr->num_fields, field_names);
			
			mxFree((void*)field_names);
			
			/* go through each recursively */
			count = 0;
			for(i = 0; i < hdr->num_elems; i++)                         /* each element */
			{
				for(field_num = 0; field_num < hdr->num_fields; field_num++)     /* each field */
				{
					dat->child_hdr[count] = mxMalloc(sizeof(header_t));
					dat->child_dat[count] = mxMalloc(sizeof(data_t));
					
					/* call recursivley */
					hdr->shm_sz += shmScan(dat->child_hdr[count], dat->child_dat[count],
									   mxGetFieldByNumber(mxInput, i, field_num), hdr, &ret_child);
					mxSetFieldByNumber(*ret_var, i, field_num, ret_child);
					
					count++; /* progress */
				}
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		
		if(hdr->is_empty)
		{
			*ret_var = mxCreateCellArray(hdr->num_dims, dat->dims);
		}
		else
		{
			
			/* use mxMalloc so mem is freed on error via garbage collection */
			dat->num_children = hdr->num_elems;
			dat->child_hdr = mxMalloc(dat->num_children * sizeof(header_t*));
			dat->child_dat = mxMalloc(dat->num_children * sizeof(data_t*));
			
			*ret_var = mxCreateCellArray(hdr->num_dims, dat->dims);
			
			/* go through each recursively */
			for(i = 0; i < hdr->num_elems; i++)
			{
				dat->child_hdr[i] = mxMalloc(sizeof(header_t));
				dat->child_dat[i] = mxMalloc(sizeof(data_t));
				hdr->shm_sz += shmScan(dat->child_hdr[i], dat->child_dat[i], mxGetCell(mxInput, i), hdr,
								   &ret_child);
				mxSetCell(*ret_var, i, ret_child);
			}
		}
	}
	else if(hdr->is_numeric || hdr->classid == mxLOGICAL_CLASS || hdr->classid == mxCHAR_CLASS)  /* a matrix containing data *//* base case */
	{
		dat->pr = mxGetData(mxInput);
		dat->pi = mxGetImagData(mxInput);
		
		if(hdr->is_sparse)
		{
			/* len(pr)=num_elems, len(ir)=num_elems, len(jc)=colc+1 */
			hdr->num_elems = (size_t)mxGetNzmax(mxInput);
			dat->ir = mxGetIr(mxInput);
			dat->jc = mxGetJc(mxInput);
			
			hdr->shm_sz += padToAlign(MXMALLOC_SIG_LEN);
			hdr->shm_sz += padToAlign(sizeof(mwIndex) * (hdr->num_elems));      /* ensure both pointers are aligned individually */
			hdr->shm_sz += padToAlign(MXMALLOC_SIG_LEN);
			hdr->shm_sz += padToAlign(sizeof(mwIndex) * (dat->dims[1] + 1));
			
			if(hdr->is_numeric)
			{
				*ret_var = mxCreateSparse(0, 0, 1, hdr->complexity);
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				*ret_var = mxCreateSparseLogicalMatrix(0, 0, 1);
			}
			else
			{
				releaseProcLock();
				readErrorMex("UnrecognizedTypeError", "The fetched array was of class 'sparse' but not of type 'numeric' or 'logical'");
			}
			
		}
		else
		{
			
			if(hdr->is_empty)
			{
				if(hdr->is_numeric)
				{
					*ret_var = mxCreateNumericArray(hdr->num_dims, dat->dims, hdr->classid, hdr->complexity);
				}
				else if(hdr->classid == mxLOGICAL_CLASS)
				{
					*ret_var = mxCreateLogicalArray(hdr->num_dims, dat->dims);
				}
				else
				{
					*ret_var = mxCreateCharArray(hdr->num_dims, dat->dims);
				}
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
		}
		
		/* ensure both pointers are aligned individually */
		hdr->shm_sz += padToAlign(MXMALLOC_SIG_LEN);
		hdr->shm_sz += padToAlign(hdr->elem_size * hdr->num_elems);
		if(hdr->complexity == mxCOMPLEX)
		{
			hdr->shm_sz += padToAlign(MXMALLOC_SIG_LEN);
			hdr->shm_sz += padToAlign(hdr->elem_size * hdr->num_elems);
		}
	}
	else
	{
		releaseProcLock();
		readErrorMex("Internal:UnexpectedError", "Tried to clone an unsupported type.");
	}
	
	return hdr->shm_sz;
}


/* ------------------------------------------------------------------------- */
/* shmCopy                                                                  */
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
void shmCopy(header_t* hdr, data_t* dat, byte_t* shm_anchor, header_t* par_hdr, mxArray* ret_var)
{
	byte_t* shm = shm_anchor;
	
	size_t i, cpy_sz, total_offset = 0, shift, count;
	
	/* load up the shared memory */
	
	/* copy the header */
	cpy_sz = sizeof(header_t);
	shift = padToAlign(cpy_sz);
	total_offset += shift;
	shm += shift;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* copy the dimensions */
	hdr->data_off.dims = total_offset;
	for(i = 0; i < hdr->num_dims; i++)
	{
		((mwSize*)shm)[i] = dat->dims[i];
	}
	shift = padToAlign(hdr->num_dims * sizeof(mwSize));
	total_offset += shift;
	shm += shift;
	
	/* assign the parent header  */
	hdr->data_off.parent_hdr = (NULL == par_hdr)? 0 : (int)(par_hdr - (header_t*)shm_anchor);
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		/* place the field names next in shared memory */
		
		/* copy the field string */
		hdr->data_off.field_str = total_offset;
		for(i = 0; i < hdr->str_sz; i++)
		{
			((char_t*)shm)[i] = dat->field_str[i];
		}
		shift = padToAlign(hdr->str_sz);
		total_offset += shift;
		shm += shift;
		
		hdr->data_off.child_hdr = total_offset;
		
		/* copy the children recursively */
		count = 0;
		for(i = 0; i < hdr->num_elems; i++)
		{
			for(field_num = 0; field_num < hdr->num_fields; field_num++)     /* each field */
			{
				/* And fill it */
				shmCopy(dat->child_hdr[count], dat->child_dat[count], shm, hdr,
					   mxGetFieldByNumber(ret_var, i, field_num));
				shift = (dat->child_hdr[count])->shm_sz;
				total_offset += shift;
				shm += shift;
				count++;
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		hdr->data_off.child_hdr = total_offset;
		/* recurse for each cell element */
		for(i = 0; i < hdr->num_elems; i++)
		{
			shmCopy(dat->child_hdr[i], dat->child_dat[i], shm, hdr, mxGetCell(ret_var, i));
			shift = (dat->child_hdr[i])->shm_sz;
			total_offset += shift;
			shm += shift;
		}
	}
	else /* base case */
	{
		/* copy real data */
		cpy_sz = (hdr->num_elems)*(hdr->elem_size);
		shift = memCpyMex(shm, dat->pr, &hdr->data_off.pr, total_offset, cpy_sz);
		total_offset += shift;
		shm += shift;
		
		if(!hdr->is_empty)
		{
			mxFree(mxGetData(ret_var));
			mxSetData(ret_var, shm_anchor + hdr->data_off.pr);
		}
		
		/* copy complex data as well */
		if(hdr->complexity == mxCOMPLEX)
		{
			
			shift = memCpyMex(shm, dat->pi, &hdr->data_off.pi, total_offset, cpy_sz);
			total_offset += shift;
			shm += shift;
			
			if(!hdr->is_empty)
			{
				mxFree(mxGetImagData(ret_var));
				mxSetImagData(ret_var, shm_anchor + hdr->data_off.pi);
			}
			
		}
		
		/* and the indices of the sparse data as well */
		if(hdr->is_sparse)
		{
			cpy_sz = hdr->num_elems*sizeof(mwIndex);
			shift = memCpyMex(shm, (byte_t*)dat->ir, &hdr->data_off.ir, total_offset, cpy_sz);
			total_offset += shift;
			shm += shift;
			
			cpy_sz = (((mwSize*)(shm_anchor + hdr->data_off.dims))[1] + 1)*sizeof(mwIndex);
			shift = memCpyMex(shm, (byte_t*)dat->jc, &hdr->data_off.jc, total_offset, cpy_sz);
			total_offset += shift;
			shm += shift;
			
			/* set the pointers relating to sparse (set the real and imaginary data later)*/
			mxFree(mxGetIr(ret_var));
			mxSetIr(ret_var, ((mwIndex*)(shm_anchor + hdr->data_off.ir)));
			
			mxFree(mxGetJc(ret_var));
			mxSetJc(ret_var, ((mwIndex*)(shm_anchor + hdr->data_off.jc)));
			
			mxSetNzmax(ret_var, hdr->num_elems);
			
		}
		
		mxSetDimensions(ret_var, ((mwSize*)(shm_anchor + hdr->data_off.dims)), hdr->num_dims);
		
	}
	
	memcpy(shm_anchor, hdr, sizeof(header_t));
	
}


mxLogical shmCompareContent(byte_t* shm_anchor, const mxArray* comp_var, size_t* offset)
{
	
	byte_t* shm = shm_anchor;
	
	/* for working with shared memory ... */
	size_t i, shmshift;
	
	/* for working with payload ... */
	header_t* hdr;
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (header_t*)shm;
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	if(hdr->num_dims != mxGetNumberOfDimensions(comp_var)
	   || memcmp(((mwSize*)(shm_anchor + hdr->data_off.dims)), mxGetDimensions(comp_var), sizeof(mwSize)*hdr->num_dims) != 0
	   || hdr->num_elems != mxGetNumberOfElements(comp_var))
	{
		return FALSE;
	}
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		char_t** field_names = (char_t**)mxCalloc((size_t)hdr->num_fields, sizeof(char_t*));
		pointCharArrayAtString(field_names, shm, hdr->num_fields);
		
		shm = shm_anchor + hdr->data_off.child_hdr;
		
		/* Go through each element */
		for(i = 0; i < hdr->num_elems; i++)
		{
			for(field_num = 0; field_num < hdr->num_fields; field_num++)     /* each field */
			{
				
				if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_names[field_num]) != 0)
				{
					return FALSE;
				}
				
				if(!shmCompareContent(shm, mxGetFieldByNumber(comp_var, i, field_num), &shmshift))
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
		
		shm = shm_anchor + hdr->data_off.child_hdr;
		
		for(i = 0, shmshift = 0; i < hdr->num_elems; i++, shmshift = 0)
		{
			if(!shmCompareContent(shm, mxGetCell(comp_var, i), &shmshift))
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
		
		//these should point to the same location
		if(shm_anchor + hdr->data_off.pr != mxGetData(comp_var))
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
			if(shm_anchor + hdr->data_off.pi != mxGetImagData(comp_var))
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
			
			if((mwIndex*)(shm_anchor + hdr->data_off.ir) != mxGetIr(comp_var)
			   || (mwIndex*)(shm_anchor + hdr->data_off.jc) != mxGetJc(comp_var))
			{
				return FALSE;
			}
			
		}
		
	}
	*offset = hdr->shm_sz;
	return TRUE;
}




