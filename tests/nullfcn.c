#include "mex.h"

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	int i;
	for(i = 0; i < nlhs; i++)
	{
		plhs[i] = mxCreateDoubleMatrix(1,1,mxREAL);
	}
}