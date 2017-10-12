#include "headers/matshare.h"

ParamStruct param_struct;

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	if(nrhs < 1)
	{
		mexErrMsgIdAndTxt("matshare:notEnoughArguments", "Not enough arguments.");
	}
	
	if(mxIsChar(prhs[0]) != TRUE)
	{
		mexErrMsgIdAndTxt("matshare:invalidArgument", "The first argument must be a character vector.");
	}
	
	readInput(nrhs, prhs);
	
	/* General outline:
	 * 		two shared memory blocks
	 * 		one holding hashtable for object locations, can be put into persistent memory after initial load
	 * 		one holding actual mxArray objects
	 *
	 * 		access hashtable by hashing name
	 */
	
	mxFree(param_struct.operation_name);
}


void readInput(int nrhs, const mxArray* prhs[])
{
	
	param_struct.operation_name = mxArrayToString(prhs[0]);
	param_struct.args = NULL;
	
	if(strcmp(param_struct.operation_name, "share") == 0 || strcmp(param_struct.operation_name, "get") == 0)
	{
		param_struct.args = &prhs[1];
	}
	
	
}

