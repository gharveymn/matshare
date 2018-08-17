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

/**
 * Acquires the specified interprocess lock.
 *
 * @note Does not lock if thread safety is turned off.
 * @param file_lock The interprocess lock to acquire.
 */
void msh_AcquireProcessLock(FileLock_T file_lock);


/**
 * Releases the specified interprocess lock.
 *
 * @note Does not try to release anything if not already locked.
 * @param file_lock The interprocess lock to release.
 */
void msh_ReleaseProcessLock(FileLock_T file_lock);


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
char_T* msh_GetConfigurationPath(void);


/**
 * Gets the PID for the current process.
 *
 * @return The PID for the current process.
 */
pid_T msh_GetPid(void);


/**
 * Writes the segment name to the name buffer.
 *
 * @param name_buffer The destination of the segment name.
 * @param seg_num The segment number used by matshare to identify the segment.
 */
void msh_WriteSegmentName(char* name_buffer, segmentnumber_T seg_num);


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
