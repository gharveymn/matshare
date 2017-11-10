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

#include "ezq.h"


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
#define MATLAB_HELP_MESSAGE ""
#define MATLAB_WARN_MESSAGE ""


typedef enum
{
	MSH_SHARE,
	MSH_GET,
	MSH_UNSHARE
} matop_t;

typedef struct
{
	matop_t matshare_operation;
	char* varname;
} ParamStruct;

typedef char byte_t;

void readInput(int nrhs, const mxArray* prhs[]);
void shareVariable(mxArray* variable, char* varname);
mxArray* getVariable(char* varname);
void unshareVariable(char* varname);
void* moveSegment(mxArray* arr_ptr, byte_t* shm_seg);
size_t getVariableSize(mxArray* variable);
size_t getVariableSize_(mxArray* variable, size_t curr_sz);
void* padTo32ByteAlign(byte_t*);


#endif //MATSHARE_MATSHARE_H
