#include "mex.h"

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{	
	mxGetIr(prhs[0])[0] = 5;	
}
