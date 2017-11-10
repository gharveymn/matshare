
#include <mex.h>
#include <matrix.h>
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	int len;
	void *data;
	size_t data_addr;
	size_t alignment;
	
	for (len = 1; len < 0x10001; len++)
	{
		data = malloc (len);
		data_addr = (size_t) data;
		alignment |= data_addr;
		free(data)
	}
	mexPrintf ("malloc alignment=%lx\n", alignment % 0x1F);
	
}

void* padTo32ByteAlign(void* ptr)
{
	size_t addr = (size_t)ptr;
	return ptr + (0x1F - (addr % 0x1F));
}