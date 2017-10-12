#ifndef MATSHARE_MATSHARE_H
#define MATSHARE_MATSHARE_H

#include <stdio.h>
#include <string.h>
#include <mex.h>
#include <matrix.h>

#define TRUE 1
#define FALSE 0

typedef struct
{
	char* operation_name;
	const mxArray** args;
} ParamStruct;

void readInput(int nrhs, const mxArray* prhs[]);


#endif //MATSHARE_MATSHARE_H
