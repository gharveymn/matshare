#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include "mshbasictypes.h"

typedef enum
{
	msh_SHARE = 0,
	msh_FETCH = 1,
	msh_DETACH = 2,
	msh_PARAM = 3,
	msh_DEEPCOPY = 4,
	msh_DEBUG = 5, /* unused */
	msh_OBJ_REGISTER = 6,
	msh_OBJ_DEREGISTER = 7,
	msh_INIT = 8, /* unused */
	msh_CLEAR = 9
} msh_directive_t;

void msh_OnExit(void);

void msh_OnError(void);

void msh_AcquireProcessLock(void);

void msh_ReleaseProcessLock(void);

msh_directive_t msh_ParseDirective(const mxArray* in);

void msh_UpdateAll(void);

void msh_VariableGC(void);

/**
 * Writes the segment name to the name buffer.
 *
 * @param name_buffer The destination of the segment name.
 * @param seg_num The segment number used by matshare to identify the segment.
 */
void msh_WriteSegmentName(char* name_buffer, msh_segmentnumber_t seg_num);

void msh_NullFunction(void);

#endif /* MATSHARE_MATSHAREUTILS_H */
