#include "mex.h"
#include "../src/headers/mshexterntypes.h"

extern mxArray* mxCreateSharedDataCopy(mxArray *);
void callback(void);
mxArray* persist = NULL;

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{	
	
	int i;
	
	mxArray* x,* link,* shared_data_copy;
	void* alloc;
	mwSize dimstmp[2] = {2,1};
	mwSize dims[2] = {1,1};
	const char* fn[] = {"hi, bye, hi"};

	if(nlhs > 0)
	{
		plhs[0] = mxCreateStructMatrix(1, 1, 3, fn);
	}
	
	if(nrhs > 0)
	{
		link = (mxArray*)prhs[0];
		do
		{
			mexPrintf("addr: %llu\n", link);
			mxSetDimensions(link, dims, 2);
			link = met_GetCrosslink(link);
		} while(link != NULL && link != prhs[0]);
		if(persist != NULL)
		{
			*((double*)mxGetData(persist)) = 2.0;
		}
		return;
	}
	
	if(persist == NULL)
	{
		persist = mxCreateDoubleMatrix(2, 1, mxREAL);
		//persist = mxCreateCellMatrix(1, 1);
		
		mexMakeArrayPersistent(persist);
		
		//mexPrintf("%u\n", mxIsEmpty(persist));
		
		//shared_data_copy = mxCreateSharedDataCopy(persist);
		
		//met_PutSharedCopy("caller", "x", shared_data_copy);
		
		if(nlhs > 0)
		{
			plhs[0] = mxCreateCellMatrix(1,1);
			
			//alloc = mxMalloc(1);
			//mxSetData(persist, alloc);
			
			shared_data_copy = mxCreateSharedDataCopy(persist);
			
			link = shared_data_copy;
			do
			{
				mexPrintf("addr1: %llu\n", link);
				link = met_GetCrosslink(link);
			} while(link != NULL && link != shared_data_copy);
			
			//mxSetData(persist, NULL);
			//mxSetData(link, NULL);
			
			mxSetCell(plhs[0], 0, shared_data_copy);
			
			link = mxGetCell(plhs[0], 0);
			do
			{
				mexPrintf("addr2: %llu\n", link);
				link = met_GetCrosslink(link);
			} while(link != NULL && link != shared_data_copy);
			
		}
	}
	else
	{
		link = persist;
		do
		{
			mexPrintf("addr: %llu\n", link);
			link = met_GetCrosslink(link);
		} while(link != NULL && link != persist);
		mxDestroyArray(persist);
		persist = NULL;
	}
	
// 	mxArray* in = mxCreateDoubleMatrix(3,3,mxREAL);
// 	mxArray* shared = mxCreateSharedDataCopy(in);
// 	mxArray* in = prhs[0];
// 	InternalMexStruct_t* in_tag = (InternalMexStruct_t*)in;
//	mxArray* out = mxCreateSharedDataCopy(prhs[0]);
//	InternalMexStruct_t* out_tag = (InternalMexStruct_t*)out;
//
//	mwSize dims[3] = {2,3,2};
//	mxSetDimensions(out,dims,3);
// 	mxArray* x = mxCreateDoubleMatrix(1,1,mxREAL);
// 	void* data = mxGetData(x);
// 	mxSetData(x, NULL);
// 	mxSetData(x, data);
// 	mxArray* in2 = prhs[1];
// 	InternalMexStruct_t* in_tag2 = (InternalMexStruct_t*)in2;
// 	InternalMexStruct_t* shared_tag = (InternalMexStruct_t*)shared;

//    mexPrintf("%d\n",in_tag->RefCount);


// 	plhs[0] = mxCreateStructArray(0,NULL,0,NULL);
//
// 	if(nrhs > 0)
// 	{
// 		if(*((double*)mxGetData(prhs[0])) == 0.0)
// 		{
// 			sps = mxCreateSparse(2, 3, 7, mxREAL);
// 			InternalMexStruct_t* in_tag = (InternalMexStruct_t*)sps;
// 			mexPrintf("addr in %p\n", in_tag);
// 			mexPrintf("addr in crosslink %p\n", in_tag->CrossLink);
// 			mexPrintf("addr in dat %p\n\n", in_tag->data);
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
// 	mexPrintf("addr shared dat %p\n\n\n", shared_tag->data);

// 		mxFree(mxGetData(in));
// 		mxSetData(in, mxMalloc(9));
//
// 		mexPrintf("addr in %p\n", in_tag);
// 		mexPrintf("addr in crosslink %p\n", in_tag->CrossLink);
// 		mexPrintf("addr in dat %p\n\n", in_tag->data);
//
// 		mexPrintf("addr shared %p\n", shared_tag);
// 		mexPrintf("addr shared crosslink %p\n", shared_tag->CrossLink);
// 		mexPrintf("addr shared dat %p\n\n\n", shared_tag->data);
//
// 		mxDestroyArray(in);
//
// 		mexPrintf("addr shared %p\n", shared_tag);
// 		mexPrintf("addr shared crosslink %p\n", shared_tag->CrossLink);
// 		mexPrintf("addr shared dat %p\n", shared_tag->data);
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
	//InternalMexStruct_t* arr = (InternalMexStruct_t*)plhs[0];
	//arr->data = mem;
	//memcpy(mxGetData(plhs[0]), mem-16,16);
}

void callback(void)
{
	mexPrintf("ran\n");
}