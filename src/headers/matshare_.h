#ifndef matshare__h
#define matshare__h

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"
#include "init.h"

void mshFetch(int nlhs, mxArray* plhs[]);

void mshShare(int nlhs, mxArray* plhs[], const mxArray* in_var);

size_t shmFetch(byte_t* shm, mxArray** ret_var);

bool_t shmCompareSize(byte_t* shm, const mxArray* comp_var);

size_t shmRewrite(byte_t* shm_anchor, const mxArray* in_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t shmScan(const mxArray* in_var, mxArray** ret_var);

/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
size_t shmCopy(byte_t* shm_anchor, const mxArray* in_var, mxArray* ret_var);

mxLogical shmCompareContent(byte_t* shm, const mxArray* comp_var);

/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
void shmDetach(mxArray* ret_var);

size_t shmFetch_(byte_t* shm_anchor, mxArray** ret_var);

bool_t shmCompareSize_(byte_t* shm_anchor, const mxArray* comp_var);

size_t shmRewrite_(byte_t* shm_anchor, const mxArray* in_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t shmScan_(const mxArray* in_var, mxArray** ret_var);

/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
size_t shmCopy_(byte_t* shm_anchor, const mxArray* in_var, mxArray* ret_var);

mxLogical shmCompareContent_(byte_t* shm, const mxArray* comp_var);


#endif
