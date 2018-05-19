#include "mex.h"

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	byte_T* mem = mxMalloc(1);
	if(*((uint16_T*)(mem - 4)) == 0xFEED)
	{
		
	}
	else if(*((uint32_T*)(mem - 8)) == 0xFEEDFACE)
	{
		
	}
	else
	{
		
	}
	
}