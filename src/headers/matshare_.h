#ifndef matshare__h
#define matshare__h

#include "mshtypes.h"
#include "matlabutils.h"
#include "mshutils.h"
#include "mshlists.h"
#include "mshinit.h"

/* finds the shift needed to add a mxMalloc signature and have data 32 byte aligned */
#define GetDataShift(obj_sz) PadToAlign((obj_sz) + MXMALLOC_SIG_LEN) - (obj_sz)

/* adds the total size of the aligned data */
#define AddDataSize(obj_sz, data_sz) (obj_sz) += GetDataShift(obj_sz) + (data_sz);

/* creates a struct for holding the pointers to shared data */
#define LocateDataPointers(hdr, shm_anchor) \
     (SharedDataPointers_t){\
		MshGetDimensions(shm_anchor),       /* dims */\
          MshGetData(shm_anchor),               /* data */\
          MshGetImagData(shm_anchor),     /* imag_data */\
          MshGetIr(shm_anchor),          /* ir/field_str */\
          MshGetJc(shm_anchor),          /* jc/child_hdrs */\
     };

typedef struct
{
	size_t curr_off;
	byte_t* ptr;
} SharedMemoryTracker_t;
#define MemoryShift(shm_tracker, shift) (shm_tracker).curr_off += (shift), (shm_tracker).ptr += (shift)

#define GetChildHeader(shm_anchor, child_hdr_offs, i) ((shm_anchor) + (child_hdr_offs)[(i)])

void MshFetch(int nlhs, mxArray** plhs);

void MshShare(int nlhs, mxArray** plhs, int num_vars, const mxArray* in_vars[]);

void MshDeepcopy(int nlhs, mxArray** plhs);

void MshClear(int num_inputs, const mxArray* in_vars[]);

/* Remove shared memory references to input matrix (in-situ), recursively    */
/* if needed.                                                                */
void ShmDetach(mxArray* ret_var);

void ShmFetch_(byte_t* shm_anchor, mxArray** ret_var);

bool_t ShmCompareSize_(byte_t* shm_anchor, const mxArray* comp_var);

void ShmRewrite_(byte_t* shm_anchor, const mxArray* in_var, mxArray* rewrite_var);

/* Recursively descend through Matlab matrix to assess how much space its    */
/* serialization will require.                                               */
size_t ShmScan_(const mxArray* in_var);

/* Descend through header and data structure and copy relevent data to       */
/* shared memory.                                                            */
size_t ShmCopy_(byte_t* shm_anchor, const mxArray* in_var);

mxLogical ShmCompareContent_(byte_t* shm, const mxArray* comp_var);

#endif
