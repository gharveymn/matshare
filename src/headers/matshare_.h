#ifndef matshare__h
#define matshare__h

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"
#include "init.h"

void MshFetch(int nlhs, mxArray** plhs);

void MshShare(int nlhs, mxArray** plhs, const mxArray* in_var);

void MshUpdateSegments(void);

void ShmFetch(byte_t* shm, mxArray** ret_var);

bool_t ShmCompareSize(byte_t* shm, const mxArray* comp_var);

size_t ShmRewrite(byte_t* shm_anchor, const mxArray* in_var, mxArray* rewrite_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t ShmScan(const mxArray* in_var);

/* Descend through header and data structure and copy relevant data to       */
/* shared memory.                                                            */
void ShmCopy(SegmentNode_t* seg_node, const mxArray* in_var);

/* mxLogical ShmCompareContent(byte_t* shm, const mxArray* comp_var); */

/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
void ShmDetach(mxArray* ret_var);

size_t ShmFetch_(byte_t* shm_anchor, mxArray** ret_var);

bool_t ShmCompareSize_(byte_t* shm_anchor, const mxArray* comp_var);

size_t ShmRewrite_(byte_t* shm_anchor, const mxArray* in_var, mxArray* rewrite_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t ShmScan_(const mxArray* in_var);

/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
size_t shmCopy_(byte_t* shm_anchor, const mxArray* in_var);

mxLogical ShmCompareContent_(byte_t* shm, const mxArray* comp_var);

#endif
