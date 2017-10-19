#include "headers/matshare.h"

static ParamStruct param_struct;
Queue mxArrayNameTracker;

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
	
}


void readInput(int nrhs, const mxArray* prhs[])
{
	
	char* opname = mxArrayToString(prhs[0]);
	param_struct.args = NULL;
	
	
	if(strcmp(opname, "open") == 0)
	{
		param_struct.matshare_operation = MSH_OPEN;
	}
	else if(strcmp(opname, "share") == 0)
	{
		param_struct.matshare_operation = MSH_SHARE;
		param_struct.args = &prhs[1];
	}
	else if(strcmp(opname, "get") == 0)
	{
		param_struct.matshare_operation = MSH_GET;
		param_struct.args = &prhs[1];
	}
	else if(strcmp(opname, "close") == 0)
	{
		param_struct.matshare_operation = MSH_CLOSE;
	}
	else
	{
		mxFree(opname);
		mexErrMsgIdAndTxt("matshare:invalidOperation", "The specified operation is invalid. "
				"Available operations are 'open', 'share', 'get', and 'close'.");
	}
	
	mxFree(opname);
	
}


void

