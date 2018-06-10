#ifndef MATSHARE_MATSHAREUTILS_H
#define MATSHARE_MATSHAREUTILS_H

#include "mshtypes.h"

/* for lcc compatibility --- requires Windows XP+ */
#ifdef MSH_WIN
#  ifndef InterlockedCompareExchange
/*LONG __cdecl InterlockedCompareExchange(_Inout_ LONG volatile *Destination, _In_ LONG Exchange, _In_ LONG Comparand);*/
LONG STDCALL InterlockedCompareExchange(LPLONG, LONG, LONG);
#  endif
#endif

void msh_OnExit(void);

void msh_OnError(void);

void msh_AcquireProcessLock(void);

void msh_ReleaseProcessLock(void);

void msh_WriteConfiguration(void);
char_t* msh_GetConfigurationPath();
void msh_SetDefaultConfiguration(void);

unsigned long msh_GetCounterCount(volatile LockFreeCounter_t* counter);
unsigned long msh_GetCounterFlag(volatile LockFreeCounter_t* counter);
unsigned long msh_GetCounterPost(volatile LockFreeCounter_t* counter);
LockFreeCounter_t msh_IncrementCounter(volatile LockFreeCounter_t* counter);
bool_t msh_DecrementCounter(volatile LockFreeCounter_t* counter, bool_t set_flag);
void msh_SetCounterFlag(volatile LockFreeCounter_t* counter, unsigned long val);
void msh_SetCounterPost(volatile LockFreeCounter_t* counter, unsigned long val);
void msh_WaitSetCounter(volatile LockFreeCounter_t* counter, unsigned long val);

bool_t msh_AtomicAddSizeWithMax(volatile size_t* value_pointer, size_t add_value, size_t max_value);
size_t msh_AtomicSubtractSize(volatile size_t* value_pointer, size_t subtract_value);
long msh_AtomicAddLong(volatile long* value_pointer, long add_value);
long msh_AtomicIncrement(volatile long* value_pointer);
long msh_AtomicDecrement(volatile long* value_pointer);
long msh_AtomicCompareSwap(volatile long* value_pointer, long compare_value, long swap_value);

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
