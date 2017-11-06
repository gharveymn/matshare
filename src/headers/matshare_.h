#ifndef MATSHARE_MATSHARE_H
#define MATSHARE_MATSHARE_H

#include <stdio.h>
#include <string.h>
#include <mex.h>
#include <matrix.h>
#include "ezq.h"

#define TRUE 1
#define FALSE 0
#define MATSHARE_SHM_SIG "TUFUU0hBUkU"
#define MATSHARE_SHM_SIG_LEN 11


typedef enum
{
	MSH_OPEN,
	MSH_SHARE,
	MSH_GET,
	MSH_UNSHARE,
	MSH_CLOSE
} matop_t;

typedef struct
{
	matop_t matshare_operation;
	char* varname;
} ParamStruct;

void readInput(int nrhs, const mxArray* prhs[]);
size_t getVariableSize(mxArray* variable);
void shareVariable(mxArray* variable);
mxArray* getVariable(char* varname);
void unshareVariable(char* varname);



#endif //MATSHARE_MATSHARE_H
