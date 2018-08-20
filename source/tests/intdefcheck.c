#include "mex.h"

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	plhs[0] = mxCreateLogicalScalar((-1 == ~0) && ((-1 >> 1) == -1));
}