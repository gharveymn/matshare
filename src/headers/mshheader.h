#ifndef MATSHARE_MSHHEADERTYPE_H
#define MATSHARE_MSHHEADERTYPE_H

/* pack the mex class ids to prevent compiler from padding SharedVariableHeader_t */
#include "mex.h"
#include "mshbasictypes.h"

/**
 * Matshare headers are defined as an opaque type.
 */
typedef struct SharedVariableHeader_t SharedVariableHeader_t;

/**
 * Offset Get macros
 */
size_t msh_GetDataOffset(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetImagDataOffset(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetIrOffset(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetFieldNamesOffset(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetJcOffset(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetChildOffsOffset(SharedVariableHeader_t* hdr_ptr);

/**
 * Other attribute Get macros
 */
size_t msh_GetNumDims(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetElemSize(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetNumElems(SharedVariableHeader_t* hdr_ptr);
size_t msh_GetNzmax(SharedVariableHeader_t* hdr_ptr);
int msh_GetNumFields(SharedVariableHeader_t* hdr_ptr);
mxClassID msh_GetClassId(SharedVariableHeader_t* hdr_ptr);
bool_t msh_GetIsEmpty(SharedVariableHeader_t* hdr_ptr);
bool_t msh_GetIsSparse(SharedVariableHeader_t* hdr_ptr);
bool_t msh_GetIsNumeric(SharedVariableHeader_t* hdr_ptr);


/**
 * Offset Set macros
 */
void msh_SetDataOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);
void msh_SetImagDataOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);
void msh_SetIrOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);
void msh_SetFieldNamesOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);
void msh_SetJcOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);
void msh_SetChildOffsOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);

/**
 * Other attribute Set macros
 */
void msh_SetNumDims(SharedVariableHeader_t* hdr_ptr, size_t in);
void msh_SetElemSize(SharedVariableHeader_t* hdr_ptr, size_t in);
void msh_SetNumElems(SharedVariableHeader_t* hdr_ptr, size_t in);
void msh_SetNzmax(SharedVariableHeader_t* hdr_ptr, size_t in);
void msh_SetNumFields(SharedVariableHeader_t* hdr_ptr, int in);
void msh_SetClassId(SharedVariableHeader_t* hdr_ptr, mxClassID in);
void msh_SetIsEmpty(SharedVariableHeader_t* hdr_ptr, bool_t in);
void msh_SetIsSparse(SharedVariableHeader_t* hdr_ptr, bool_t in);
void msh_SetIsNumeric(SharedVariableHeader_t* hdr_ptr, bool_t in);


/**
 * Pointers to data segments at offsets specified by the header.
 */
mwSize* msh_GetDimensions(SharedVariableHeader_t* hdr_ptr);
void* msh_GetData(SharedVariableHeader_t* hdr_ptr);
void* msh_GetImagData(SharedVariableHeader_t* hdr_ptr);
mwIndex* msh_GetIr(SharedVariableHeader_t* hdr_ptr);
char_t* msh_GetFieldNames(SharedVariableHeader_t* hdr_ptr);
mwIndex* msh_GetJc(SharedVariableHeader_t* hdr_ptr);
size_t* msh_GetChildOffsets(SharedVariableHeader_t* hdr_ptr);

/**
 * Retrieves pointer using the child offsets data
 */
SharedVariableHeader_t* msh_GetChildHeader(SharedVariableHeader_t* hdr_ptr, size_t child_num);

/**
 * Miscellaneous macros using inferences
 */
bool_t msh_GetIsComplex(SharedVariableHeader_t* hdr_ptr);
mxComplexity msh_GetComplexity(SharedVariableHeader_t* hdr_ptr);

size_t msh_FindSharedSize(const mxArray* in_var);
size_t msh_CopyVariable(void* dest, const mxArray* in_var);
mxArray* msh_FetchVariable(SharedVariableHeader_t* shm_hdr);
void msh_OverwriteData(SharedVariableHeader_t* shm_hdr, const mxArray* in_var, mxArray* rewrite_var);
bool_t msh_CompareVariableSize(SharedVariableHeader_t* shm_hdr, const mxArray* comp_var);
void msh_DetachVariable(mxArray* ret_var);



#endif /* MATSHARE_MSHHEADERTYPE_H */
