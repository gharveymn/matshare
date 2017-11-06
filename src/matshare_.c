#include "headers/matshare_.h"

static ParamStruct param_struct;

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
	
	/* SUPPORTED TYPES GOAL
	n	mxUNKNOWN_CLASS,
	y	mxCELL_CLASS,
	y	mxSTRUCT_CLASS,
	y	mxLOGICAL_CLASS,
	y	mxCHAR_CLASS,
	n	mxVOID_CLASS,
	y	mxDOUBLE_CLASS,
	y	mxSINGLE_CLASS,
	y	mxINT8_CLASS,
	y	mxUINT8_CLASS,
	y	mxINT16_CLASS,
	y	mxUINT16_CLASS,
	y	mxINT32_CLASS,
	y	mxUINT32_CLASS,
	y	mxINT64_CLASS,
	y	mxUINT64_CLASS,
	n	mxFUNCTION_CLASS,
	n	mxOPAQUE_CLASS,
	n	mxOBJECT_CLASS,
	*/
	
	readInput(nrhs, prhs);
	
	switch(param_struct.matshare_operation)
	{
		case MSH_SHARE:
			break;
		case MSH_GET:
			break;
		case MSH_UNSHARE:
			break;
		default:
			break;
	}
	
	
	
}


void readInput(int nrhs, const mxArray* prhs[])
{
	
	char* opname = mxArrayToString(prhs[0]);
	param_struct.varname = NULL;
	
	
	if(strcmp(opname, "share") == 0)
	{
		param_struct.matshare_operation = MSH_SHARE;
		param_struct.varname = malloc((MATSHARE_SHM_SIG_LEN + strlen()));
	}
	else if(strcmp(opname, "get") == 0)
	{
		param_struct.matshare_operation = MSH_GET;
		param_struct. = &prhs[1];
	}
	else if(strcmp(opname, "unshare") == 0)
	{
		param_struct.matshare_operation = MSH_UNSHARE;
	}
	else
	{
		mxFree(opname);
		mexErrMsgIdAndTxt("matshare:invalidOperation", "The specified operation is invalid. "
				"Available operations are 'open', 'share', 'get', and 'close'.");
	}
	
	mxFree(opname);
	
}

