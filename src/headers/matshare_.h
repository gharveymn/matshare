#ifndef MATSHARE_MATSHARE_H
#define MATSHARE_MATSHARE_H

#include <stdio.h>
#include <string.h>
#include <mex.h>
#include <matrix.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../extlib/mman-win32/mman.h"
#include "ezq.h"

#define TRUE 1
#define FALSE 0
#define MATSHARE_SHM_SIG "TUFUU0hBUkU"
#define MATSHARE_SHM_SIG_LEN 11


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
mxArray* shareVariable(mxArray* variable);
mxArray* getVariable(char* varname);
void unshareVariable(char* varname);
void* moveSegment(mxArray* arr_ptr, byte_t* shm_seg);
size_t getVariableSize(mxArray* variable);
size_t getVariableSize_(mxArray* variable, size_t curr_sz);



#endif //MATSHARE_MATSHARE_H
