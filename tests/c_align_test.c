#include <mex.h>
#include <matrix.h>
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	int len;
	void *data;
	size_t data_addr;
	size_t alignment;
	alignment = 0;
	for (len = 1; len < 0x10001; len++)
	{
		data = mxMalloc (len);
		data_addr = (size_t) data;
		alignment |= data_addr;
		mxFree(data);
	}
	mexPrintf ("mxMalloc alignment=%lx\n", alignment & 0x1F);
	alignment = 0;
	for (len = 1; len < 0x10001; len++)
	{
		data = mxCalloc (len, 1);
		data_addr = (size_t) data;
		alignment |= data_addr;
		mxFree(data);
	}
	mexPrintf ("mxCalloc alignment=%lx\n", alignment & 0x1F);
	
	for (len = 1; len < 0x10001; len++)
	{
		data = malloc (len);
		data_addr = (size_t) data;
		alignment |= data_addr;
		free(data);
	}
	mexPrintf ("malloc alignment=%lx\n", alignment & 0x1F);
	alignment = 0;
	for (len = 1; len < 0x10001; len++)
	{
		data = calloc (len, 1);
		data_addr = (size_t) data;
		alignment |= data_addr;
		free(data);
	}
	mexPrintf ("calloc alignment=%lx\n", alignment & 0x1F);
	
}