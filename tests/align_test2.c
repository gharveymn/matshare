
#include <mex.h>
#include <matrix.h>
void* padTo32ByteAlign(char* ptr);

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	int len;
	void *data;
	void* newdata;
	size_t data_addr;
	size_t padded_data_addr;
	size_t alignment;
	
	alignment = 0;
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
		data = malloc (len + 0x20);
		newdata = padTo32ByteAlign(data);
		data_addr = (size_t) data;
		padded_data_addr = (size_t) newdata;
		//mexPrintf ("data_addr=%lx\n", data_addr);
		//mexPrintf ("padded_data_addr=%lx\n", padded_data_addr);
		alignment |= padded_data_addr;
		free(data);
	}
	mexPrintf ("malloc alignment with padding=%lx\n", alignment & 0x1F);
	
}

void* padTo32ByteAlign(char* ptr)
{
	size_t addr = (size_t)ptr;
	return ptr + (0x20 - (addr & 0x1F));
}