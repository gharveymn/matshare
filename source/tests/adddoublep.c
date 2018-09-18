#include "mex.h"

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	size_t i;
	
	size_t num_elems = mxGetNumberOfElements(prhs[0]);
	double* accum = mxGetData(prhs[0]);
	double* addend = mxGetData(prhs[1]);
	
	for(i = 0; i < num_elems; i++, accum++, addend++)
	{
		*accum += *addend;
	}	
}