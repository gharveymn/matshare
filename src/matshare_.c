#include "headers/matshare_.h"
#include "headers/externtypes.h"

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
		readErrorMex("NotEnoughInputsError", "Minimum input arguments missing; must supply a directive.");
	}
	
	/* assign inputs */
	in_directive = prhs[0];
	
	/* get the directive */
	directive = parseDirective(in_directive);

#ifdef MSH_AUTO_INIT
	autoInit(directive);
#endif
	
	if(directive != msh_DETACH && directive != msh_INIT && precheck() != TRUE)
	{
		readErrorMex("NotInitializedError",
				   "At least one of the needed shared memory segments has not been initialized. Cannot continue.");
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
				readErrorMex("NoVariableError", "No variable supplied to clone.");
			}
			
			/* Assign */
			in_var = prhs[1];
			
			if(!(mxIsNumeric(in_var) || mxIsLogical(in_var) || mxIsChar(in_var) || mxIsStruct(in_var) || mxIsCell(in_var)))
			{
				readErrorMex("InvalidTypeError",
						   "Unexpected input type. Shared variable must be of type 'numeric', 'logical', 'char', 'struct', or 'cell'.");
			}
			
			mshShare(nlhs, plhs, in_var);
			
			break;
		
		case msh_FETCH:
			
			mshFetch(nlhs, plhs);
			
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
			//plhs[0] = mxDuplicateArray(g_var_list_front->var);
			break;
		case msh_DEBUG:
			/* maybe print debug information at some point */
			break;
		case msh_OBJ_REGISTER:
			/* tell matshare to register a new object */
			g_info->num_registered_objs += 1;
			break;
		case msh_INIT:
#ifndef MSH_AUTO_INIT
			init();
#else
			readWarnMex("AutoInitWarn",
					  "matshare has been compiled with automatic initialization turned on. There is not need to call mshinit.");
#endif
			break;
		default:
			readErrorMex("UnknownDirectiveError", "Unrecognized directive.");
			break;
	}
	
}


void mshShare(int nlhs, mxArray* plhs[], const mxArray* in_var)
{
	
	acquireProcLock();
	
	mshUpdateSegments();
	
	if(shm_info->sharetype == msh_SHARETYPE_COPY)
	{
		removeUnused();
	}
	else if(shm_info->sharetype == msh_SHARETYPE_OVERWRITE)
	{
		if(g_var_list.num_nodes != 0 &&
		   !mxIsEmpty(g_var_list.back->var) &&
		   (shmCompareSize((byte_t*)g_seg_list.back->data_seg.ptr, in_var) == TRUE))
		{
			/* DON'T INCREMENT THE REVISION NUMBER */
			/* this is an in-place change, so everyone is still fine */
			
			/* do the rewrite after checking because the comparison is cheap */
			shmRewrite((byte_t*)g_seg_list.back->data_seg.ptr, in_var);
			
			updateAll();
			releaseProcLock();
			
			if(nlhs == 1)
			{
				plhs[0] = mxCreateSharedDataCopy(g_var_list.back->var);
			}
			
			/* DON'T DO ANYTHING ELSE */
			return;
		}
	}
	else
	{
		releaseProcLock();
		readErrorMex("UnexpectedError", "Invalid sharetype. The shared memory has been corrupted.");
	}
	
	/* scan input data */
	addSegment(shmScan(in_var));
	
	/* copy data to the shared memory */
	shmCopy(g_seg_list.back, in_var);
	
	if(shm_info->sharetype == msh_SHARETYPE_OVERWRITE)
	{
		cleanVariableList();
	}
	
	if(nlhs == 1)
	{
		addVariable(g_seg_list.back);
		plhs[0] = mxCreateSharedDataCopy(g_var_list.back->var);
	}
	
	updateAll();
	releaseProcLock();
}


void mshFetch(int nlhs, mxArray* plhs[])
{
	
	VariableNode_t* new_var_node,* curr_var_node,* next_var_node;
	SegmentNode_t* curr_seg_node;
	VariableList_t new_vars_list = {NULL, NULL, 0};
	size_t i;
	size_t ret_dims[2] = {1,1};
	
	mshUpdateSegments();
	
	acquireProcLock();
	
	switch(shm_info->sharetype)
	{
		
		case msh_SHARETYPE_COPY:
			
			removeUnused();
			
			if(g_seg_list.num_nodes == 0)
			{
				for(i = 0; i < nlhs; i++)
				{
					plhs[i] = mxCreateDoubleMatrix(0, 0, mxREAL);
				}
				return;
			}
			
			switch(nlhs)
			{
				case 0:
					/* do nothing */
					return;
				case 1:
					
					if(g_seg_list.back->var_node == NULL)
					{
						addVariable(g_seg_list.back);
					}
					plhs[0] = mxCreateSharedDataCopy(g_seg_list.back->var_node->var);
					break;
					
				case 2:
				case 3:
					
					curr_seg_node = g_seg_list.front;
					while(curr_seg_node != NULL)
					{
						curr_var_node = curr_seg_node->var_node;
						if(curr_var_node == NULL)
						{
							/* create the variable node if it hasnt been created yet */
							addVariable(curr_seg_node);
							
							if(nlhs == 2)
							{
								/* place in new variables list */
								new_var_node = mxMalloc(sizeof(VariableNode_t));
								new_var_node->next = NULL;
								new_var_node->prev = NULL;
								new_var_node->var = curr_seg_node->var_node->var;
								
								if(new_vars_list.num_nodes != 0)
								{
									new_vars_list.back->next = new_var_node;
								}
								else
								{
									new_vars_list.front = new_var_node;
								}
								new_vars_list.back = new_var_node;
								
								new_vars_list.num_nodes += 1;
							}
							
						}
						
						curr_seg_node->var_node->next = NULL;
						curr_seg_node->var_node->prev = NULL;
						
						if(curr_seg_node->prev != NULL)
						{
							/* link up previous node */
							curr_seg_node->var_node->prev = curr_seg_node->prev->var_node;
							curr_seg_node->prev->var_node->next = curr_seg_node->var_node;
						}
						
						curr_seg_node = curr_seg_node->next;
					}
					
					g_var_list.num_nodes = g_seg_list.num_nodes;
					
					plhs[0] = mxCreateSharedDataCopy(g_seg_list.back->var_node->var);
					
					if(nlhs >= 2)
					{
						ret_dims[0] = new_vars_list.num_nodes;
						plhs[1] = mxCreateCellArray(2, ret_dims);
						curr_var_node = new_vars_list.front;
						for(i = 0; i < new_vars_list.num_nodes; i++, curr_var_node = next_var_node)
						{
							next_var_node = curr_var_node->next;
							mxSetCell(plhs[1], i, mxCreateSharedDataCopy(curr_var_node->var));
							mxFree(curr_var_node);
						}
						
						if(nlhs == 3)
						{
							ret_dims[0] = g_var_list.num_nodes;
							plhs[2] = mxCreateCellArray(2, ret_dims);
							curr_var_node = g_var_list.front;
							for(i = 0; i < g_var_list.num_nodes; i++, curr_var_node = curr_var_node->next)
							{
								mxSetCell(plhs[2], i, mxCreateSharedDataCopy(curr_var_node->var));
							}
						}
					}
					
				default:
					readErrorMex("OutputError", "Too many outputs.");
			}
			
			break;
		case msh_SHARETYPE_OVERWRITE:
			
			if(nlhs == 0)
			{
				/* in this case mshFetch is just used to clear out unused variables and update this process */
				return;
			}
			else if(nlhs > 1)
			{
				readErrorMex("OutputError", "Too many outputs.");
			}
			
			/* check if the current revision number is the same as the shm revision number
			 * if it hasn't been changed then the segment was subject to an inplace change
			 */
			if(g_seg_list.back->var_node == NULL)
			{
				
				/* remove all other variable nodes */
				cleanVariableList();
				
				addVariable(g_seg_list.back);
				
				shmFetch((byte_t*)g_seg_list.back->data_seg.ptr, &g_var_list.back->var);
				g_var_list.back->crosslink = &((mxArrayStruct*)g_var_list.back->var)->CrossLink;
				
			}
			plhs[0] = mxCreateSharedDataCopy(g_var_list.back->var);
			break;
		default:
			releaseProcLock();
			readErrorMex("UnexpectedError", "Invalid sharetype. The shared memory has been corrupted.");
		
	}
	
	releaseProcLock();
}


/**
 * Update the local segment list from shared memory
 */
void mshUpdateSegments(void)
{
	size_t i;
	signed long curr_seg_num;
	bool_t is_fetched;
	
	SegmentNode_t* seg_node_iter,* new_seg_node = NULL,* next_seg_node;
	SegmentList_t new_seg_list = {NULL, NULL, 0};
	
#ifdef MSH_WIN
	DWORD err;
#else
	errno_t err;
#endif
	
	SegmentMetadata_t* temp_map;
	
	if(g_info->rev_num == shm_info->rev_num)
	{
		/* we should be up to date if this is true */
		return;
	}
	
	seg_node_iter = g_seg_list.front;
	while(seg_node_iter != NULL)
	{
		
		next_seg_node = seg_node_iter->next;
		
		/* mark each node for deletion unless it will be reused */
		seg_node_iter->will_free = TRUE;
		
		/* check if the data segment is ready for deletion */
		if(seg_node_iter->data_seg.ptr->is_fetched && seg_node_iter->data_seg.ptr->procs_using == 0)
		{
#ifdef MSH_WIN
			if(seg_node_iter->data_seg.is_mapped)
			{
				if(UnmapViewOfFile(seg_node_iter->data_seg.ptr) == 0)
				{
					if(g_info->flags.is_proc_lock_init)
					{ releaseProcLock(); }
					readErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
				}
				seg_node_iter->data_seg.is_mapped = FALSE;
			}
			
			if(seg_node_iter->data_seg.is_init)
			{
				if(CloseHandle(seg_node_iter->data_seg.handle) == 0)
				{
					if(g_info->flags.is_proc_lock_init)
					{ releaseProcLock(); }
					readErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
				}
				seg_node_iter->data_seg.is_init = FALSE;
			}
#else
			bool_t will_remove_data = FALSE;
			if(seg_node_iter->data_seg.is_mapped)
			{
	
				will_remove_data = (bool_t) (seg_node_iter->data_seg.ptr->procs_using == 1);
	
				if(munmap(seg_node_iter->data_seg.ptr, seg_node_iter->data_seg.seg_sz) != 0)
				{
					if(g_info->flags.is_proc_lock_init)
					{ releaseProcLock(); }
					readMunmapError(errno);
				}
				seg_node_iter->data_seg.is_mapped = FALSE;
			}
	
			if(seg_node_iter->data_seg.is_init)
			{
				if(will_remove_data)
				{
					if(shm_unlink(seg_node_iter->data_seg.name) != 0)
					{
						if(g_info->flags.is_proc_lock_init)
						{ releaseProcLock(); }
						readShmUnlinkError(errno);
					}
				}
				seg_node_iter->data_seg.is_init = FALSE;
			}
	#endif
	
			if(seg_node_iter->prev != NULL)
			{
				seg_node_iter->prev->next = seg_node_iter->next;
			}
			
			if(seg_node_iter->next != NULL)
			{
				seg_node_iter->next->prev = seg_node_iter->prev;
			}
			
			mxFree(seg_node_iter);
		}
		seg_node_iter = next_seg_node;
	}
	
	acquireProcLock();
	
	/* construct a new list of segments, reusing what we can */
	for(i = 0, curr_seg_num = shm_info->first_seg_num; i < shm_info->num_shared_vars; i++)
	{
		
		seg_node_iter = g_seg_list.front;
		is_fetched = FALSE;
		while(seg_node_iter != NULL)
		{
			
			/* check if the current segment has been fetched */
			if(seg_node_iter->seg_num == curr_seg_num)
			{
				/* the segment has been fetched already, check if the local segment tracking has the same next and previous as shm */
				if(seg_node_iter->next_seg_num != seg_node_iter->data_seg.ptr->next_seg_num
				   || seg_node_iter->prev_seg_num != seg_node_iter->data_seg.ptr->prev_seg_num)
				{
					/* create a new var node */
					new_seg_node = mxMalloc(sizeof(SegmentNode_t));
					mexMakeMemoryPersistent(new_seg_node);
					memcpy(new_seg_node, seg_node_iter, sizeof(SegmentNode_t));
					new_seg_node->next_seg_num = new_seg_node->data_seg.ptr->next_seg_num;
					new_seg_node->prev_seg_num = new_seg_node->data_seg.ptr->prev_seg_num;
					new_seg_node->next = NULL;
					new_seg_node->prev = new_seg_list.back;
					
					if(new_seg_node->prev != NULL)
					{
						/* adjust the reference in the previous node */
						new_seg_node->prev->next = new_seg_node;
					}
					
				}
				else
				{
					/* hook the currently used node into the list */
					new_seg_node = seg_node_iter;
					seg_node_iter->will_free = FALSE;
				}
				
				is_fetched = TRUE;
				break;
			}
			seg_node_iter = seg_node_iter->next;
		}
		
		if(!is_fetched)
		{
			/* if this has not been fetched yet make a new var node with a new map */
			new_seg_node = mxMalloc(sizeof(SegmentNode_t));
			mexMakeMemoryPersistent(new_seg_node);
			new_seg_node->will_free = FALSE;
			new_seg_node->next = NULL;
			new_seg_node->prev = new_seg_list.back;
			new_seg_node->seg_num = curr_seg_num;
			
			/* update the region name */
			snprintf(new_seg_node->data_seg.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME,
				    (unsigned long long) new_seg_node->seg_num);

#ifdef MSH_WIN
			/* get the new file handle */
			new_seg_node->data_seg.handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE,
												   new_seg_node->data_seg.name);
			if(new_seg_node->data_seg.handle == NULL)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("OpenFileError", "Error opening the file mapping (Error Number %u)", err);
			}
			new_seg_node->data_seg.is_init = TRUE;
			
			
			/* map the metadata to get the size of the segment */
			temp_map = MapViewOfFile(new_seg_node->data_seg.handle, FILE_MAP_ALL_ACCESS, 0, 0,
								sizeof(SegmentMetadata_t));
			if(temp_map == NULL)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("MappingError", "Could not fetch the memory segment (Error number: %d).",
						   err);
			}
			else
			{
				/* get the segment size */
				new_seg_node->data_seg.seg_sz = temp_map->seg_sz;
			}
			
			/* unmap the temporary view */
			if(UnmapViewOfFile(temp_map) == 0)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("UnmapFileError", "Error unmapping the file (Error Number %u)", err);
			}
			
			/* now map the whole thing */
			new_seg_node->data_seg.ptr = MapViewOfFile(new_seg_node->data_seg.handle,
											   FILE_MAP_ALL_ACCESS, 0, 0,
											   new_seg_node->data_seg.seg_sz);
			if(new_seg_node->data_seg.ptr == NULL)
			{
				err = GetLastError();
				releaseProcLock();
				readErrorMex("MappingError", "Could not fetch the memory segment (Error number: %d).",
						   err);
			}
			new_seg_node->data_seg.is_mapped = TRUE;

#else
			/* get the new file handle */
			new_seg_node->data_seg.handle = shm_open(new_seg_node->data_seg.name, O_RDWR, shm_info->security);
			if(new_seg_node->data_seg.handle == -1)
			{
				err = errno;
				releaseProcLock();
				readShmOpenError(err);
			}
			new_seg_node->data_seg.is_init = TRUE;
			
			/* map the metadata to get the size of the segment */
			temp_map = mmap(NULL, sizeof(SegmentMetadata_t), PROT_READ | PROT_WRITE, MAP_SHARED, new_seg_node->data_seg.handle, 0);
			if(temp_map == MAP_FAILED)
			{
				err = errno;
				releaseProcLock();
				readMmapError(err);
			}
			
			/* get the segment size */
			new_seg_node->data_seg.seg_sz = temp_map->seg_sz;
			
			/* unmap the temporary map */
			if(munmap(temp_map, sizeof(SegmentMetadata_t)) != 0)
			{
				err = errno;
				releaseProcLock();
				readMunmapError(err);
			}
			
			/* now map the whole thing */
			new_seg_node->data_seg.ptr = mmap(NULL, new_seg_node->data_seg.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, new_seg_node->data_seg.handle, 0);
			if(new_seg_node->data_seg.ptr == MAP_FAILED)
			{
				err = errno;
				releaseProcLock();
				readMmapError(err);
			}
			new_seg_node->data_seg.is_mapped = TRUE;
#endif
			
			new_seg_node->prev_seg_num = new_seg_node->data_seg.ptr->prev_seg_num;
			new_seg_node->next_seg_num = new_seg_node->data_seg.ptr->next_seg_num;
			
			new_seg_node->data_seg.ptr->is_fetched = TRUE;
			new_seg_node->data_seg.ptr->procs_using += 1;
			
		}
		
		if(new_seg_list.back != NULL)
		{
			new_seg_list.back->next = new_seg_node;
		}
		else
		{
			new_seg_list.front = new_seg_node;
		}
		new_seg_list.back = new_seg_node;
		curr_seg_num = new_seg_list.back->data_seg.ptr->next_seg_num;
		
	}
	
	/* free nodes which were not reused in the new list */
	seg_node_iter = g_seg_list.front;
	for(i = 0; i < g_seg_list.num_nodes; i++)
	{
		next_seg_node = seg_node_iter->next;
		if(seg_node_iter->will_free)
		{
			mxFree(seg_node_iter);
		}
		seg_node_iter = next_seg_node;
	}
	
	/* set the new list front */
	g_seg_list.front = new_seg_list.front;
	
	/* the back */
	g_seg_list.back = new_seg_list.back;
	
	/* and the current number of tracked segments */
	g_seg_list.num_nodes = shm_info->num_shared_vars;
	
	releaseProcLock();
}

size_t shmScan(const mxArray* in_var)
{
	return shmScan_(in_var) + padToAlign(sizeof(SegmentMetadata_t));
}


/* ------------------------------------------------------------------------- */
/* shmScan_                                                                  */
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
size_t shmScan_(const mxArray* in_var)
{
	
	if(in_var == NULL)
	{
		releaseProcLock();
		readErrorMex("UnexpectedError", "Input variable was unexpectedly NULL.");
	}
	
	/* counters */
	Header_t hdr;
	size_t idx, count, cml_sz = 0;
	int field_num;
	
	const size_t padded_mxmalloc_sig_len = padToAlign(MXMALLOC_SIG_LEN);
	
	hdr.num_dims = mxGetNumberOfDimensions(in_var);
	hdr.elem_size = mxGetElementSize(in_var);
	hdr.num_elems = mxGetNumberOfElements(in_var);
	hdr.nzmax = mxGetNzmax(in_var);
	hdr.obj_sz = 0;     /* update this later */
	hdr.num_fields = mxGetNumberOfFields(in_var);
	hdr.classid = mxGetClassID(in_var);
	hdr.complexity = mxIsComplex(in_var) ? mxCOMPLEX : mxREAL;
	hdr.is_sparse = mxIsSparse(in_var);
	hdr.is_numeric = mxIsNumeric(in_var);
	hdr.is_empty = (bool_t) (hdr.num_elems == 0);
	
	/* Add space for the header */
	cml_sz += padToAlign(sizeof(Header_t));
	
	/* Add space for the dimensions */
	cml_sz += padToAlign(mxGetNumberOfDimensions(in_var) * sizeof(mwSize));
	
	/* Structure case */
	if(hdr.classid == mxSTRUCT_CLASS)
	{
		/* Add space for the field string */
		cml_sz += padToAlign(getFieldNamesSize(in_var));
		
		/* add size of storing the offsets */
		cml_sz += padToAlign((hdr.num_fields * hdr.num_elems) * sizeof(size_t));
		
		/* go through each recursively */
		for(field_num = 0, count = 0; field_num < hdr.num_fields; field_num++)          /* each field */
		{
			for(idx = 0; idx < hdr.num_elems; idx++, count++)                         /* each element */
			{
				/* call recursivley */
				cml_sz += shmScan_(mxGetFieldByNumber(in_var, idx, field_num));
				
			}
			
		}
	}
	else if(hdr.classid == mxCELL_CLASS) /* Cell case */
	{
		
		/* add size of storing the offsets */
		cml_sz += padToAlign(hdr.num_elems * sizeof(size_t));
		
		/* go through each recursively */
		for(count = 0; count < hdr.num_elems; count++)
		{
			cml_sz += shmScan_(mxGetCell(in_var, count));
			
			
		}
	}
	else if(hdr.is_numeric || hdr.classid == mxLOGICAL_CLASS ||
		   hdr.classid == mxCHAR_CLASS)  /* a matrix containing data *//* base case */
	{
		
		if(hdr.is_sparse)
		{
			/* len(pr)==nzmax, len(pi)==nzmax, len(ir)=nzmax, len(jc)==N+1 */
			
			/* ensure both pointers are aligned individually */
			cml_sz += padded_mxmalloc_sig_len + padToAlign(hdr.elem_size * hdr.nzmax);
			if(hdr.complexity == mxCOMPLEX)
			{
				cml_sz += padded_mxmalloc_sig_len + padToAlign(hdr.elem_size * hdr.nzmax);
			}
			cml_sz += padded_mxmalloc_sig_len + padToAlign(sizeof(mwIndex) * (hdr.nzmax));              /* ir */
			cml_sz += padded_mxmalloc_sig_len + padToAlign(sizeof(mwIndex) * (mxGetN(in_var) + 1));     /* jc */
			
		}
		else
		{
			
			/* ensure both pointers are aligned individually */
			cml_sz += padded_mxmalloc_sig_len + padToAlign(hdr.elem_size * hdr.num_elems);
			if(hdr.complexity == mxCOMPLEX)
			{
				cml_sz += padded_mxmalloc_sig_len + padToAlign(hdr.elem_size * hdr.num_elems);
			}
		}
		
	}
	else
	{
		releaseProcLock();
		readErrorMex("Internal:UnexpectedError", "Tried to clone an unsupported type.");
	}
	
	return cml_sz;
}


void shmCopy(SegmentNode_t* seg_node, const mxArray* in_var)
{
	SegmentMetadata_t metadata;
	metadata.procs_using = 0;
	metadata.next_seg_num = seg_node->next_seg_num;
	metadata.prev_seg_num = seg_node->prev_seg_num;
	metadata.seg_sz = seg_node->data_seg.seg_sz;
	metadata.is_fetched = FALSE;
	
	/* copy metadata to shared memory */
	memcpy(seg_node->data_seg.ptr, &metadata, sizeof(SegmentMetadata_t));
	
	shmCopy_(((byte_t*)seg_node->data_seg.ptr) + padToAlign(sizeof(SegmentMetadata_t)), in_var);
}


/* ------------------------------------------------------------------------- */
/* shmCopy_                                                                  */
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
size_t shmCopy_(byte_t* shm_anchor, const mxArray* in_var)
{
	
	const size_t padded_mxmalloc_sig_len = padToAlign(MXMALLOC_SIG_LEN);
	
	Header_t hdr;
	ShmData_t data_ptrs;
	byte_t* shm_ptr = shm_anchor;
	
	size_t cml_off = 0, shift, idx, cpy_sz, count, field_str_len;
	
	int field_num;
	const char_t* field_name;
	char_t* field_str_iter;
	
	/* initialize header info */
	
	/* these are updated when the data is copied over */
	hdr.data_offsets.dims = SIZE_MAX;
	hdr.data_offsets.pr = SIZE_MAX;
	hdr.data_offsets.pi = SIZE_MAX;
	hdr.data_offsets.ir = SIZE_MAX;
	hdr.data_offsets.jc = SIZE_MAX;
	hdr.data_offsets.field_str = SIZE_MAX;
	hdr.data_offsets.child_hdrs = SIZE_MAX;
	
	hdr.num_dims = mxGetNumberOfDimensions(in_var);
	hdr.elem_size = mxGetElementSize(in_var);
	hdr.num_elems = mxGetNumberOfElements(in_var);
	hdr.nzmax = mxGetNzmax(in_var);
	hdr.num_fields = mxGetNumberOfFields(in_var);
	hdr.classid = mxGetClassID(in_var);
	hdr.complexity = mxIsComplex(in_var) ? mxCOMPLEX : mxREAL;
	hdr.is_sparse = mxIsSparse(in_var);
	hdr.is_numeric = mxIsNumeric(in_var);
	hdr.is_empty = (bool_t) (hdr.num_elems == 0);
	
	shift = padToAlign(sizeof(Header_t));
	cml_off += shift, shm_ptr += shift;
	
	/* copy the dimensions */
	hdr.data_offsets.dims = cml_off;
	data_ptrs.dims = (mwSize*) shm_ptr;
	memcpy(data_ptrs.dims, mxGetDimensions(in_var), hdr.num_dims * sizeof(mwSize));
	
	shift = padToAlign(hdr.num_dims * sizeof(mwSize));
	cml_off += shift, shm_ptr += shift;
	
	/* Structure case */
	if(hdr.classid == mxSTRUCT_CLASS)
	{
		/* place the field names next in shared memory */
		
		/* Find the size required to store the field names */
		field_str_len = getFieldNamesSize(in_var);
		
		/* locate the field string */
		hdr.data_offsets.field_str = cml_off;
		data_ptrs.field_str = shm_ptr;
		
		/*shift past the field string */
		shift = padToAlign(field_str_len);
		cml_off += shift, shm_ptr += shift;
		
		
		hdr.data_offsets.child_hdrs = cml_off;
		data_ptrs.child_hdrs = (size_t*) shm_ptr;
		cml_off += padToAlign((hdr.num_fields * hdr.num_elems) * sizeof(size_t));
		
		hdr.obj_sz = cml_off;
		
		/* copy the children recursively */
		field_str_iter = data_ptrs.field_str;
		for(field_num = 0, count = 0; field_num < hdr.num_fields; field_num++)                /* the fields */
		{
			
			field_name = mxGetFieldNameByNumber(in_var, field_num);
			cpy_sz = strlen(field_name) + 1;
			memcpy(field_str_iter, field_name, cpy_sz);
			field_str_iter += cpy_sz;
			
			for(idx = 0;
			    idx < hdr.num_elems; idx++, count++)                                   /* the struct array indices */
			{
				/* place relative offset into shared memory */
				data_ptrs.child_hdrs[count] = cml_off;
				
				/* And fill it */
				cml_off += shmCopy_(shm_anchor + data_ptrs.child_hdrs[count],
								mxGetFieldByNumber(in_var, idx, field_num));
				
			}
			
		}
		
	}
	else if(hdr.classid == mxCELL_CLASS) /* Cell case */
	{
		
		
		hdr.data_offsets.child_hdrs = cml_off;
		data_ptrs.child_hdrs = (size_t*) shm_ptr;
		cml_off += padToAlign(hdr.num_elems * sizeof(size_t));
		
		hdr.obj_sz = cml_off;
		
		
		/* recurse for each cell element */
		for(count = 0; count < hdr.num_elems; count++)
		{
			/* place relative offset into shared memory */
			data_ptrs.child_hdrs[count] = cml_off;
			
			cml_off += shmCopy_(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(in_var, count));
			
			
		}
		
	}
	else /* base case */
	{
		
		/* and the indices of the sparse data as well */
		if(hdr.is_sparse)
		{
			
			/* note: in this case hdr->is_empty means the sparse is 0x0
			 *  also note that nzmax is still 1 */
			
			
			/* copy pr */
			cpy_sz = (hdr.nzmax) * (hdr.elem_size);
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.pr = cml_off, data_ptrs.pr = shm_ptr;
			
			memCpyMex(data_ptrs.pr, mxGetData(in_var), cpy_sz);
			
			shift = padToAlign(cpy_sz);
			cml_off += shift, shm_ptr += shift;
			
			/* copy pi */
			if(hdr.complexity == mxCOMPLEX)
			{
				cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
				hdr.data_offsets.pi = cml_off, data_ptrs.pi = shm_ptr;
				
				memCpyMex(data_ptrs.pi, mxGetImagData(in_var), cpy_sz);
				
				shift = padToAlign(cpy_sz);
				cml_off += shift, shm_ptr += shift;
				
			}
			
			/* copy ir */
			cpy_sz = hdr.nzmax * sizeof(mwIndex);
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.ir = cml_off, data_ptrs.ir = (mwIndex*) shm_ptr;
			
			memCpyMex((void*) data_ptrs.ir, (void*) mxGetIr(in_var), cpy_sz);
			
			shift = padToAlign(cpy_sz);
			cml_off += shift, shm_ptr += shift;
			
			/* copy jc */
			cpy_sz = (data_ptrs.dims[1] + 1) * sizeof(mwIndex);
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.jc = cml_off, data_ptrs.jc = (mwIndex*) shm_ptr;
			
			memCpyMex((void*) data_ptrs.jc, (void*) mxGetJc(in_var), cpy_sz);
			
			shift = padToAlign(cpy_sz);
			cml_off += shift, shm_ptr += shift;
			
		}
		else
		{
			/* copy pr */
			cpy_sz = (hdr.num_elems) * (hdr.elem_size);
			cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
			hdr.data_offsets.pr = cml_off, data_ptrs.pr = shm_ptr;
			
			memCpyMex(data_ptrs.pr, mxGetData(in_var), cpy_sz);
			
			shift = padToAlign(cpy_sz);
			cml_off += shift, shm_ptr += shift;
			
			/* copy complex data as well */
			if(hdr.complexity == mxCOMPLEX)
			{
				cml_off += padded_mxmalloc_sig_len, shm_ptr += padded_mxmalloc_sig_len;
				hdr.data_offsets.pi = cml_off, data_ptrs.pi = shm_ptr;
				
				memCpyMex(data_ptrs.pi, mxGetImagData(in_var), cpy_sz);
				
				shift = padToAlign(cpy_sz);
				cml_off += shift, shm_ptr += shift;
			}
		}
		
		hdr.obj_sz = cml_off;
		
	}
	
	memcpy(shm_anchor, &hdr, sizeof(Header_t));
	
	return cml_off;
	
}


void shmFetch(byte_t* shm_anchor, mxArray** ret_var)
{
	shmFetch_(shm_anchor + padToAlign(sizeof(SegmentMetadata_t)), ret_var);
	mexMakeArrayPersistent(*ret_var);
}


size_t shmFetch_(byte_t* shm_anchor, mxArray** ret_var)
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
	hdr = (Header_t*) shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	const char_t* field_name;
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		
		field_name = data_ptrs.field_str;
		*ret_var = mxCreateStructArray(hdr->num_dims, data_ptrs.dims, 0, NULL);
		for(field_num = 0; field_num < hdr->num_fields; field_num++)
		{
			mxAddField(*ret_var, field_name);
			getNextFieldName(&field_name);
		}
		
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < hdr->num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < hdr->num_elems; idx++, count++)
			{
				shmFetch_(shm_anchor + data_ptrs.child_hdrs[count], &ret_child);
				mxSetFieldByNumber(*ret_var, idx, field_num, ret_child);
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		*ret_var = mxCreateCellArray(hdr->num_dims, data_ptrs.dims);
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			shmFetch_(shm_anchor + data_ptrs.child_hdrs[count], &ret_child);
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
				releaseProcLock();
				readErrorMex("UnrecognizedTypeError",
						   "The fetched array was of class 'sparse' but not of type 'double' or 'logical'");
			}
			
			if(!hdr->is_empty)
			{
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
				mxSetIr(*ret_var, data_ptrs.ir);
				
				mxFree(mxGetJc(*ret_var));
				mxSetJc(*ret_var, data_ptrs.jc);
			}
			
			mxSetDimensions(*ret_var, data_ptrs.dims, hdr->num_dims);
			mxSetNzmax(*ret_var, hdr->nzmax);
			
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
	size_t num_dims = 0, idx, num_elems;
	int field_num, num_fields;
	mwSize null_dims[] = {0, 0};
	void* new_pr = NULL, * new_pi = NULL;
	void* new_ir = NULL, * new_jc = NULL;
	mwSize nzmax = 0;
	
	/* restore matlab  memory */
	if(ret_var == (mxArray*) NULL || mxIsEmpty(ret_var))
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
			for(idx = 0; idx < num_elems; idx++)
			{
				shmDetach(mxGetFieldByNumber(ret_var, idx, field_num));
			}/* detach this one */
		}
	}
	else if(mxIsCell(ret_var))
	{
		/* detach each cell */
		num_elems = mxGetNumberOfElements(ret_var);
		for(idx = 0; idx < num_elems; idx++)
		{
			shmDetach(mxGetCell(ret_var, idx));
		}/* detach this one */
		
	}
	else if(mxIsNumeric(ret_var) || mxIsLogical(ret_var) || mxIsChar(ret_var))  /* a matrix containing data */
	{
		
		/* handle sparse objects */
		if(mxIsSparse(ret_var))
		{
			/* I don't seem to be able to give sparse arrays zero size so (nzmax must be 1) */
			num_dims = 2;
			nzmax = 1;
			
			if(mxIsEmpty(ret_var))
			{
				releaseProcLock();
				readErrorMex("InvalidSparseError", "Detached sparse was unexpectedly empty.");
			}
			
			/* allocate 1 element */
			new_pr = mxCalloc(nzmax, mxGetElementSize(ret_var));
			mxSetData(ret_var, new_pr);
			if(mxIsComplex(ret_var))
			{
				new_pi = mxCalloc(nzmax, mxGetElementSize(ret_var));
				mxSetImagData(ret_var, new_pi);
			}
			
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
		for(link = ((mxArrayStruct*) ret_var)->CrossLink;
		    link != NULL && link != ret_var; link = ((mxArrayStruct*) link)->CrossLink)
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
	return shmRewrite_(shm_anchor + padToAlign(sizeof(SegmentMetadata_t)), in_var) +
		  padToAlign(sizeof(SegmentMetadata_t));
}


size_t shmRewrite_(byte_t* shm_anchor, const mxArray* in_var)
{
	size_t idx, count;
	
	/* for working with payload */
	Header_t* hdr;
	ShmData_t data_ptrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*) shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	/* Structure case */
	if(hdr->classid == mxSTRUCT_CLASS)
	{
		/* Go through each element */
		for(field_num = 0, count = 0; field_num < hdr->num_fields; field_num++)     /* each field */
		{
			for(idx = 0; idx < hdr->num_elems; idx++, count++)
			{
				/* And fill it */
				shmRewrite_(shm_anchor + data_ptrs.child_hdrs[count], mxGetFieldByNumber(in_var, idx, field_num));
			}
		}
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		for(count = 0; count < hdr->num_elems; count++)
		{
			/* And fill it */
			shmRewrite_(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(in_var, count));
		}
	}
	else     /*base case*/
	{
		
		/* if sparse get a list of the elements */
		if(hdr->is_sparse)
		{
			/* rewrite real data */
			memcpy(data_ptrs.pr, mxGetData(in_var), (hdr->nzmax) * (hdr->elem_size));
			
			/* if complex get a pointer to the complex data */
			if(hdr->complexity == mxCOMPLEX)
			{
				memcpy(data_ptrs.pi, mxGetImagData(in_var), (hdr->nzmax) * (hdr->elem_size));
			}
			
			memcpy(data_ptrs.ir, mxGetIr(in_var), (hdr->nzmax) * sizeof(mwIndex));
			memcpy(data_ptrs.jc, mxGetJc(in_var), ((data_ptrs.dims)[1] + 1) * sizeof(mwIndex));
		}
		else
		{
			/* rewrite real data */
			memcpy(data_ptrs.pr, mxGetData(in_var), (hdr->num_elems) * (hdr->elem_size));
			
			/* if complex get a pointer to the complex data */
			if(hdr->complexity == mxCOMPLEX)
			{
				memcpy(data_ptrs.pi, mxGetImagData(in_var), (hdr->num_elems) * (hdr->elem_size));
			}
		}
		
	}
	
	return hdr->obj_sz;
	
}


bool_t shmCompareSize(byte_t* shm_anchor, const mxArray* comp_var)
{
	return shmCompareSize_(shm_anchor + padToAlign(sizeof(SegmentMetadata_t)), comp_var);
}


bool_t shmCompareSize_(byte_t* shm_anchor, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	
	/* for working with payload ... */
	Header_t* hdr;
	ShmData_t data_ptrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*) shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
	const char_t* field_name;
	
	if(mxGetClassID(comp_var) != hdr->classid)
	{
		return FALSE;
	}
	
	/* the size pointer */
	/* eventually eliminate this check for sparses */
	if(hdr->num_dims != mxGetNumberOfDimensions(comp_var) ||
	   memcmp(data_ptrs.dims, mxGetDimensions(comp_var), sizeof(mwSize) * hdr->num_dims) != 0)
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
				if(!shmCompareSize_(shm_anchor + data_ptrs.child_hdrs[count],
								mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
			
			getNextFieldName(&field_name);
			
		}
		
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			if(!shmCompareSize_(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(comp_var, count)))
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
			if(hdr->nzmax != mxGetNzmax(comp_var) || (data_ptrs.dims)[1] != mxGetN(comp_var))
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
	return shmCompareContent_(shm_anchor + padToAlign(sizeof(SegmentMetadata_t)), comp_var);
}


mxLogical shmCompareContent_(byte_t* shm_anchor, const mxArray* comp_var)
{
	
	/* for working with shared memory ... */
	size_t idx, count;
	
	/* for working with payload ... */
	Header_t* hdr;
	ShmData_t data_ptrs;
	
	/* for structures */
	int field_num;                /* current field */
	
	/* retrieve the data */
	hdr = (Header_t*) shm_anchor;
	locateDataPointers(&data_ptrs, hdr, shm_anchor);
	
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
				
				if(!shmCompareContent_(shm_anchor + data_ptrs.child_hdrs[count],
								   mxGetFieldByNumber(comp_var, idx, field_num)))
				{
					return FALSE;
				}
			}
			
			getNextFieldName(&field_name);
			
		}
		
	}
	else if(hdr->classid == mxCELL_CLASS) /* Cell case */
	{
		
		for(count = 0; count < hdr->num_elems; count++)
		{
			if(!shmCompareContent_(shm_anchor + data_ptrs.child_hdrs[count], mxGetCell(comp_var, count)))
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
			
			if(data_ptrs.ir != mxGetIr(comp_var) || data_ptrs.jc != mxGetJc(comp_var))
			{
				return FALSE;
			}
			
		}
		
	}
	
	return TRUE;
	
}




