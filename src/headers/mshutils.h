#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include "mshtypes.h"

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

unsigned long msh_GetCounterCount(LockFreeCounter_t* counter);
unsigned long msh_GetCounterFlag(LockFreeCounter_t* counter);
unsigned long msh_GetCounterPost(LockFreeCounter_t* counter);
LockFreeCounter_t msh_IncrementCounter(LockFreeCounter_t* counter);
bool_t msh_DecrementCounter(LockFreeCounter_t* counter, bool_t set_flag);
void msh_SetCounterFlag(LockFreeCounter_t* counter, unsigned long val);
void msh_SetCounterPost(LockFreeCounter_t* counter, unsigned long val);
void msh_WaitSetCounter(LockFreeCounter_t* counter, unsigned long val);

long msh_AtomicIncrement(volatile long* val_ptr);
long msh_AtomicDecrement(volatile long* val_ptr);
long msh_AtomicCompareSwap(volatile long* val_ptr, long compare_value, long swap_value);

/**
 * Writes the segment name to the name buffer.
 *
 * @param name_buffer The destination of the segment name.
 * @param seg_num The segment number used by matshare to identify the segment.
 */
void msh_WriteSegmentName(char* name_buffer, msh_segmentnumber_t seg_num);

void msh_NullFunction(void);

pid_t msh_GetPid(void);

#endif /* MATSHARE_MATSHAREUTILS_H */
