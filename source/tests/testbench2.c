#include "mex.h"

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{	
	const char* fn[] = {"hi", "bye", "hi"};
	
	if(nlhs > 0)
	{
		//plhs[0] = mxCreateStructMatrix(1, 1, 3, fn);
		plhs[0] = mxCreateStructMatrix(1, 1, 0, NULL);
		mxGetField(plhs[0], 0, "abc");
		mxAddField(plhs[0], fn[0]);
		mxAddField(plhs[0], fn[1]);
		mxAddField(plhs[0], fn[2]);
	}
	
}