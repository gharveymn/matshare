#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include <ctype.h>
#include "mshtypes.h"
#include "matshare_.h"
#include "matlabutils.h"

#define MSH_PARAM_MEMSAFE_U "MemorySafety"
#define MSH_PARAM_MEMSAFE_L "memorysafety"

#define MSH_PARAM_SECURITY_U "Security"
#define MSH_PARAM_SECURITY_L "security"

/* Descend through header and data structure and free the memory.            */
void freeTmp(data_t* dat);

/*field names of a structure */
size_t fieldNamesSize(const mxArray* mxStruct);

/*returns the number of bytes used in pList									*/
size_t copyFieldNames(const mxArray* mxStruct, char_t* pList, const char_t** field_names);

/*returns 0 if successful													 */
int pointCharArrayAtString(char_t** pCharArray, char_t* pString, int nFields);

void onExit(void);

/* Pads the size to something that guarantees pointer alignment.			*/
size_t memCpyMex(byte_t* dest, byte_t* orig, size_t* data_off, size_t dest_off, size_t cpy_sz);
size_t padToAlign(size_t size);
void makeMxMallocSignature(uint8_t* sig, size_t seg_size);
void acquireProcLock(void);
void releaseProcLock(void);
msh_directive_t parseDirective(const mxArray* in);
bool_t precheck(void);
void parseParams(int num_params, const mxArray* in[]);
void updateAll(void);
void makeDummyVar(mxArray**);
void nullfcn(void);


#endif //MATSHARE_MATSHAREUTILS_H
