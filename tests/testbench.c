#include "mex.h"

extern mxArray* mxCreateSharedDataCopy(mxArray *);

typedef struct {
	void *name;             /*   prev - R2008b: Name of variable in workspace
	 * R2009a - R2010b: NULL
	 * R2011a - later : Reverse CrossLink pointer    */
	mxClassID ClassID;      /*  0 = unknown     10 = int16
	 * 1 = cell        11 = uint16
	 * 2 = struct      12 = int32
	 * 3 = logical     13 = uint32
	 * 4 = char        14 = int64
	 * 5 = void        15 = uint64
	 * 6 = double      16 = function_handle
	 * 7 = single      17 = opaque (classdef)
	 * 8 = int8        18 = object (old style)
	 * 9 = uint8       19 = index (deprecated)
	 * 10 = int16       20 = sparse (deprecated)     */
	int VariableType;       /*  0 = normal
	 * 1 = persistent
	 * 2 = global
	 * 3 = sub-element (field or cell)
	 * 4 = temporary
	 * 5 = (unknown)
	 * 6 = property of opaque class object
	 * 7 = (unknown)                                */
	mxArray *CrossLink;     /* Address of next shared-data variable          */
	size_t ndim;            /* Number of dimensions                          */
	unsigned int RefCount;  /* Number of extra sub-element copies            */
	unsigned int flags;     /*  bit  0 = is scalar double full
	 * bit  2 = is empty double full
	 * bit  4 = is temporary
	 * bit  5 = is sparse
	 * bit  9 = is numeric
	 * bits 24 - 31 = User Bits                     */
	union mdim_data{
		size_t M;           /* Row size for 2D matrices, or                  */
		size_t *dims;       /* Pointer to dims array for nD > 2 arrays       */
	} Mdims;
	size_t N;               /* Product of dims 2:end                         */
	void *pr;               /* Real Data Pointer (or cell/field elements)    */
	void *pi;               /* Imag Data Pointer (or field information)      */
	union ir_data{
		mwIndex *ir;        /* Pointer to row values for sparse arrays       */
		mxClassID ClassID;  /* New User Defined Class ID (classdef)          */
		char *ClassName;    /* Pointer to Old User Defined Class Name        */
	} irClassNameID;
	union jc_data{
		mwIndex *jc;        /* Pointer to column values for sparse arrays    */
		mxClassID ClassID;  /* Old User Defined Class ID                     */
	} jcClassID;
	size_t nzmax;           /* Number of elements allocated for sparse       */
	/*  size_t reserved;           Don't believe this! It is not really there!   */
} mxArrayStruct;

mxArray* sps;

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	
// 	mxArray* in = mxCreateDoubleMatrix(3,3,mxREAL);
// 	mxArray* shared = mxCreateSharedDataCopy(in);
// 	mxArray* in = prhs[0];
// 	mxArrayStruct* in_tag = (mxArrayStruct*)in;
// 	mxArray* in2 = prhs[1];
// 	mxArrayStruct* in_tag2 = (mxArrayStruct*)in2;
// 	mxArrayStruct* shared_tag = (mxArrayStruct*)shared;
    
    plhs[0] = mxCreateSparse(0,0,1,mxREAL);
    mexPrintf("%d\n",mxGetNumberOfElements(plhs[0]));
    mexPrintf("%d\n",mxGetNzmax(plhs[0]));
    mexPrintf("%d\n",mxIsEmpty(plhs[0]));
    
// 	plhs[0] = mxCreateStructArray(0,NULL,0,NULL);
// 	
// 	if(nrhs > 0)
// 	{
// 		if(*((double*)mxGetData(prhs[0])) == 0.0)
// 		{
// 			sps = mxCreateSparse(2, 3, 7, mxREAL);
// 			mxArrayStruct* in_tag = (mxArrayStruct*)sps;
// 			mexPrintf("addr in %p\n", in_tag);
// 			mexPrintf("addr in crosslink %p\n", in_tag->CrossLink);
// 			mexPrintf("addr in dat %p\n\n", in_tag->pr);
// 
// 			mwSize dims[] = {2,2};
// 			mwSize nzmax = 5;
// 			mxSetDimensions(sps, dims, 2);
// 			mxSetNzmax(sps, nzmax);
// 			mexMakeArrayPersistent(sps);
// 			plhs[0] = mxCreateSharedDataCopy(sps);
// 		}
// 		else if(*((double*)mxGetData(prhs[0])) == 1.0)
// 		{
// 			mxDestroyArray(sps);
// 		}
// 	}
//
// 	mexPrintf("addr shared %p\n", shared_tag);
// 	mexPrintf("addr shared crosslink %p\n", shared_tag->CrossLink);
// 	mexPrintf("addr shared dat %p\n\n\n", shared_tag->pr);
	
// 		mxFree(mxGetData(in));
// 		mxSetData(in, mxMalloc(9));
//
// 		mexPrintf("addr in %p\n", in_tag);
// 		mexPrintf("addr in crosslink %p\n", in_tag->CrossLink);
// 		mexPrintf("addr in dat %p\n\n", in_tag->pr);
//
// 		mexPrintf("addr shared %p\n", shared_tag);
// 		mexPrintf("addr shared crosslink %p\n", shared_tag->CrossLink);
// 		mexPrintf("addr shared dat %p\n\n\n", shared_tag->pr);
//
// 		mxDestroyArray(in);
//
// 		mexPrintf("addr shared %p\n", shared_tag);
// 		mexPrintf("addr shared crosslink %p\n", shared_tag->CrossLink);
// 		mexPrintf("addr shared dat %p\n", shared_tag->pr);
//
// 		mxDestroyArray(shared);
// 	int bytes = (int)*(double*)mxGetData(prhs[0]);
// 	mexPrintf("%i",bytes);
// 	uint8_T* mem = mxMalloc(bytes);
// 	int i;
// 	for(i=16;i>0;i--)
// 	{
// 		mexPrintf("%X\n",*(mem-i));
// 	}
// 	plhs[0] = mxCreateNumericMatrix(1, 8, mxUINT8_CLASS, mxREAL);
// 	memcpy(mxGetData(plhs[0]), mem - 16, 8);
// 	plhs[1] = mxCreateDoubleScalar((double)((size_t)mem));
// 	plhs[2] = mxCreateDoubleScalar((double)((size_t)(mem+bytes)));
// 	mxFree(mem);
	
// 	mexPrintf("\n");
	
// 	mem = malloc(1);
// 	*mem = 7;
// 	for(i=16;i>0;i--)
// 	{
// 		mexPrintf("%X\n",*(mem-i));
// 	}
//  	plhs[3] = mxCreateDoubleScalar(17);
// 	mxSetData(plhs[3],mem);
	//mxArrayStruct* arr = (mxArrayStruct*)plhs[0];
	//arr->pr = mem;
	//memcpy(mxGetData(plhs[0]), mem-16,16);
}