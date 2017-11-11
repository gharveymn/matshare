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
			shareVariable(prhs[1], param_struct.varname);
			break;
		case MSH_GET:
			plhs[0] = getVariable(param_struct.varname);
			break;
		case MSH_UNSHARE:
			unshareVariable(param_struct.varname);
			break;
		default:
			break;
	}
	
	
	
}


void readInput(int nrhs, const mxArray* prhs[])
{
	
	param_struct.matshare_operation = (matop_t)mxGetData(prhs[0]);
	param_struct.varname = NULL;
	
	switch(param_struct.matshare_operation)
	{
		case MSH_SHARE:
			break;
		case MSH_GET:
		case MSH_UNSHARE:
			param_struct.varname = malloc((1 + MATSHARE_SHM_SIG_LEN + mxGetNumberOfElements(prhs[1]) + 1)*sizeof(char));
			param_struct.varname[0] = '/';
			strcpy(param_struct.varname + 1, MATSHARE_SHM_SIG);
			strcpy(param_struct.varname + 1 + MATSHARE_SHM_SIG_LEN, (char*)mxGetData(prhs[1]));
			break;
		default:
			break;
	}
	
}


void shareVariable(mxArray* variable, char* varname)
{
	//TODO memory alignment
	size_t variable_sz = getVariableSize(variable);
	int fd = shm_open(param_struct.varname, O_RDWR|O_CREAT|O_TRUNC, 0666);
	
	struct stat shm_stats;
	fstat(fd, &shm_stats);
	
	if (shm_stats.st_size > 0)
	{
		readMXWarn("matshare:badInputError", "The variable %s already exists in shared memory.\n", varname);
		return;
	}
	ftruncate(fd, variable_sz);
	byte_t* shm_seg = mmap(NULL , variable_sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	mxArray* variable_clone = mxDuplicateArray(variable);
	moveSegment(variable_clone, shm_seg);
}


mxArray* getVariable(char* varname)
{
	int fd = shm_open(varname, O_RDONLY, 0666);
    
    //get shm segment size
    struct stat shm_stats;
	fstat(fd, &shm_stats);
	size_t variable_sz = shm_stats.st_size;
    
	byte_t* shm_seg = mmap(NULL, variable_sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    shm_seg = padTo32ByteAlign(shm_seg);
	return (mxArray*)shm_seg;
}


void unshareVariable(char* varname)
{
	int fd = shm_open(varname, O_RDWR, 0666);
    
    //get shm segment size
    struct stat shm_stats;
	fstat(fd, &shm_stats);
	size_t variable_sz = shm_stats.st_size;
    
	byte_t* shm_seg = mmap(NULL , variable_sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	mxDestroyArray((mxArray*)shm_seg);
	munmap(shm_seg, variable_sz);
	close(fd);
}


void* moveSegment(mxArray* arr_ptr, byte_t* shm_seg)
{
	shm_seg = padTo32ByteAlign(shm_seg);
	memmove(shm_seg, arr_ptr, sizeof(mxArray));
	shm_seg += sizeof(*arr_ptr);
	
	if(mxIsStruct(arr_ptr) == TRUE)
	{
		size_t num_structs = mxGetNumberOfElements(arr_ptr);
		int num_fields = mxGetNumberOfFields(arr_ptr);
		for(int i = 0; i < num_structs; i++)
		{
			for(int j = 0; j < num_fields; j++)
			{
				shm_seg = moveSegment(mxGetFieldByNumber(arr_ptr, i, j), shm_seg);
				mxSetFieldByNumber(arr_ptr, i, j, (mxArray*)shm_seg);
			}
		}
	}
	else if(mxIsCell(arr_ptr) == TRUE)
	{
		size_t num_cells = mxGetNumberOfElements(arr_ptr);
		for(int i = 0; i < num_cells; i++)
		{
			shm_seg = moveSegment(mxGetCell(arr_ptr, i), shm_seg);
			mxSetCell(arr_ptr, i, (mxArray*)shm_seg);
		}
	}
	else
	{
		shm_seg = padTo32ByteAlign(shm_seg);
		memmove(shm_seg, mxGetData(arr_ptr), mxGetElementSize(arr_ptr) + mxGetNumberOfElements(arr_ptr));
		shm_seg += mxGetElementSize(arr_ptr) + mxGetNumberOfElements(arr_ptr);
		if(mxIsComplex(arr_ptr))
		{
			shm_seg = padTo32ByteAlign(shm_seg);
			memmove(shm_seg, mxGetImagData(arr_ptr), mxGetElementSize(arr_ptr) + mxGetNumberOfElements(arr_ptr));
			shm_seg += mxGetElementSize(arr_ptr) + mxGetNumberOfElements(arr_ptr);
		}
	}
	
	return shm_seg;
	
}


size_t getVariableSize(mxArray* variable)
{
	return getVariableSize_(variable, 0);
}


size_t getVariableSize_(mxArray* variable, size_t curr_sz)
{
	
	//31 possible extra padded bytes to satisfy AVX 32-byte alignment
	curr_sz += sizeof(mxArray) + 0x20;
	
	if(mxIsStruct(variable) == TRUE)
	{
		size_t num_structs = mxGetNumberOfElements(variable);
		int num_fields = mxGetNumberOfFields(variable);
		for(int i = 0; i < num_structs; i++)
		{
			for(int j = 0; j < num_fields; j++)
			{
				curr_sz += getVariableSize_(mxGetFieldByNumber(variable,i,j), curr_sz);
			}
		}
		return curr_sz;
	}
	else if(mxIsCell(variable) == TRUE)
	{
		size_t num_cells = mxGetNumberOfElements(variable);
		for(int i = 0; i < num_cells; i++)
		{
			mxArray* cell = mxGetCell(variable, i);
			curr_sz += getVariableSize_(cell, curr_sz);
		}
		return curr_sz;
	}
	else
	{
		return curr_sz + (mxIsComplex(variable) + 1)*(mxGetElementSize(variable) + mxGetNumberOfElements(variable) + 0x20);
	}
}


void* padTo32ByteAlign(byte_t* ptr)
{
	size_t addr = (size_t)ptr;
	return ptr + (0x20 - (addr & 0x1F));
}

