#include "headers/MatShare.hpp"

/* shared memory for windows - shared mem is destroyed when no threads are attached to it*/
using namespace boost::interprocess;

/* define the segment container with static linkage */
/* shared memory will be destroyed when the shared memory is removed from this
(or this is destroyed).  Each calling process will have its own version of this variable */
static shared_mem_stack SegmentBuffer;


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
	
	/* for working with shared memory ... */
	windows_shared_memory* pSegment;             /* pointer to the segment that interfaces the shared memory */
	mapped_region* pRegion;                      /* pointer to the memory map for the shared memory */
	int segIndex;                                /* which segment in the current process? */
	size_t sm_size;                              /* size required by the shared memory */
	
	/* for storing the mxArrays ... */
	header_t hdr;
	data_t dat;
	
	/* check min number of arguments */
	if(nrhs < 2)
	{
		mexErrMsgIdAndTxt("MATLAB:SharedMemory", "Minimum input arguments missing; must supply directive and key.");
	}
	
	/* assign inputs */
	mxDirective = prhs[0];
	
	/* get directive */
	directive = (msh_directive_t)mxGetData(mxDirective);
	
	/* Switch yard {clone, attach, detach, free} */
	switch(directive)
	{
		case msh_INIT:
			init();
			plhs[0] = global_shared_variable;
			break;
		case msh_CLONE:
			/********************/
			/*	Clone case		*/
			/********************/
			
			/* check the inputs */
			if((nrhs < 3) || (mxIsEmpty(prhs[2])))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone",
							   "Required third argument is missing (variable) or empty.");
			}
			
			/* Assign */
			mxInput = prhs[1];
			
			/* scan input data */
			sm_size = deepscan(&hdr, &dat, mxInput, nullptr, global_shared_variable);

#ifdef DEBUG
			mexPrintf("clone: Debug: deepscan done.\n");
#endif
			
			/* create the segment */
			segIndex = SegmentBuffer.findSegmentByName(MATSHARE_SEGMENT_NAME);
			if(segIndex < 0)
			{
				SegmentBuffer.removeSegmentByIndex(segIndex);
			}
			
			try
			{
				pSegment = new windows_shared_memory(open_or_create, MATSHARE_SEGMENT_NAME, read_write, sm_size);
			} /* changed from 'create_only' to 'open_or_create' per user Guy Katz suggestion */
			catch(interprocess_exception &ex)
			{
				mexPrintf("%i) %s\n", ex.get_error_code(), ex.what());
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone",
							   "SharedMemory::Could not create the memory segment (name conflict?)");
			}
			
			/* attach the segment to this dataspace */
			pRegion = new mapped_region(*pSegment, read_write);
			if(pRegion->get_address() == nullptr)
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone",
							   "SharedMemory::Unable to attach shared memory to data space.");
			}
			
			/* Successful, so add it to the shared memory stack so its preserved after this function exits */
			segIndex = SegmentBuffer.addSegmentToBuffer(pSegment, pRegion);
			
			/* copy data to the shared memory */
			deepcopy(&hdr, &dat, (char*) pRegion->get_address(), nullptr);
			
			/* free temporary allocation */
			deepfree(&dat);

#ifdef DEBUG
			mexPrintf("clone: Debug: shared memory at: 0x%016x\n", pRegion->get_address());
#endif
			
			plhs[0] = global_shared_variable;
			break;
		case msh_ATTACH:
			/********************/
			/*	Attach case	*/
			/********************/
			
			/* Has this shared memory segment been mapped in this region? */
			segIndex = SegmentBuffer.findSegmentByName(MATSHARE_SEGMENT_NAME);
			
			if(segIndex < 0)
			{
				/* It hasn't, so attach to the shared memory segment */
				try
				{
					pSegment = new windows_shared_memory(open_only, MATSHARE_SEGMENT_NAME, read_write);
				}
				catch(interprocess_exception &ex)
				{
					mexPrintf("%i) %s\n", ex.get_error_code(), ex.what());
					mexErrMsgIdAndTxt("MATLAB:SharedMemory:attach",
								   "SharedMemory::Could not attach to the memory segment (does it exist?)");
				}
				
				/* Map the whole shared memory in this process */
				pRegion = new mapped_region(*pSegment, read_write);
				
				/* Check the validity */
				if(nullptr == pRegion->get_address())
				{
					mexErrMsgIdAndTxt("MATLAB:SharedMemory:attach",
								   "SharedMemory::Unable to attach shared memory to data space.");
				}
				
				/* Successful, so add it to the shared memory stack so its preserved after this function exits */
				segIndex = SegmentBuffer.addSegmentToBuffer(pSegment, pRegion);
				
			}
			else
			{
				/* It has, so retrieve the mapping */
				pSegment = SegmentBuffer.getSegmentByIndex(segIndex);
				pRegion = SegmentBuffer.getRegionByIndex(segIndex);
			}

#ifdef DEBUG
			mexPrintf("attach: Debug: shared memory at: 0x%016x\n",pRegion->get_address());
#endif
			
			/* Restore the segments */
			shallowrestore((char*) pRegion->get_address(), &mxOutput);
			
			/* Tell the segment buffer that a matlab variables is "attached" to it */
			SegmentBuffer.addReferenceByIndex(segIndex);

#ifdef DEBUG
			mexPrintf("attach: Debug: done\n");
#endif
			
			
			break;
		case msh_DETACH:
			/********************/
			/*	Dettach case	*/
			/********************/
			
			/* check validity */
			if((nrhs < 3) || mxIsEmpty(prhs[0]))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:detach",
							   "Required third argument is missing (variable) or empty.");
			}
			
			/* Assign */
			mxInput = prhs[2];
			
			/* Find the segment */
			segIndex = SegmentBuffer.findSegmentByName(MATSHARE_SEGMENT_NAME);
			
			/* check the mapping exists */
			if(segIndex < 0)
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:detach",
							   "There is no shared segment with this name in this process.");
			}
			
			if(SegmentBuffer.getRefCountByIndex(segIndex) < 1)
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:detach",
							   "There are no variables attached to the shared memory in this process.");
			}
			
			/* NULL all of the Matlab pointers */
			deepdetach((mxArray*) mxInput);               /* Abuse const */
			
			/* Tell the segment buffer that a matlab variables is "dettached" to it */
			SegmentBuffer.removeReferenceByIndex(segIndex);
			
			break;
		case msh_FREE:
			/********************/
			/*	free case		*/
			/********************/
			
			/* Find the segment */
			segIndex = SegmentBuffer.findSegmentByName(MATSHARE_SEGMENT_NAME);
			
			/* check the mapping exists */
			if(segIndex < 0)
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:free",
							   "There are no shared segments with this name in this process.");
			}
			
			if(SegmentBuffer.getRefCountByIndex(segIndex) > 0)
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:free",
							   "There are still variables attached to the shared memory in this process.");
			}
			
			/* remove the segment */
			SegmentBuffer.removeSegmentByIndex(segIndex);
			
			break;
		default:
			mexErrMsgIdAndTxt("MATLAB:SharedMemory", "Unrecognized directive.");
			/* unrecognised directive */
			break;
	}
	
	
}



void init()
{
	global_shared_variable = mxCreateDoubleMatrix(0,0,mxREAL);
	mexMakeArrayPersistent(global_shared_variable); /* freed by Matlab Memory Manager */
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
void deepdetach(mxArray* mxInput)
{
	
	/* uses side-effects! */
	mwSize dims[] = {0, 0};
	mwSize nzmax = 0;
	size_t elemsiz;
	size_t i, n;
	
	/*for structure */
	size_t nFields, j;
	
	/* restore matlab  memory */
	if(mxInput == (mxArray*) nullptr || mxIsEmpty(mxInput))
	{
		return;
	}
	else if(mxIsStruct(mxInput))
	{
		/* detach each field for each element */
		n = mxGetNumberOfElements(mxInput);
		nFields = mxGetNumberOfFields(mxInput);
		for(i = 0; i < n; i++)                         /* element */
		{
			for(j = 0; j < nFields; j++)               /* field */
			{
				deepdetach((mxArray*) (mxGetFieldByNumber(mxInput, i, j)));
			}/* detach this one */
		}
	}
	else if(mxIsCell(mxInput))
	{
		/* detach each cell */
		n = mxGetNumberOfElements(mxInput);
		for(i = 0; i < n; i++)
		{
			deepdetach((mxArray*) (mxGetCell(mxInput, i)));
		}/* detach this one */
		
	}
	else if(mxIsNumeric(mxInput) || mxIsLogical(mxInput) || mxIsChar(mxInput))  /* a matrix containing data */
	{
		
		/* In safe mode these entries were allocated so remove them properly */
#ifdef SAFEMODE
		mxFree(mxGetData(mxInput));
		mxFree(mxGetImagData(mxInput));
#endif
		
		/* handle sparse objects */
		if(mxIsSparse(mxInput))
		{
			/* I don't seem to be able to give sparse arrays zero size so (nzmax must be 1) */
			dims[0] = dims[1] = 1;
			nzmax = 1;
			if(mxSetDimensions(mxInput, dims, 2))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:Unknown", "detach: unable to resize the array.");
			}
			
			/* In safe mode these entries were allocated so remove them properly */
#ifdef SAFEMODE
			mxFree(mxGetIr(mxInput));
			mxFree(mxGetJc(mxInput));
#endif
			
			/* allocate 1 element */
			elemsiz = mxGetElementSize(mxInput);
			mxSetData(mxInput, mxCalloc(nzmax, elemsiz));
			if(mxIsComplex(mxInput))
			{
				mxSetImagData(mxInput, mxCalloc(nzmax, elemsiz));
			}
			else
			{
				mxSetImagData(mxInput, (void*) nullptr);
			}
			
			/* allocate 1 element */
			mxSetNzmax(mxInput, nzmax);
			mxSetIr(mxInput, (mwSize*) mxCalloc(nzmax, sizeof(mwIndex)));
			mxSetJc(mxInput, (mwSize*) mxCalloc(dims[1] + 1, sizeof(mwIndex)));
		}
		else
		{
			/* Can have zero size, so nullify data storage containers */
			if(mxSetDimensions(mxInput, dims, 2))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:Unknown", "detach: unable to resize the array.");
			}
			
			/* Doesn't allocate or deallocate any space for the pr or pi arrays */
			mxSetData(mxInput, (void*) nullptr);
			mxSetImagData(mxInput, (void*) nullptr);
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
size_t shallowrestore(char* shm, mxArray** p_mxInput)
{
	
	/* for working with shared memory ... */
	size_t i, shmsiz;
	mxArray* mxChild;   /* temporary pointer */
	
	/* for working with payload ... */
	header_t* hdr;
	mwSize* pSize;             /* pointer to the array dimensions */
	void* pr = nullptr, * pi = nullptr;  /* real and imaginary data pointers */
	mwIndex* ir = nullptr, * jc = nullptr;  /* sparse matrix data indices */
	
	
	/* for structures */
	int ret;
	size_t nFields;                     /* number of fields */
	size_t field_num;                /* current field */
	size_t strBytes;                /* Number of bytes in the string recording the field names */
	char** pFieldStr;           /* Array to pass to Matlab with the field names pFields[0] is a poitner to the first field name, pFields[1] is the pointer to the second*/
	
	/* for testing ... */
	static double pBuffer[50];
	for(int i = 0; i < 50; i++)
	{
		pBuffer[i] = i;
	}
	
	/* retrieve the data */
	hdr = (header_t*) shm;
	shm += sizeof(header_t);
	
	/* the size pointer */
	pSize = (mwSize*) shm;
	
	/* skip over the stored sizes */
	shm += pad_to_align(sizeof(mwSize) * hdr->nDims);
	
	
	/* Structure case */
	if(hdr->isStruct)
	{
#ifdef DEBUG
		mexPrintf("shallowrestore: Debug: found structure %d x %d.\n", pSize[0], pSize[1]);
#endif
		
		/* Pull out the field names, they follow the size*/
		ret = NumFieldsFromString(shm, &nFields, &strBytes);
		
		/* check the recovery */
		if((ret) || (nFields != hdr->nFields))
		{
			mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone", "Structure fields have not been recovered properly");
		}
		
		/* And convert to something matlab understands */
		pFieldStr = (char**) mxCalloc(nFields, sizeof(char*));
		PointCharArrayAtString(pFieldStr, shm, nFields);
		
		/* skip over the stored field string */
		shm += strBytes;
		
		/*create the matrix */
		*p_mxInput = mxCreateStructArray(hdr->nDims, pSize, hdr->nFields, (const char**) pFieldStr);
		mxFree(pFieldStr);  /* should be able to do this now... */
		
		/* Go through each element */
		for(i = 0; i < hdr->nzmax; i++)
		{
			for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
			{
#ifdef DEBUG
				mexPrintf("shallowrestore: Debug: working on %d field %d at 0x%016x.\n",i,field_num, *p_mxInput);
#endif
				/* And fill it */
				shmsiz = shallowrestore(shm, &mxChild);                           /* restore the mxArray */
				mxSetFieldByNumber(*p_mxInput, i, field_num, mxChild);    /* and pop it in	   */
				shm += shmsiz;

#ifdef DEBUG
				mexPrintf("shallowrestore: Debug: completed %d field %d.\n",i, field_num);
#endif
			}
		}
	}
	else if(hdr->isCell) /* Cell case */
	{
#ifdef DEBUG
		mexPrintf("shallowrestore: Debug: found cell %d x %d.\n",pSize[0], pSize[1]);
#endif
		
		/* Create the array */
		*p_mxInput = mxCreateCellArray(hdr->nDims, pSize);
		for(i = 0; i < hdr->nzmax; i++)
		{
#ifdef DEBUG
			mexPrintf("shallowrestore: Debug: working on %d.\n",i);
#endif
			/* And fill it */
			shmsiz = shallowrestore(shm, &mxChild);
			mxSetCell(*p_mxInput, i, mxChild);
			shm += shmsiz;
#ifdef DEBUG
			mexPrintf("shallowrestore: Debug: completed %d at 0x%016x.\n",i,mxChild);
#endif
		}
	}
	else     /*base case*/
	{
		/* this is the address of the first data */
		pr = (void*) shm;
		shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));          /* takes us to the end of the real data */
		
		/* if complex get a pointer to the complex data */
		if(hdr->complexity == mxCOMPLEX)
		{
			pi = (void*) shm;
			shm += pad_to_align((hdr->nzmax) * (hdr->elemsiz));     /* takes us to the end of the complex data */
		}
		
		/* if sparse get a list of the elements */
		if(hdr->isSparse)
		{
#ifdef DEBUG
			mexPrintf("shallowrestore: Debug: found non-cell, sparse 0x%016x.\n",*p_mxInput);
#endif
			
			ir = (mwIndex*) shm;
			shm += pad_to_align((hdr->nzmax) * sizeof(mwIndex));
			
			jc = (mwIndex*) shm;
			shm += pad_to_align((pSize[1] + 1) * sizeof(mwIndex));
			
			/* FIX: need to create sparse logical differently */
			if(hdr->classid == mxLOGICAL_CLASS)
			{
				*p_mxInput = mxCreateSparseLogicalMatrix(0, 0, 0);
			}
			else
			{
				*p_mxInput = mxCreateSparse(0, 0, 0, hdr->complexity);
			}
			
			/* Free the memory it was created with */
			mxFree(mxGetIr(*p_mxInput));
			mxFree(mxGetJc(*p_mxInput));
			
			/* set the size */
			if(mxSetDimensions(*p_mxInput, pSize, hdr->nDims))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:Unknown", "attach: unable to resize the sparse array.");
			}
				
				/* set the pointers relating to sparse (set the real and imaginary data later)*/
#ifndef SAFEMODE
				
				/* Hack  Method */
				// ((Internal_mxArray*)*p_mxInput)->data.number_array.reserved5    = ir;
				// ((Internal_mxArray*)*p_mxInput)->data.number_array.reserved6[0] = (size_t)jc;
				// ((Internal_mxArray*)*p_mxInput)->data.number_array.reserved6[1] = (size_t)(hdr->nzmax);

#else
				
				/* Safe - but takes it out of global memory */
				mxSetIr(*p_mxInput, (mwIndex*)safeCopy(ir, (hdr->nzmax)*sizeof(mwIndex)));
				mxSetJc(*p_mxInput, (mwIndex*)safeCopy(jc, (pSize[1]+1)*sizeof(mwIndex)));

#endif
			mxSetNzmax(*p_mxInput, hdr->nzmax);
			
		}
		else
		{
			
			/*  rebuild constitute the array  */
#ifdef DEBUG
			mexPrintf("shallowrestore: Debug: found non-cell, non-sparse.\n");
#endif
			
			/*  create an empty array */
			*p_mxInput = mxCreateNumericMatrix(0, 0, hdr->classid, hdr->complexity);
			
			/* set the size */
			if(mxSetDimensions(*p_mxInput, pSize, hdr->nDims))
			{
				mexErrMsgIdAndTxt("MATLAB:SharedMemory:Unknown", "attach: unable to resize the array.");
			}
		}
		
		
		/* Free the memory it was created with (shouldn't be necessary) */
		mxFree(mxGetPr(*p_mxInput));
		mxFree(mxGetPi(*p_mxInput));
		
		/* set the real and imaginary data */
#ifndef SAFEMODE
		
		/* Hack  Method */
		/* ((Internal_mxArray*)(*p_mxInput))->data.number_array.pdata = pBuffer;  / * works assuming the size is <= 50 * / */
		// ((Internal_mxArray*)(*p_mxInput))->data.number_array.pdata = pr;		 /* Hack */
		// if ( hdr->complexity )
		// {	((Internal_mxArray*)(*p_mxInput))->data.number_array.pimag_data = pi;   }

#else
		
		/* Safe - but takes it out of global memory */
		mxSetData(*p_mxInput, safeCopy(pr, hdr->nzmax*hdr->elemsiz));
		if ( hdr->complexity )
		{	mxSetImagData(*p_mxInput, safeCopy(pi, hdr->nzmax*hdr->elemsiz));  }
#endif
	
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
size_t deepscan(header_t* hdr, data_t* dat, const mxArray* mxInput, header_t* par_hdr, mxArray* ret_var)
{
	/* counter */
	size_t i;
	
	/* for structures */
	size_t field_num, count;
	size_t strBytes;
	
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
		hdr->shmsiz = sizeof(header_t);
		
		return hdr->shmsiz;
	}
	
	/* initialize header info */
	hdr->isNumeric = mxIsNumeric(mxInput);
	hdr->isSparse = mxIsSparse(mxInput);
	hdr->complexity = mxIsComplex(mxInput)? mxCOMPLEX : mxREAL;
	hdr->classid = mxGetClassID(mxInput);
	hdr->nDims = mxGetNumberOfDimensions(mxInput);
	hdr->elemsiz = mxGetElementSize(mxInput);
	hdr->nzmax = mxGetNumberOfElements(mxInput);     /* update this later on sparse*/
	hdr->nFields = 0;                                             /* update this later */
	hdr->shmsiz = sizeof(header_t);                         /* update this later */
	
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
		strBytes = (size_t)FieldNamesSize(mxInput);     /* always a multiple of alignment size */
		hdr->shmsiz += strBytes;                   /* Add space for the field string */
		
		/* use mxCalloc so mem is freed on error via garbage collection */
		dat->child_hdr = (header_t*) mxCalloc(
				hdr->nzmax * hdr->nFields * (sizeof(header_t) + sizeof(data_t)) + strBytes,
				1); /* extra space for the field string */
		dat->child_dat = (data_t*) &dat->child_hdr[hdr->nFields * hdr->nzmax];
		dat->field_str = (char*) &dat->child_dat[hdr->nFields * hdr->nzmax];
		
		const char** field_names = (const char**)mxMalloc(hdr->nFields*sizeof(char*));
		
		/* make a record of the field names */
		CopyFieldNames(mxInput, dat->field_str, field_names);
		
		ret_var = mxCreateStructArray(hdr->nDims, dat->pSize, hdr->nFields, field_names);
		mexMakeArrayPersistent(ret_var);
		
		/* go through each recursively */
		count = 0;
		for(i = 0; i < hdr->nzmax; i++)                         /* each element */
		{
			for(field_num = 0; field_num < hdr->nFields; field_num++)     /* each field */
			{
				/* call recursivley */
				hdr->shmsiz += deepscan(&(dat->child_hdr[count]), &(dat->child_dat[count]),
								    mxGetFieldByNumber(mxInput, i, field_num), hdr, mxGetFieldByNumber(ret_var, i, field_num));
				
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
		
		ret_var = mxCreateCellArray(hdr->nDims, dat->pSize);
		mexMakeArrayPersistent(ret_var);
		
		/* go through each recursively */
		for(i = 0; i < hdr->nzmax; i++)
		{
			hdr->shmsiz += deepscan(&(dat->child_hdr[i]), &(dat->child_dat[i]), mxGetCell(mxInput, i), hdr, mxGetCell(ret_var, i));
#ifdef DEBUG
			mexPrintf("deepscan: Debug: finished %d.\n",i);
#endif
		}
		
		/* if its a cell it has to have at least one mom-empty element */
		if(hdr->shmsiz == (1 + hdr->nzmax) * sizeof(header_t))
		{
			mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone", "Required third argument (variable) must contain at "
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
				ret_var = mxCreateSparse(mxGetM(mxInput), mxGetN(mxInput), 1, hdr->complexity);
			}
			else
			{
				ret_var = mxCreateSparseLogicalMatrix(mxGetM(mxInput), mxGetN(mxInput), 1);
			}
			
			mxFree(mxGetIr(ret_var));
			mwIndex* ret_ir = (mwIndex*)mxMalloc(hdr->nzmax * sizeof(mwIndex));
			memcpy(ret_ir, dat->ir, hdr->nzmax * sizeof(mwIndex));
			mxSetIr(ret_var, ret_ir);
			
			mwIndex* ret_jc = mxGetJc(ret_var);
			memcpy(ret_jc, dat->jc, hdr->nzmax * sizeof(mwIndex));
			
			
		}
		else
		{
			if(hdr->isNumeric)
			{
				ret_var = mxCreateNumericArray(0, nullptr, hdr->classid, hdr->complexity);
				mxSetDimensions(ret_var, dat->pSize, hdr->nDims);
			}
			else if(hdr->classid == mxLOGICAL_CLASS)
			{
				ret_var = mxCreateLogicalArray(0, nullptr);
				mxSetDimensions(ret_var, dat->pSize, hdr->nDims);
			}
			else
			{
				ret_var = mxCreateCharArray(0, nullptr);
				mxSetDimensions(ret_var, dat->pSize, hdr->nDims);
			}
		}
		mexMakeArrayPersistent(ret_var);
		
		/* ensure both pointers are aligned individually */
		hdr->shmsiz += pad_to_align(hdr->elemsiz * hdr->nzmax);
		if(hdr->complexity == mxCOMPLEX)
		{
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
void deepcopy(header_t* hdr, data_t* dat, char* shm, header_t* par_hdr)
{
	
	header_t* cpy_hdr;          /* the header in its copied location */
	size_t i, n, offset = 0;
	mwSize* pSize;               /* points to the size data */
	
	/* for structures */
	size_t nFields, strBytes;
	int ret;
	
	/* load up the shared memory */
	
	/* copy the header */
	n = sizeof(header_t);
	memcpy(shm, hdr, n);
	offset = n;
	
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
	if(hdr->isStruct)
	{
#ifdef DEBUG
		mexPrintf("deepcopy: Debug: found structure.\n");
#endif
		
		/* place the field names next in shared memory */
		ret = NumFieldsFromString(dat->field_str, &nFields, &strBytes);
		
		/* check the recovery */
		if((ret) || (nFields != hdr->nFields))
		{
			mexErrMsgIdAndTxt("MATLAB:SharedMemory:clone", "Structure fields have not been recovered properly");
		}
		
		/* copy the field string */
		for(i = 0; i < strBytes; i++)
		{
			shm[i] = dat->field_str[i];
		}
		shm += strBytes;
		
		/* copy the children recursively */
		for(i = 0; i < hdr->nzmax * hdr->nFields; i++)
		{
			deepcopy(&(dat->child_hdr[i]), &(dat->child_dat[i]), shm, cpy_hdr);
			shm += (dat->child_hdr[i]).shmsiz;
		}
	}
	else if(hdr->isCell) /* Cell case */
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
		n = (hdr->nzmax) * (hdr->elemsiz);
		memcpy(shm, dat->pr, n);
		shm += pad_to_align(n);
		
		/* copy complex data as well */
		if(hdr->complexity == mxCOMPLEX)
		{
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
