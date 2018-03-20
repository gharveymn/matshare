#include "mex.h"
#include "windows.h"
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	HANDLE temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1, "fwdchk");
	DWORD err = GetLastError();
	if(temp_handle == NULL)
	{
		//readErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
	}
	else if(err != ERROR_ALREADY_EXISTS)
	{
		// nothing
	}
	else
	{
		if(CloseHandle(temp_handle) == 0)
		{
			//readErrorMex("CloseHandleError", "Error closing the init file handle (Error Number %u)", GetLastError());
		}
	}
}