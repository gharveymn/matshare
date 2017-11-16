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
	
	param_struct.matshare_operation = (matop_t) mxGetData(prhs[0]);
	param_struct.varname = NULL;
	
	switch(param_struct.matshare_operation)
	{
		case MSH_SHARE:
			
			switch(mxGetClassID(prhs[1]))
			{
				case mxINT8_CLASS:
				case mxUINT8_CLASS:
				case mxINT16_CLASS:
				case mxUINT16_CLASS:
				case mxINT32_CLASS:
				case mxUINT32_CLASS:
				case mxINT64_CLASS:
				case mxUINT64_CLASS:
				case mxSINGLE_CLASS:
				case mxDOUBLE_CLASS:
				case mxLOGICAL_CLASS:
				case mxCHAR_CLASS:
				case mxSTRUCT_CLASS:
				case mxCELL_CLASS:
					//ok
					break;
				case mxSPARSE_CLASS:
					//TODO
				default:
					readMXError("matshare:unknownOutputType",
							  "The output datatype is either corrupted or not available for use with matshare.");
			}
					
			break;
		case MSH_GET:
		case MSH_UNSHARE:
			param_struct.varname = malloc(
					(1 + MATSHARE_SHM_SIG_LEN + mxGetNumberOfElements(prhs[1]) + 1) * sizeof(char));
			param_struct.varname[0] = '/';
			strcpy(param_struct.varname + 1, MATSHARE_SHM_SIG);
			strcpy(param_struct.varname + 1 + MATSHARE_SHM_SIG_LEN, (char*) mxGetData(prhs[1]));
			break;
		default:
			break;
	}
	
}


void shareVariable(mxArray* variable, char* varname)
{
	//TODO memory alignment
	size_t variable_sz = getVariableSize(variable);
	int fd = shm_open(param_struct.varname, O_RDWR | O_CREAT | O_TRUNC, 0666);
	
	struct stat shm_stats;
	fstat(fd, &shm_stats);
	
	if(shm_stats.st_size > 0)
	{
		readMXWarn("matshare:badInputError", "The variable %s already exists in shared memory.\n", varname);
		return;
	}
	ftruncate(fd, variable_sz);
	byte_t* shm_seg = mmap(NULL, variable_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	mxArray* variable_clone = mxDuplicateArray(variable);
	storeSegment(variable_clone, shm_seg);
}


mxArray* getVariable(char* varname)
{
	int fd = shm_open(varname, O_RDONLY, 0666);
	
	//get shm segment size
	struct stat shm_stats;
	fstat(fd, &shm_stats);
	size_t variable_sz = shm_stats.st_size;
	
	byte_t* shm_seg = mmap(NULL, variable_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	return (mxArray*) shm_seg;
}


void unshareVariable(char* varname)
{
	int fd = shm_open(varname, O_RDWR, 0666);
	
	//get shm segment size
	struct stat shm_stats;
	fstat(fd, &shm_stats);
	size_t variable_sz = shm_stats.st_size;
	
	byte_t* shm_seg = mmap(NULL, variable_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	mxDestroyArray((mxArray*) shm_seg);
	munmap(shm_seg, variable_sz);
	close(fd);
}


void* storeSegment(mxArray* arr_ptr, mshHeader_t* array_header)
{
	array_header->datatype = mxGetClassID(arr_ptr);
	array_header->elemsz = mxGetElementSize(arr_ptr);
	array_header->numelems = mxGetNumberOfElements(arr_ptr);
	array_header->numfields = mxGetNumberOfFields(arr_ptr);
	array_header->is_complex = (mxComplexity)mxIsComplex(arr_ptr);
	array_header->numdims = mxGetNumberOfDimensions(arr_ptr);
	array_header->right_adj = NULL;
	array_header->first_child = NULL;
	const mwSize* arr_dims = mxGetDimensions(arr_ptr);
	memcpy(array_header->dims, arr_dims, array_header->numdims*sizeof(mwSize));
	
	mshHeader_t* subheader = array_header + sizeof(mshHeader_t);
	mshHeader_t* left_adj = NULL;
	
	switch(array_header->datatype)
	{
		case mxINT8_CLASS:
		case mxUINT8_CLASS:
		case mxINT16_CLASS:
		case mxUINT16_CLASS:
		case mxINT32_CLASS:
		case mxUINT32_CLASS:
		case mxINT64_CLASS:
		case mxUINT64_CLASS:
		case mxSINGLE_CLASS:
		case mxDOUBLE_CLASS:
		case mxLOGICAL_CLASS:
		case mxCHAR_CLASS:
			
			array_header->real_data = (byte_t*)subheader;
			array_header->imag_data = NULL;
			
			array_header->real_data = padTo32ByteAlign(array_header->real_data);
			memmove(array_header->real_data, mxGetData(arr_ptr), mxGetElementSize(arr_ptr)*mxGetNumberOfElements(arr_ptr));
			
			if(array_header->is_complex == TRUE)
			{
				array_header->imag_data = array_header->real_data + mxGetElementSize(arr_ptr)*mxGetNumberOfElements(arr_ptr);
				array_header->imag_data = padTo32ByteAlign(array_header->imag_data);
				memmove(array_header->imag_data, mxGetImagData(arr_ptr), mxGetElementSize(arr_ptr)*mxGetNumberOfElements(arr_ptr));
			}
			
			if(array_header->imag_data == NULL)
			{
				array_header = (mshHeader_t*)(array_header->real_data +
										mxGetElementSize(arr_ptr) * mxGetNumberOfElements(arr_ptr));
			}
			else
			{
				array_header = (mshHeader_t*)(array_header->imag_data +
										mxGetElementSize(arr_ptr) * mxGetNumberOfElements(arr_ptr));
			}
		
		case mxSTRUCT_CLASS:
			
			for(int i = 0; i < array_header->numelems; i++)
			{
				for(int j = 0; j < array_header->numfields; j++)
				{
					subheader = storeSegment(mxGetFieldByNumber(arr_ptr, i, j), subheader);
					strcpy(subheader->fieldname, mxGetFieldNameByNumber(arr_ptr, j));
					
					if(i == 0 && j == 0)
					{
						array_header->first_child = subheader;
					}
					
					if(left_adj != NULL)
					{
						left_adj->right_adj = subheader;
					}
					left_adj = subheader;
					
				}
			}
			
			array_header = subheader;
			break;
			
		case mxCELL_CLASS:
			
			for(int i = 0; i < array_header->numelems; i++)
			{
				subheader = storeSegment(mxGetCell(arr_ptr, i), subheader);
				if(i == 0)
				{
					array_header->first_child = subheader;
				}
				
				if(left_adj != NULL)
				{
					left_adj->right_adj = subheader;
				}
				left_adj = subheader;
			}
			
			array_header = subheader;
		
		case mxSPARSE_CLASS:
			//TODO
		default:
			readMXError("matshare:unknownOutputType", "The output datatype is either corrupted or not available for use with matshare.");
		
	}
	
	return array_header;
	
}


size_t getVariableSize(mxArray* variable)
{
	return getVariableSize_(variable, 0);
}


size_t getVariableSize_(mxArray* variable, size_t curr_sz)
{
	
	curr_sz += sizeof(mshHeader_t);
	
	if(mxIsStruct(variable) == TRUE)
	{
		size_t num_structs = mxGetNumberOfElements(variable);
		int num_fields = mxGetNumberOfFields(variable);
		for(int i = 0; i < num_structs; i++)
		{
			for(int j = 0; j < num_fields; j++)
			{
				curr_sz = getVariableSize_(mxGetFieldByNumber(variable, i, j), curr_sz);
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
			curr_sz = getVariableSize_(cell, curr_sz);
		}
		return curr_sz;
	}
	else
	{
		return curr_sz +
			  (mxIsComplex(variable) + 1) * (mxGetElementSize(variable) + mxGetNumberOfElements(variable) + 0x20);
	}
}


mxArray* createVariable(mshHeader_t* variable_header)
{
	mxArray* ret = NULL;
	mshHeader_t* subheader = NULL;
	char** fieldnames;
	switch(variable_header->datatype)
	{
		case mxINT8_CLASS:
		case mxUINT8_CLASS:
		case mxINT16_CLASS:
		case mxUINT16_CLASS:
		case mxINT32_CLASS:
		case mxUINT32_CLASS:
		case mxINT64_CLASS:
		case mxUINT64_CLASS:
		case mxSINGLE_CLASS:
		case mxDOUBLE_CLASS:
			
			ret = mxCreateNumericArray(0, NULL, variable_header->datatype, variable_header->is_complex);
			mxSetData(ret, (void*)variable_header->real_data);
			if(variable_header->is_complex == TRUE)
			{
				mxSetImagData(ret, (void*)variable_header->imag_data);
			}
			mxSetDimensions(ret, variable_header->dims, variable_header->numdims);
			break;
			
		case mxLOGICAL_CLASS:
			
			ret = mxCreateLogicalArray(0, NULL);
			mxSetData(ret, (void*)variable_header->real_data);
			mxSetDimensions(ret, variable_header->dims, variable_header->numdims);
			break;
			
		case mxCHAR_CLASS:
			
			ret = mxCreateCharArray(0, NULL);
			mxSetData(ret, (void*)variable_header->real_data);
			mxSetDimensions(ret, variable_header->dims, variable_header->numdims);
			break;
			
		case mxSTRUCT_CLASS:
			subheader = variable_header->first_child;
			fieldnames = malloc(variable_header->numfields*sizeof(char*));
			//get field names
			for(int i = 0; i < variable_header->numfields; i++, subheader = subheader->right_adj)
			{
				fieldnames[i] = subheader->fieldname;
			}
			ret = mxCreateStructArray(variable_header->numdims,variable_header->dims, variable_header->numfields, fieldnames);
			
			subheader = variable_header->first_child;
			for(int i = 0; i < variable_header->numelems; i++)
			{
				for(int j = 0; j < variable_header->numfields; j++, subheader = subheader->right_adj)
				{
					mxSetFieldByNumber(ret, i, j, createVariable(subheader));
					fieldnames[j] = subheader->fieldname;
				}
			}
			free(fieldnames);
			break;
		case mxCELL_CLASS:
			ret = mxCreateCellArray(variable_header->numdims,variable_header->dims);
			subheader = variable_header->first_child;
			for(int i = 0; i < variable_header->numelems; i++, subheader = subheader->right_adj)
			{
				mxSetCell(ret, i, createVariable(subheader));
			}
			break;
		case mxSPARSE_CLASS:
			//TODO
		default:
			readMXError("matshare:unknownOutputType", "The output datatype is either corrupted or not available for use with matshare.");
		
	}
	
	return ret;
}


void* padTo32ByteAlign(byte_t* ptr)
{
	size_t addr = (size_t) ptr;
	return ptr + (0x20 - (addr & 0x1F));
}

void exitHooks(void)
{
	free(param_struct.varname);
}

