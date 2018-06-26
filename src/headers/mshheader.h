/** mshheaders.h
 * Provides some function declarations for access to shared
 * variable headers.
 *
 * Copyright (c) 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef MATSHARE_MSHHEADERTYPE_H
#define MATSHARE_MSHHEADERTYPE_H

#include "mshbasictypes.h"

#define MSH_VIRTUAL_SCALAR_MAX_DIM 2

/* forward declaration to avoid include */
typedef struct mxArray_tag mxArray;

/** Forward declaration for SharedVariableHeader_t **/
typedef struct SharedVariableHeader_t SharedVariableHeader_t;

/** getters **/

/**
 * Gets the data offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @return The data offset.
 */
size_t msh_GetDataOffset(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the imaginary data offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @return The imaginary data offset.
 */
size_t msh_GetImagDataOffset(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the ir offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @return The ir offset.
 */
size_t msh_GetIrOffset(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the field names offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @return The field names offset.
 */
size_t msh_GetFieldNamesOffset(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the jc offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @return The jc offset.
 */
size_t msh_GetJcOffset(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the offset of the child offsets array from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @return The offset of the child offsets array.
 */
size_t msh_GetChildOffsOffset(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the number of dimensions.
 *
 * @param hdr_ptr The shared variable header.
 * @return The number of dimensions.
 */
size_t msh_GetNumDims(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the element size.
 *
 * @param hdr_ptr The shared variable header.
 * @return The element size.
 */
size_t msh_GetElemSize(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the number of elements.
 *
 * @param hdr_ptr The shared variable header.
 * @return The number of elements
 */
size_t msh_GetNumElems(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets nzmax for this variable.
 *
 * @param hdr_ptr The shared variable header.
 * @return The nzmax for this variable.
 */
size_t msh_GetNzmax(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the number of fields.
 *
 * @param hdr_ptr The shared variable header.
 * @return The number of fields.
 */
int msh_GetNumFields(SharedVariableHeader_t* hdr_ptr);


/**
 * Gets the class ID.
 *
 * @param hdr_ptr The shared variable header.
 * @return The class ID.
 */
int msh_GetClassID(SharedVariableHeader_t* hdr_ptr);


/**
 * Returns the variable empty flag.
 *
 * @param hdr_ptr The shared variable header.
 * @return The empty flag.
 */
int msh_GetIsEmpty(SharedVariableHeader_t* hdr_ptr);


/**
 * Returns the variable sparsity flag.
 *
 * @param hdr_ptr The shared variable header.
 * @return The sparsity flag.
 */
int msh_GetIsSparse(SharedVariableHeader_t* hdr_ptr);


/**
 * Returns the variable numeric flag.
 *
 * @param hdr_ptr The shared variable header.
 * @return The numeric flag.
 */
int msh_GetIsNumeric(SharedVariableHeader_t* hdr_ptr);


/**
 * Returns the virtual scalar flag.
 *
 * @param hdr_ptr The shared variable header.
 * @return The numeric flag.
 */
int msh_GetIsVirtualScalar(SharedVariableHeader_t* hdr_ptr);


/** setters **/

/**
 * Sets the data offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @param new_off The data offset.
 */
void msh_SetDataOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);


/**
 * Sets the imaginary data offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @param new_off The imaginary data offset.
 */
void msh_SetImagDataOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);


/**
 * Sets the ir offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @param new_off The ir offset.
 */
void msh_SetIrOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);


/**
 * Sets the field names offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @param new_off The field names offset.
 */
void msh_SetFieldNamesOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);


/**
 * Sets the jc offset from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @param new_off The jc offset.
 */
void msh_SetJcOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);


/**
 * Gets the offset of the child offsets array from the shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @param new_off The offset of the child offsets array.
 */
void msh_SetChildOffsOffset(SharedVariableHeader_t* hdr_ptr, size_t new_off);


/**
 * Sets the number of dimensions.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The number of dimensions.
 */
void msh_SetNumDims(SharedVariableHeader_t* hdr_ptr, size_t in);


/**
 * Sets the element size.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The element size.
 */
void msh_SetElemSize(SharedVariableHeader_t* hdr_ptr, size_t in);


/**
 * Sets the number of elements.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The number of elements
 */
void msh_SetNumElems(SharedVariableHeader_t* hdr_ptr, size_t in);


/**
 * Sets nzmax for this variable.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The nzmax for this variable.
 */
void msh_SetNzmax(SharedVariableHeader_t* hdr_ptr, size_t in);


/**
 * Sets the number of fields.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The number of fields.
 */
void msh_SetNumFields(SharedVariableHeader_t* hdr_ptr, int in);


/**
 * Sets the class ID.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The class ID.
 */
void msh_SetClassId(SharedVariableHeader_t* hdr_ptr, int in);


/**
 * Sets the variable empty flag.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The empty flag.
 */
void msh_SetIsEmpty(SharedVariableHeader_t* hdr_ptr, int in);


/**
 * Sets the variable sparsity flag.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The sparsity flag.
 */
void msh_SetIsSparse(SharedVariableHeader_t* hdr_ptr, int in);


/**
 * Sets the variable numeric flag.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The numeric flag.
 */
void msh_SetIsNumeric(SharedVariableHeader_t* hdr_ptr, int in);


/**
 * Returns the virtual scalar flag.
 *
 * @param hdr_ptr The shared variable header.
 * @param in The numeric flag.
 */
void msh_SetIsVirtualScalar(SharedVariableHeader_t* hdr_ptr, int in);


/**
 * Creates a local pointer to the dimensions array.
 *
 * @param hdr_ptr The shared variable header.
 * @return A local pointer to the dimensions array.
 */
mwSize* msh_GetDimensions(SharedVariableHeader_t* hdr_ptr);


/**
 * Creates a local pointer to the data segment.
 *
 * @param hdr_ptr The shared variable header.
 * @return A local pointer to the data segment.
 */
void* msh_GetData(SharedVariableHeader_t* hdr_ptr);


/**
 * Creates a local pointer to the imaginary data segment.
 *
 * @param hdr_ptr The shared variable header.
 * @return A local pointer to the imaginary data segment.
 */
void* msh_GetImagData(SharedVariableHeader_t* hdr_ptr);


/**
 * Creates a local pointer to the ir segment.
 *
 * @param hdr_ptr The shared variable header.
 * @return A local pointer to the ir segment.
 */
mwIndex* msh_GetIr(SharedVariableHeader_t* hdr_ptr);


/**
 * Creates a local pointer to the field names.
 *
 * @param hdr_ptr The shared variable header.
 * @return A local pointer to the field names.
 */
char_t* msh_GetFieldNames(SharedVariableHeader_t* hdr_ptr);


/**
 * Creates a local pointer to the jc segment.
 *
 * @param hdr_ptr The shared variable header.
 * @return A local pointer to the jc segment.
 */
mwIndex* msh_GetJc(SharedVariableHeader_t* hdr_ptr);


/**
 * Creates a local pointer to the child offsets array.
 *
 * @param hdr_ptr The shared variable header.
 * @return A local pointer to the child offsets array.
 */
size_t* msh_GetChildOffsets(SharedVariableHeader_t* hdr_ptr);


/**
 * Creates a local pointer to the specified child of this shared variable header.
 *
 * @param hdr_ptr The shared variable header.
 * @param child_num The index of the child.
 * @return A local pointer to the child.
 */
SharedVariableHeader_t* msh_GetChildHeader(SharedVariableHeader_t* hdr_ptr, size_t child_num);


/**
 * Checks if the variable is complex.
 *
 * @note This function works implicitly by checking if imag_data is NULL.
 * @param hdr_ptr The shared variable header.
 * @return Whether the variable is complex.
 */
int msh_GetIsComplex(SharedVariableHeader_t* hdr_ptr);


/**
 * Returns the complexity value in MEX-specific mxComplexity form.
 *
 * @param hdr_ptr The shared variable header.
 * @return The mxComplexity of this variable.
 */
#define msh_GetComplexity(hdr_ptr) (msh_GetIsComplex(hdr_ptr)? mxCOMPLEX : mxREAL)


/**
 * Finds the shared size of the variable using the matshare header system.
 *
 * @note The returned size does not include the size of segment metadata.
 * @param in_var The variable to be shared.
 * @return The size of the shared variable.
 */
size_t msh_FindSharedSize(const mxArray* in_var);


/**
 * Copy the variable to the destination pointer recursively.
 *
 * @param dest The destination pointer.
 * @param in_var The input variable.
 * @return The number of bytes needed to store this variable and its children.
 */
size_t msh_CopyVariable(void* dest, const mxArray* in_var, int is_top_level);


/**
 * Creates a new mxArray from the specified shared variable header tree.
 *
 * @param shared_header The shared variable header.
 * @return The created mxArray.
 */
mxArray* msh_FetchVariable(SharedVariableHeader_t* shared_header);


/**
 * Overwrites the data in the specified shared variable.
 *
 * @note This is not an atomic operation.
 * @param shared_header The shared variable header.
 * @param in_var The input variable.
 */
void msh_OverwriteHeader(SharedVariableHeader_t* shared_header, const mxArray* in_var);


/**
 * Recursively compares the size of the shared variable in the input variable.
 *
 * @param shared_header The shared variable header.
 * @param comp_var The input comparison variable.
 * @return Whether the variable is the same size.
 */
int msh_CompareHeaderSize(SharedVariableHeader_t* shared_header, const mxArray* comp_var);


/**
 * Detaches the specified variable from shared memoory.
 *
 * @param ret_var The variable to be detached.
 */
void msh_DetachVariable(mxArray* ret_var);


#endif /* MATSHARE_MSHHEADERTYPE_H */
