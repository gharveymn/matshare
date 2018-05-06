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

#define MSH_PARAM_SECURITY "Security"
#define MSH_PARAM_SECURITY_L "security"

#define MSH_PARAM_COPYONWRITE "CopyOnWrite"
#define MSH_PARAM_COPYONWRITE_L "copyonwrite"

#define MSH_PARAM_GC "GC"
#define MSH_PARAM_GC_L "gc"

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


void MshOnExit(void);

void MshOnError(void);

void AcquireProcessLock(void);

void ReleaseProcessLock(void);

msh_directive_t ParseDirective(const mxArray* in);

void UpdateAll(void);

void RemoveUnusedVariables(void);

/**
 * Writes the segment name to the name buffer.
 *
 * @param name_buffer The destination of the segment name.
 * @param seg_num The segment number used by matshare to identify the segment.
 */
void WriteSegmentName(char name_buffer[MSH_MAX_NAME_LEN], msh_segmentnumber_t seg_num);

void NullFunction(void);

#endif /* MATSHARE_MATSHAREUTILS_H */
