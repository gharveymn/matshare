#ifndef MATSHARE_MATSHARE_H
#define MATSHARE_MATSHARE_H

#include <stdio.h>
#include <string.h>
#include <mex.h>
#include <matrix.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#if UINTPTR_MAX == 0xffffffffffffffff
#define GMV_64_BIT
typedef int64_t address_t;
typedef int64_t index_t;
#define UNDEF_ADDR (address_t)0xffffffffffffffff
#elif UINTPTR_MAX == 0xffffffff
#define GMV_32_BIT
typedef uint32_t address_t;
typedef uint32_t index_t;
#define UNDEF_ADDR (address_t)0xffffffff
#else
#error Your architecture is weird
#endif

#include "utils.h"


#if (defined(_WIN32) || defined(WIN32) || defined(_WIN64)) && !defined __CYGWIN__
#include "../extlib/mman-win32/mman.h"


#else
#include <sys/mman.h>
#endif

#define TRUE 1
#define FALSE 0
#define MATSHARE_SHM_SIG "TUFUU0hBUkU"
#define MATSHARE_SHM_SIG_LEN 11
#define ERROR_BUFFER_SIZE 5000
#define WARNING_BUFFER_SIZE 1000
#define MATLAB_MAX_NAME_LENGTH 63
#define MATLAB_MAX_NUM_DIMS 32 //mirrors MAT-files
#define MATLAB_HELP_MESSAGE ""
#define MATLAB_WARN_MESSAGE ""

typedef char byte_t;
typedef bool bool_t;

typedef enum
{
	MSH_SHARE,
	MSH_GET,
	MSH_UNSHARE
} matop_t;


typedef struct mshHeader_t_ mshHeader_t;
struct mshHeader_t_
{
	mxClassID datatype;
	char fieldname[MATLAB_MAX_NAME_LENGTH + 1];
	mwSize dims[MATLAB_MAX_NUM_DIMS];
	mwSize numdims;
	size_t elemsz;
	size_t numelems;
	int numfields;
	mxComplexity is_complex;
	byte_t* real_data;
	byte_t* imag_data;
	mshHeader_t* first_child;
	mshHeader_t* right_adj;
};

typedef struct
{
	matop_t matshare_operation;
	char* varname;
} ParamStruct;

void readInput(int nrhs, const mxArray* prhs[]);
void shareVariable(mxArray* variable, char* varname);
mxArray* getVariable(char* varname);
void unshareVariable(char* varname);
void* storeSegment(mxArray* arr_ptr, mshHeader_t* array_header);
mxArray* createVariable(mshHeader_t* variable_header);
size_t getVariableSize(mxArray* variable);
size_t getVariableSize_(mxArray* variable, size_t curr_sz);
void* padTo32ByteAlign(byte_t*);
void exitHooks(void);




#endif //MATSHARE_MATSHARE_H
