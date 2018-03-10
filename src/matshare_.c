#include "headers/matshare_.h"

/* ------------------------------------------------------------------------- */
/* Matlab gateway function                                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{

//	mexPrintf("%s\n", mexIsLocked()? "MEX is locked": "MEX is unlocked" );
	
	int num_params;
	
	/* For inputs */
	const mxArray* in_directive;			/* Directive {clone, attach, detach, free} */
	const mxArray* in_var;           		/* Input array (for clone) */
	
	/* For storing inputs */
	mshdirective_t directive;
	
	size_t shm_size;					/* size required by the shared memory */
	
	/* for storing the mxArrays */
	Header_t hdr;
	InputData_t dat;

#ifdef MSH_WIN
	DWORD hi_sz, lo_sz, err;
#endif
	
	/* check min number of arguments */
	if(nrhs < 1)
	{
		readErrorMex("NotEnoughInputsError", "Minimum input arguments missing; must supply a directive.");
	}
	
	/* assign inputs */
	in_directive = prhs[0];
	
	/* get the directive */
	directive = parseDirective(in_directive);
	
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
		g_info->lcl_init_seg.handle = temp_handle;
		memcpy(g_info->lcl_init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->lcl_init_seg.is_init = TRUE;
	}
	else
	{
		if(CloseHandle(temp_handle) == 0)
		{
			readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
		}
	}
#else
	snprintf(init_check_name, MSH_MAX_NAME_LEN, MSH_INIT_CHECK_NAME, (unsigned long)(getpid()));
	int temp_handle;
	if((temp_handle = shm_open(init_check_name, O_RDONLY|O_CREAT|O_EXCL, 0))  == -1)
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
		g_info->lcl_init_seg.handle = temp_handle;
		memcpy(g_info->lcl_init_seg.name, init_check_name, MSH_MAX_NAME_LEN*sizeof(char));
		g_info->lcl_init_seg.is_init = TRUE;
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
			in_var = prhs[1];
			
			if(!(mxIsNumeric(in_var) || mxIsLogical(in_var) || mxIsChar(in_var) || mxIsStruct(in_var) || mxIsCell(in_var)))
			{
				readErrorMex("InvalidTypeError", "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
			}
			
			acquireProcLock();
			
			/* clear the previous variable if needed */
			if(!mxIsEmpty(g_shm_var))
			{
				/* if the current shared variable shares all the same dimensions, etc. then just copy over the memory */
				if(shmCompareSize(shm_data_ptr, in_var) == TRUE)
				{
					/* DON'T INCREMENT THE REVISION NUMBER */
					/* this is an in-place change, so everyone is still fine */
					
					/* do the rewrite after checking because the comparison is cheap */
					shmRewrite(shm_data_ptr, in_var);
					
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
			shm_size = shmScan(&hdr, &dat, in_var, &g_shm_var);
			
			/* update the revision number and indicate our info is current */
			g_info->cur_seg_info.rev_num = shm_update_info->rev_num + 1;
			if(shm_size > g_info->shm_data_seg.seg_sz)
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
				g_info->shm_data_seg.seg_sz = shm_size;
				
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
				
				g_info->shm_data_seg.seg_sz = shm_size;
				
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
			shmCopy(&hdr, &dat, shm_data_ptr, in_var, g_shm_var);
			
			mexMakeArrayPersistent(g_shm_var);
			g_info->flags.is_glob_shm_var_init = TRUE;
			
			/* make sure all of above is actually done before telling everyone to move to the new segment */
			updateAll();
			
			releaseProcLock();
			
			/* lazy return---doesn't return to ans, but returns if assigned */
			if(nlhs == 1)
			{
				plhs[0] = mxCreateSharedDataCopy(g_shm_var);
			}
			
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
			/* maybe print debug information at some point */
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
size_t shmScan(Header_t* hdr, InputData_t* dat, const mxArray* in_var, mxArray** ret_var)
{
	/* counters */
	size_t idx, count, field_str_len, cml_sz = 0, cml_child_off;
	int field_num;
	
	mxArray* ret_child;
	
	if(in_var == NULL)
	{
		releaseProcLock();
		readErrorMex("UnexpectedError", "Input variable was unexpectedly NULL.");
	}
	
	/* initialize header info */
	
	/* these are updated when the data is copied over */
	hdr->data_offsets.dims = SIZE_MAX;
	hdr->data_offsets.pr = SIZE_MAX;
	hdr->data_offsets.pi = SIZE_MAX;
	hdr->data_offsets.ir = SIZE_MAX;
	hdr->data_offsets.jc = SIZE_MAX;
	hdr->data_offsets.field_str = SIZE_MAX;
	hdr->data_offsets.child_hdrs = SIZE_MAX;
	
	hdr->is_numeric = mxIsNumeric(in_var);
	hdr->is_sparse = mxIsSparse(in_var);
	hdr->is_empty = mxIsEmpty(in_var);
	hdr->complexity = mxIsComplex(in_var)? mxCOMPLEX : mxREAL;
	hdr->classid = mxGetClassID(in_var);
	hdr->num_dims = mxGetNumberOfDimensions(in_var);
	hdr->elem_size = mxGetElementSize(in_var);
	hdr->num_elems = mxGetNumberOfElements(in_var);      /* update this later on sparse*/
	hdr->num_fields = 0;                                 /* update this later */
	hdr->obj_sz = 0;     /* update this later */
	
	dat->child_hdr_offs = NULL;
	dat->child_hdrs = NULL;
	dat->child_dat = NULL;
	
	/* Add space for the header */
	cml_sz += padToAlign(sizeof(Header_t));
	
	/* Add space for the dimensions */
	hdr->data_offsets.dims = cml_sz;
	cml_sz += padToAlign(hdr->num_dims * sizeof(mwSize));
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		if(hdr->is_empty)
		{
			if(hdr->num_fields > 0)
			{
				releaseProcLock();
				readErrorMex("UnexpectedError", "An empty struct array unexpectedly had more than one field. This is undefined behavior.");
			}
			*ret_var = mxCreateStructArray(hdr->num_dims, mxGetDimensions(in_var), hdr->num_fields, NULL);
			hdr->obj_sz = cml_sz;
		}
		else
		{
			
			/* How many fields to work with? */
			hdr->num_fields = mxGetNumberOfFields(in_var);
			
			/* Find the size required to store the field names */
			field_str_len = getFieldNamesSize(in_var);
			
			/* locate the field string */
			hdr->data_offsets.field_str = cml_sz;
			cml_sz += padToAlign(field_str_len);                   /* Add space for the field string */
			
			const char_t** field_names = mxMalloc(hdr->num_fields*sizeof(char_t*));
			for(field_num = 0; field_num < hdr->num_fields; field_num++)
			{
				field_names[field_num] = mxGetFieldNameByNumber(in_var, field_num);
			}
			
			*ret_var = mxCreateStructArray(hdr->num_dims, mxGetDimensions(in_var), hdr->num_fields, field_names);
			mxFree((void*)field_names);
			
			/* add size of storing the offsets */
			hdr->data_offsets.child_hdrs = cml_sz;
			cml_sz += padToAlign((hdr->num_fields * hdr->num_elems) * sizeof(size_t));
			dat->child_hdr_offs = mxMalloc((hdr->num_elems * hdr->num_fields) * sizeof(size_t));
			
			dat->child_hdrs = mxMalloc((hdr->num_elems * hdr->num_fields) * sizeof(Header_t*));
			dat->child_dat = mxMalloc((hdr->num_elems * hdr->num_fields) * sizeof(InputData_t*));
			
			/* set the object size here---don't include subobjects */
			hdr->obj_sz = cml_sz;
			
			/* go through each recursively */
			for(idx = 0, count = 0, cml_child_off = hdr->obj_sz; idx < hdr->num_elems; idx++)                         /* each element */
			{
				for(field_num = 0; field_num < hdr->num_fields; field_num++, count++)     /* each field */
				{
					
					dat->child_hdrs[count] = mxMalloc(sizeof(Header_t)); /* these will be replaced by offsets in the shmCopy */
					dat->child_dat[count] = mxMalloc(sizeof(InputData_t));
					
					/* call recursivley */
					cml_sz += shmScan(dat->child_hdrs[count], dat->child_dat[count], mxGetFieldByNumber(in_var, idx, field_num), &ret_child);
					mxSetFieldByNumber(*ret_var, idx, field_num, ret_child);
					
					dat->child_hdr_offs[count] = cml_child_off;
					cml_child_off += dat->child_hdrs[count]->obj_sz;
					
				}
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		
		if(hdr->is_empty)
		{
			*ret_var = mxCreateCellArray(hdr->num_dims, mxGetDimensions(in_var));
			hdr->obj_sz = cml_sz;
		}
		else
		{
			
			*ret_var = mxCreateCellArray(hdr->num_dims,  mxGetDimensions(in_var));
			
			/* add size of storing the offsets */
			hdr->data_offsets.child_hdrs = cml_sz;
			cml_sz += padToAlign(hdr->num_elems * sizeof(size_t));
			dat->child_hdr_offs = mxMalloc(hdr->num_elems * sizeof(size_t));
			
			dat->child_hdrs = mxMalloc(hdr->num_elems * sizeof(Header_t*));
			dat->child_dat = mxMalloc(hdr->num_elems * sizeof(InputData_t*));
			
			/* set the object size here---don't include subobjects */
			hdr->obj_sz = cml_sz;
			
			/* go through each recursively */
			for(count = 0, cml_child_off = hdr->obj_sz; count < hdr->num_elems; count++)
			{
				dat->child_hdrs[count] = mxMalloc(sizeof(Header_t)); /* these will be replaced by offsets in the shmCopy */
				dat->child_dat[count] = mxMalloc(sizeof(InputData_t));
				
				cml_sz += shmScan(dat->child_hdrs[count], dat->child_dat[count], mxGetCell(in_var, count), &ret_child);
				mxSetCell(*ret_var, count, ret_child);
				
				dat->child_hdr_offs[count] = cml_child_off;
				cml_child_off += dat->child_hdrs[count]->obj_sz;
				
			}
		}
	}
	else if(hdr->is_numeric || hdr->classid == mxLOGICAL_CLASS || hdr->classid == mxCHAR_CLASS)  /* a matrix containing data *//* base case */
	{
		
		/* ensure both pointers are aligned individually */
		cml_sz += padToAlign(MXMALLOC_SIG_LEN);
		hdr->data_offsets.pr = cml_sz;
		cml_sz += padToAlign(hdr->elem_size * hdr->num_elems);
		if(hdr->complexity == mxCOMPLEX)
		{
			cml_sz += padToAlign(MXMALLOC_SIG_LEN);
			hdr->data_offsets.pi = cml_sz;
			cml_sz += padToAlign(hdr->elem_size * hdr->num_elems);
		}
		
		if(hdr->is_sparse)
		{
			/* len(pr)=num_elems, len(ir)=num_elems, len(jc)=colc+1 */
			hdr->num_elems = (size_t)mxGetNzmax(in_var);
			
			cml_sz += padToAlign(MXMALLOC_SIG_LEN);
			hdr->data_offsets.ir = cml_sz;
			cml_sz += padToAlign(sizeof(mwIndex) * (hdr->num_elems));      /* ensure both pointers are aligned individually */
			
			cml_sz += padToAlign(MXMALLOC_SIG_LEN);
			hdr->data_offsets.jc = cml_sz;
			cml_sz += padToAlign(sizeof(mwIndex) * (mxGetN(in_var) + 1));
			
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
					*ret_var = mxCreateNumericArray(hdr->num_dims, mxGetDimensions(in_var), hdr->classid, hdr->complexity);
				}
				else if(hdr->classid == mxLOGICAL_CLASS)
				{
					*ret_var = mxCreateLogicalArray(hdr->num_dims, mxGetDimensions(in_var));
				}
				else
				{
					*ret_var = mxCreateCharArray(hdr->num_dims, mxGetDimensions(in_var));
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
		
		hdr->obj_sz = cml_sz;
		
	}
	else
	{
		releaseProcLock();
		readErrorMex("Internal:UnexpectedError", "Tried to clone an unsupported type.");
	}
	
	return cml_sz;
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
void shmCopy(Header_t* hdr, InputData_t* dat, byte_t* shm_anchor, const mxArray* in_var, mxArray* ret_var)
{
	size_t idx, cpy_sz, count;
	
	int field_num;
	const char_t* field_name;
	char_t* field_str_iter;
	
	/* load up the shared memory */
	ShmData_t data_ptrs;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	/* copy the dimensions */
	memcpy(data_ptrs.dims, mxGetDimensions(in_var), hdr->num_dims * sizeof(mwSize));
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		/* place the field names next in shared memory */
		
		/* copy the field string */
		for(field_num = 0, field_str_iter = data_ptrs.field_str; field_num < hdr->num_fields; field_num++, field_str_iter += cpy_sz)
		{
			field_name = mxGetFieldNameByNumber(in_var, field_num);
			cpy_sz = strlen(field_name) + 1;
			memcpy(field_str_iter, field_name, cpy_sz);
		}
		
		/* copy the children recursively */
		for(idx = 0, count = 0; idx < hdr->num_elems; idx++) 							/* the struct array indices */
		{
			for(field_num = 0; field_num < hdr->num_fields; field_num++, count++)		/* the fields */
			{
				/* place relative offset into shared memory */
				data_ptrs.child_hdrs[count] = dat->child_hdr_offs[count];
				
				/* And fill it */
				shmCopy(dat->child_hdrs[count],
					   dat->child_dat[count],
					   shm_anchor + data_ptrs.child_hdrs[count],
					   mxGetFieldByNumber(in_var, idx, field_num),
					   mxGetFieldByNumber(ret_var, idx, field_num));
				
				mxFree(dat->child_hdrs[count]);
				mxFree(dat->child_dat[count]);
			}
		}
		mxFree(dat->child_hdrs);
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		/* recurse for each cell element */
		for(count = 0; count < hdr->num_elems; count++)
		{
			/* place relative offset into shared memory */
			data_ptrs.child_hdrs[count] = dat->child_hdr_offs[count];
			
			shmCopy(dat->child_hdrs[count],
				   dat->child_dat[count],
				   shm_anchor + data_ptrs.child_hdrs[count],
				   mxGetCell(in_var, count),
				   mxGetCell(ret_var, count));
			
			mxFree(dat->child_hdrs[count]);
			mxFree(dat->child_dat[count]);
		}
		mxFree(dat->child_hdrs);
	}
	else /* base case */
	{
		/* copy real data */
		cpy_sz = (hdr->num_elems)*(hdr->elem_size);
		memCpyMex(data_ptrs.pr, mxGetData(in_var), cpy_sz);
		
		if(!hdr->is_empty)
		{
			mxFree(mxGetData(ret_var));
			mxSetData(ret_var, data_ptrs.pr);
		}
		
		/* copy complex data as well */
		if(hdr->complexity == mxCOMPLEX)
		{
			
			memCpyMex(data_ptrs.pi, mxGetImagData(in_var), cpy_sz);
			
			if(!hdr->is_empty)
			{
				mxFree(mxGetImagData(ret_var));
				mxSetImagData(ret_var, data_ptrs.pi);
			}
			
		}
		
		/* and the indices of the sparse data as well */
		if(hdr->is_sparse)
		{
			cpy_sz = hdr->num_elems*sizeof(mwIndex);
			memCpyMex((void*)data_ptrs.ir, (void*)mxGetIr(in_var), cpy_sz);
			
			mxFree(mxGetIr(ret_var));
			mxSetIr(ret_var, data_ptrs.ir);
			
			cpy_sz = (data_ptrs.dims[1] + 1)*sizeof(mwIndex);
			memCpyMex((void*)data_ptrs.jc, (void*)mxGetJc(in_var), cpy_sz);
			
			mxFree(mxGetJc(ret_var));
			mxSetJc(ret_var, data_ptrs.jc);
			
			mxSetNzmax(ret_var, hdr->num_elems);
		}
		mxSetDimensions(ret_var, data_ptrs.dims, hdr->num_dims);
	}
	
	memcpy(shm_anchor, hdr, sizeof(Header_t));
	
}


size_t shmFetch(byte_t* shm_anchor, mxArray** ret_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	mxArray* ret_child;
	
	/* for working with payload ... */
	Header_t* hdr;
	ShmData_t data_ptrs;
	
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*)shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		if(hdr->is_empty)
		{
			/* make this a separate case since there are no field names */
			*ret_var = mxCreateStructArray(hdr->num_dims, data_ptrs.dims, hdr->num_fields, NULL);
		}
		else
		{
			const char_t** field_names = mxMalloc(hdr->num_fields * sizeof(char_t*));
			retrieveFieldNames(field_names, data_ptrs.field_str, hdr->num_fields);
			
			*ret_var = mxCreateStructArray(hdr->num_dims, data_ptrs.dims, hdr->num_fields, field_names);
			mxFree((void*)field_names);
			
			/* Go through each element */
			for(idx = 0, count = 0; idx < hdr->num_elems; idx++)
			{
				for(field_num = 0; field_num < hdr->num_fields; field_num++, count++)     /* each field */
				{
					shmFetch(shm_anchor + data_ptrs.child_hdrs[count], &ret_child);
					mxSetFieldByNumber(*ret_var, idx, field_num, ret_child);
				}
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		*ret_var = mxCreateCellArray(hdr->num_dims, data_ptrs.dims);
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			shmFetch(shm_anchor + data_ptrs.child_hdrs[count], &ret_child);
			mxSetCell(*ret_var, count, ret_child);
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
			
			/* set the real and imaginary data */
			mxFree(mxGetData(*ret_var));
			mxSetData(*ret_var, data_ptrs.pr);
			if(hdr->complexity)
			{
				mxFree(mxGetImagData(*ret_var));
				mxSetImagData(*ret_var, data_ptrs.pi);
			}
			
			/* set the pointers relating to sparse */
			mxFree(mxGetIr(*ret_var));
//			mwIndex* new_ir = mxMalloc(hdr->num_elems * sizeof(mwIndex));
//			memcpy(new_ir, data_ptrs.ir, hdr->num_elems * sizeof(mwIndex));
			mxSetIr(*ret_var, data_ptrs.ir);

//			memcpy(mxGetJc(*ret_var), data_ptrs.jc, (data_ptrs.dims[1] + 1) * sizeof(mwIndex));
			mxFree(mxGetJc(*ret_var));
			mxSetJc(*ret_var, data_ptrs.jc);
			
			mxSetDimensions(*ret_var, data_ptrs.dims, hdr->num_dims);
			mxSetNzmax(*ret_var, hdr->num_elems);
			
		}
		else
		{
			
			if(hdr->is_empty)
			{
				if(hdr->is_numeric)
				{
					*ret_var = mxCreateNumericArray(hdr->num_dims, data_ptrs.dims, hdr->classid, hdr->complexity);
				}
				else if(hdr->classid == mxLOGICAL_CLASS)
				{
					*ret_var = mxCreateLogicalArray(hdr->num_dims, data_ptrs.dims);
				}
				else
				{
					*ret_var = mxCreateCharArray(hdr->num_dims, data_ptrs.dims);
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
				mxSetData(*ret_var, data_ptrs.pr);
				if(hdr->complexity)
				{
					mxSetImagData(*ret_var, data_ptrs.pi);
				}
				
				mxSetDimensions(*ret_var, data_ptrs.dims, hdr->num_dims);
				
			}
			
		}
		
	}
	return hdr->obj_sz;
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
			
			/* allocate 1 element */
			new_pr = mxCalloc(nzmax, mxGetElementSize(ret_var));
			mxSetData(ret_var, new_pr);
			if(mxIsComplex(ret_var))
			{
				new_pi = mxCalloc(nzmax, mxGetElementSize(ret_var));
				mxSetImagData(ret_var, new_pi);
			}
			
//			mxFree(mxGetIr(ret_var));
			new_ir = mxCalloc(nzmax, sizeof(mwIndex));
			mxSetIr(ret_var, new_ir);
			
			new_jc = mxCalloc(null_dims[1] + 1, sizeof(mwIndex));
			mxSetJc(ret_var, new_jc);
			
			mxSetDimensions(ret_var, null_dims, num_dims);
			mxSetNzmax(ret_var, nzmax);
			
		}
		else
		{
			mxSetData(ret_var, new_pr);
			if(mxIsComplex(ret_var))
			{
				mxSetImagData(ret_var, new_pi);
			}
			
			mxSetDimensions(ret_var, null_dims, num_dims);
			
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


size_t shmRewrite(byte_t* shm_anchor, const mxArray* in_var)
{
	size_t idx, count;
	
	/* for working with payload */
	Header_t* hdr;
	ShmData_t data_ptrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*)shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		/* Go through each element */
		for(idx = 0, count = 0; idx < hdr->num_elems; idx++)
		{
			for(field_num = 0; field_num < hdr->num_fields; field_num++, count++)     /* each field */
			{
				/* And fill it */
				shmRewrite(shm_anchor + data_ptrs.child_hdrs[count], mxGetFieldByNumber(in_var, idx, field_num));
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		for(count = 0; count < hdr->num_elems; count++)
		{
			/* And fill it */
			shmRewrite(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(in_var, count));
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		memcpy(data_ptrs.pr, mxGetData(in_var), (hdr->num_elems)*(hdr->elem_size));
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			memcpy(data_ptrs.pi, mxGetImagData(in_var), (hdr->num_elems)*(hdr->elem_size));
		}
		
		/* if sparse get a list of the elements */
		if(hdr->is_sparse)
		{
			memcpy(data_ptrs.ir, mxGetIr(in_var), (hdr->num_elems)*sizeof(mwIndex));
			memcpy(data_ptrs.jc, mxGetJc(in_var), ((data_ptrs.dims)[1] + 1)*sizeof(mwIndex));
		}
		
	}
	
	return hdr->obj_sz;
	
}


bool_t shmCompareSize(byte_t* shm_anchor, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	
	/* for working with payload ... */
	Header_t* hdr;
	ShmData_t data_ptrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*)shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	if(hdr->num_dims != mxGetNumberOfDimensions(comp_var)
	   || memcmp(data_ptrs.dims, mxGetDimensions(comp_var), sizeof(mwSize)*hdr->num_dims) != 0)
	{
		return FALSE;
	}
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		const char_t** field_names = mxMalloc(hdr->num_fields * sizeof(char_t*));
		retrieveFieldNames(field_names, data_ptrs.field_str, hdr->num_fields);
		
		/* Go through each element */
		for(idx = 0, count = 0; idx < hdr->num_elems; idx++)
		{
			for(field_num = 0; field_num < hdr->num_fields; field_num++, count++)     /* each field */
			{
				if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_names[field_num]) != 0)
				{
					return FALSE;
				}
				
				if(!shmCompareSize(shm_anchor + data_ptrs.child_hdrs[count], mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
		}
		
		mxFree((void*)field_names);
		
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			if(!shmCompareSize(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(comp_var, count)))
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
		
		if(hdr->is_sparse != mxIsSparse(comp_var))
		{
			return FALSE;
		}
		
		if(hdr->is_sparse)
		{

			if(hdr->num_elems != mxGetNzmax(comp_var)
			   || (data_ptrs.dims)[1] != mxGetN(comp_var))
			{
				return FALSE;
			}
			
		
		}
		else
		{

			if(hdr->num_elems != mxGetNumberOfElements(comp_var))
			{
				return FALSE;
			}
		}
		
	}
	
	return TRUE;
	
}








mxLogical shmCompareContent(byte_t* shm_anchor, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	
	/* for working with payload ... */
	Header_t* hdr;
	ShmData_t data_ptrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*)shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	if(hdr->num_dims != mxGetNumberOfDimensions(comp_var)
	   || memcmp((data_ptrs.dims), mxGetDimensions(comp_var), sizeof(mwSize)*hdr->num_dims) != 0
	   || hdr->num_elems != mxGetNumberOfElements(comp_var))
	{
		return FALSE;
	}
	
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		const char_t** field_names = mxMalloc(hdr->num_fields * sizeof(char_t*));
		retrieveFieldNames(field_names, data_ptrs.field_str, hdr->num_fields);
		
		/* Go through each element */
		for(idx = 0, count = 0; idx < hdr->num_elems; idx++)
		{
			for(field_num = 0; field_num < hdr->num_fields; field_num++, count++)     /* each field */
			{
				if(strcmp(mxGetFieldNameByNumber(comp_var, field_num), field_names[field_num]) != 0)
				{
					return FALSE;
				}
				
				if(!shmCompareContent(shm_anchor + data_ptrs.child_hdrs[count], mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
		}
		
		mxFree((void*)field_names);
		
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			if(!shmCompareContent(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(comp_var, count)))
			{
				return FALSE;
			}
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		
		//these should point to the same location
		if(data_ptrs.pr != mxGetData(comp_var))
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
			if(data_ptrs.pi != mxGetImagData(comp_var))
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
			
			if(data_ptrs.ir != mxGetIr(comp_var)
			   || data_ptrs.jc != mxGetJc(comp_var))
			{
				return FALSE;
			}
			
		}
		
	}
	
	return TRUE;
	
}




