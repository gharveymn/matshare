#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include <ctype.h>
#include <sys/stat.h>
#include "mshtypes.h"
#include "matshare_.h"
#include "matlabutils.h"
#include "mshlists.h"


#define MSH_PARAM_THRSAFE_U "ThreadSafety"
#define MSH_PARAM_THRSAFE_L "threadsafety"

#define MSH_PARAM_SECURITY_U "Security"
#define MSH_PARAM_SECURITY_L "security"

#define MSH_PARAM_COPYONWRITE_U "CopyOnWrite"
#define MSH_PARAM_COPYONWRITE_L "copyonwrite"


/*field names of a structure */
size_t GetFieldNamesSize(const mxArray* mxStruct);

void MshExit(void);

void GetNextFieldName(const char_t** field_str);

void* MemCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz);

/* note: must be > 0 */
#define PadToAlign(size) ((size) + (ALIGN_SHIFT - (((size) - 1) & ALIGN_SHIFT)))

void MakeMxMallocSignature(unsigned char* sig, size_t seg_size);

void AcquireProcessLock(void);

void ReleaseProcessLock(void);

mshdirective_t ParseDirective(const mxArray* in);

bool_t Precheck(void);

void ParseParams(int num_params, const mxArray** in);

void UpdateAll(void);

void RemoveUnusedVariables(VariableList_t* var_list);

void SetDataPointers(mxArray* var, SharedDataPointers_t* data_ptrs);

void NullFunction(void);

#endif //MATSHARE_MATSHAREUTILS_H
