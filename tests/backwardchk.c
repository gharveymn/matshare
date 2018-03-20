#include "mex.h"
#include "windows.h"
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	HANDLE temp_handle = OpenFileMapping(0, FALSE, "bkwdchk");
	if(temp_handle == NULL)
	{
		temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1, "bkwdchk");
	}
	else
	{
		if(CloseHandle(temp_handle) == 0)
		{
			//readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
		}
	}
}