#include "mex.h"

static void msh_AddInt32(int32_T* accum, int32_T* addend, size_t num_elems);

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	msh_AddInt32(mxGetData(prhs[0]), mxGetData(prhs[1]), mxGetNumberOfElements(prhs[0]));
}

static void msh_AddInt32(int32_T* accum, int32_T* addend, size_t num_elems)
{
	size_t i;
	int32_T unsigned_add, flip_check1, flip_check2;
	
	/* saturated arithmetic testbench */
	for(i = 0; i < num_elems; i++, accum++, addend++)
	{
		unsigned_add = (uint32_T)*accum + (uint32_T)*addend;
		
		flip_check1 = unsigned_add ^ *accum;
		flip_check2 = unsigned_add ^ *addend;
		
		if((flip_check1 & flip_check2) < 0)
		{
			*accum = 0x7FFFFFFF + (~unsigned_add >> 31);
		}
		else
		{
			*accum = unsigned_add;
		}
	}
}
