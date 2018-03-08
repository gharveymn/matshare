#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include <ctype.h>
#include "mshtypes.h"
#include "matshare_.h"
#include "matlabutils.h"

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
size_t memCpyMex(byte_t* dest, byte_t* orig, byte_t** data_ptr, size_t cpy_sz);
size_t padToAlign(size_t size);
void makeMxMallocSignature(uint8_t* sig, size_t seg_size);
void acquireProcLock(void);
void releaseProcLock(void);
msh_directive_t parseDirective(const mxArray* in);
bool_t precheck(void);
void updateAll(void);
void makeDummyVar(mxArray**);
void nullfcn(void);


#endif //MATSHARE_MATSHAREUTILS_H
