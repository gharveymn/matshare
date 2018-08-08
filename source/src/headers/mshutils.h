/** mshutils.h
 * Declares miscellaneous utility functions.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include "mshbasictypes.h"

typedef struct ParsedIndices_t
{
	mwIndex* starting_indices;
	size_t   num_indices;
	mwSize*  slice_lens;
	size_t   num_slices;
} ParsedIndices_t;

/**
 * Acquires the specified interprocess lock.
 *
 * @note Does not lock if thread safety is turned off.
 * @param process_lock The interprocess lock to acquire.
 */
void msh_AcquireProcessLock(ProcessLock_t process_lock);


/**
 * Releases the specified interprocess lock.
 *
 * @note Does not try to release anything if not already locked.
 * @param process_lock The interprocess lock to release.
 */
void msh_ReleaseProcessLock(ProcessLock_t process_lock);


/**
 * Writes the current configuration to the config file.
 */
void msh_WriteConfiguration(void);


/**
 * Gets the path of the configuration file, creating needed directories along the way.
 *
 * @note The return must be freed.
 * @return The path of the configuration file.
 */
char_t* msh_GetConfigurationPath(void);


/**
 * Gets the PID for the current process.
 *
 * @return The PID for the current process.
 */
pid_t msh_GetPid(void);


/**
 * Compares the size of the two mxArrays. Returns TRUE if
 * they are the same size, returns FALSE otherwise.
 *
 * @param dest_var The first variable to be compared.
 * @param comp_var The second variable to be compared.
 * @return Whether they are equal in size.
 */
int msh_CompareVariableSize(const mxArray* dest_var, const mxArray* comp_var, ParsedIndices_t* parsed_indices);


/**
 * Overwrites the data in dest_var with the data in in_var.
 *
 * @param dest_var The destination variable.
 * @param in_var The input variable.
 */
void msh_OverwriteVariable(const mxArray* dest_var, const mxArray* in_var, ParsedIndices_t* parsed_indices, int will_sync);


/**
 * Writes the segment name to the name buffer.
 *
 * @param name_buffer The destination of the segment name.
 * @param seg_num The segment number used by matshare to identify the segment.
 */
void msh_WriteSegmentName(char* name_buffer, segmentnumber_t seg_num);


/**
 * Pads the input size to the alignment specified by ALIGN_SIZE and ALIGN_SHIFT.
 *
 * @note Do not use for curr_sz = 0.
 * @param curr_sz The size to pad.
 * @return The padded size.
 */
size_t msh_PadToAlignData(size_t curr_sz);

void msh_CheckVarname(const mxArray* varname);

uint32_T msh_MurmurHash3(const uint8_T* key, size_t len, int seed);

#endif /* MATSHARE_MATSHAREUTILS_H */
