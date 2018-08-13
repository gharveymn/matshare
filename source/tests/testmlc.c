#include "mex.h"

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{	
	int i;
	void* alc;
	for(i = 0; i < 10000000; i++)
	{
		alc = malloc(100);
		free(alc);
	}
}