#include "mex.h"

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{	
    size_t x = 0xFFFFFFFF;
	mexPrintf("%lu\n",x >> 100);	
}
