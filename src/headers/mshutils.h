#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include <ctype.h>
#include <sys/stat.h>
#include "mshtypes.h"
#include "matshare_.h"
#include "matlabutils.h"

#define MSH_PARAM_THRSAFE_U "ThreadSafety"
#define MSH_PARAM_THRSAFE_L "threadsafety"

#define MSH_PARAM_SECURITY_U "Security"
#define MSH_PARAM_SECURITY_L "security"


/*field names of a structure */
size_t getFieldNamesSize(const mxArray* mxStruct);

void retrieveFieldNames(const char_t** field_names, const char_t* field_str, int num_fields);

void onExit(void);

/* Pads the size to something that guarantees pointer alignment.			*/
void locateDataPointers(ShmData_t* data_ptrs, Header_t* hdr, byte_t* shm_anchor);
void* memCpyMex(byte_t* dest, byte_t* orig, size_t cpy_sz);
size_t padToAlign(size_t size);
void makeMxMallocSignature(uint8_t* sig, size_t seg_size);
void acquireProcLock(void);
void releaseProcLock(void);
mshdirective_t parseDirective(const mxArray* in);
bool_t precheck(void);
void parseParams(int num_params, const mxArray* in[]);
void updateAll(void);
void makeDummyVar(mxArray**);
void nullfcn(void);


#endif //MATSHARE_MATSHAREUTILS_H
