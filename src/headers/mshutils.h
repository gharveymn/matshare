#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include <ctype.h>
#include <sys/stat.h>
#include "mshtypes.h"
#include "matshare_.h"
#include "matlabutils.h"
#include "mshlists.h"


#define MSH_PARAM_THREADSAFETY "ThreadSafety"
#define MSH_PARAM_THREADSAFETY_L "threadsafety"

#define MSH_PARAM_SECURITY_U "Security"
#define MSH_PARAM_SECURITY_L "security"

#define MSH_PARAM_COPYONWRITE_U "CopyOnWrite"
#define MSH_PARAM_COPYONWRITE_L "copyonwrite"

#define MSH_PARAM_REMOVEUNUSED_U "RemoveUnused"
#define MSH_PARAM_REMOVEUNUSED_L "removeunused"

#ifdef MSH_WIN
#define MSH_PARAM_INFO \
"Current parameters for matshare: \n\
                                      ThreadSafety: '%s'\n\
                                      CopyOnWrite:  '%s'\n"
#else
#define MSH_PARAM_INFO \
"Current parameters for matshare: \n\
                                      ThreadSafety: '%s'\n\
                                      CopyOnWrite:  '%s'\n\
                                      Security:     '%o'\n"
#endif

/*field names of a structure */
size_t GetFieldNamesSize(const mxArray* mxStruct);

void MshOnExit(void);

void MshOnError(void);

void GetNextFieldName(const char_t** field_str);

void* MemCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz);

/* note: must be > 0 */
#define PadToAlign(size) ((size) + (ALIGN_SHIFT - (((size) - 1) & ALIGN_SHIFT)))

void MakeMxMallocSignature(unsigned char* sig, size_t seg_size);

void AcquireProcessLock(void);

void ReleaseProcessLock(void);

msh_directive_t ParseDirective(const mxArray* in);

bool_t Precheck(void);

void ParseParams(int num_params, const mxArray** in);

void UpdateAll(void);

void RemoveUnusedVariables(void);

void SetDataPointers(mxArray* var, SharedDataPointers_t* data_ptrs);

/**
 * Writes the segment name to the name buffer.
 *
 * @param name_buffer The destination of the segment name.
 * @param seg_num The segment number used by matshare to identify the segment.
 */
void WriteSegmentName(char name_buffer[static MSH_MAX_NAME_LEN], msh_segmentnumber_t seg_num);

void NullFunction(void);

#endif //MATSHARE_MATSHAREUTILS_H
