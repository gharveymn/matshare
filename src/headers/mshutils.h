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

/* for lcc compatibility --- matshare requires at least Windows XP */
#if defined(MSH_WIN) && defined(__LCC__)
/*LONG __cdecl InterlockedCompareExchange(_Inout_ LONG volatile *Destination, _In_ LONG Exchange, _In_ LONG Comparand);*/
LONG STDCALL InterlockedCompareExchange(LPLONG, LONG, LONG);
#endif


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
 * Sets the shared configuration to default.
 */
void msh_SetDefaultConfiguration(void);


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
int msh_CompareVariableSize(const mxArray* dest_var, const mxArray* comp_var);


/**
 * Overwrites the data in dest_var with the data in in_var.
 *
 * @param dest_var The destination variable.
 * @param in_var The input variable.
 */
void msh_OverwriteVariable(const mxArray* dest_var, const mxArray* in_var);


/**
 * Gets the current count of the lock free counter.
 *
 * @param counter The counter.
 * @return The current count.
 */
unsigned long msh_GetCounterCount(volatile LockFreeCounter_t* counter);


/**
 * Gets the current value of the counter flag.
 *
 * @param counter The counter.
 * @return The value of the counter flag.
 */
unsigned long msh_GetCounterFlag(volatile LockFreeCounter_t* counter);


/**
 * Gets the current value of the counter post flag.
 *
 * @param counter The counter.
 * @return The value of the counter post flag.
 */
unsigned long msh_GetCounterPost(volatile LockFreeCounter_t* counter);


/**
 * Performs an atomic increment of the specified counter.
 *
 * @param counter The counter.
 * @return The counter immediately after the increment.
 */
LockFreeCounter_t msh_IncrementCounter(volatile LockFreeCounter_t* counter);


/**
 * Performs an atomic decrement of the specified counter. If set_flag is
 * TRUE then the counter will set the counter flag if the count is 0.
 *
 * @param counter The counter.
 * @param set_flag A flag determining whether to set the counter flag to TRUE if the count is 0.
 * @return Whether the counter flipped the counter flag from FALSE to TRUE.
 */
bool_t msh_DecrementCounter(volatile LockFreeCounter_t* counter, bool_t set_flag);


/**
 * Sets the counter flag.
 *
 * @param counter The counter.
 * @param val The value for the flag.
 */
void msh_SetCounterFlag(volatile LockFreeCounter_t* counter, unsigned long val);


/**
 * Sets the counter post flag.
 *
 * @param counter The counter.
 * @param val The value for the post flag.
 */
void msh_SetCounterPost(volatile LockFreeCounter_t* counter, unsigned long val);


/**
 * Sets the counter flag to the specified value, but
 * busy-waits until the counter is 0 to do so.
 *
 * @param counter The counter.
 * @param val The value for the flag.
 */
void msh_WaitSetCounter(volatile LockFreeCounter_t* counter, unsigned long val);


/**
 * Adds the size_t value to the pointer up to a certain maximum value.
 *
 * @param dest A pointer to the destination.
 * @param add_value The value to be added.
 * @param max_value The maximum value of dest.
 * @return Whether the operation succeeded.
 */
bool_t msh_AtomicAddSizeWithMax(volatile size_t* dest, size_t add_value, size_t max_value);


/**
 * Subtracts the size_t value from the pointer.
 *
 * @param dest A pointer to the destination.
 * @param subtract_value The value to be subtracted.
 * @return The destination value immediately after the operation.
 */
size_t msh_AtomicSubtractSize(volatile size_t* dest, size_t subtract_value);


/**
 * Increments the destination value by 1.
 *
 * @param dest A pointer to the destination.
 * @return The destination value immediately after the operation.
 */
long msh_AtomicIncrement(volatile long* dest);


/**
 * Decrements the destination value by 1.
 *
 * @param dest A pointer to the destination.
 * @return The destination value immediately after the operation.
 */
long msh_AtomicDecrement(volatile long* dest);


/**
 * Compares the value at the destination with a comparison value
 * and sets the destination value to a specified value if they are
 * the same.
 *
 * @param dest A pointer to the destination.
 * @param compare_value The value to compare with the value at the destination.
 * @param swap_value The value to set the destination if the comparison is true.
 * @return The value of the destination during comparison.
 */
long msh_AtomicCompareSwap(volatile long* dest, long compare_value, long swap_value);


#ifdef MSH_DEBUG_PERF


/**
 * Gets the current CPU tick.
 *
 * @param tick_pointer A pointer to the value which will be set by this.
 */
void msh_GetTick(msh_tick_t* tick_pointer);


/**
 * Gets the difference in tick values in the tick tracker.
 *
 * @param tracker A CPU tick tracker holding new and old CPU tick values.
 * @return The difference in tick values.
 */
size_t msh_GetTickDifference(TickTracker_t* tracker);


#endif


/**
 * Writes the segment name to the name buffer.
 *
 * @param name_buffer The destination of the segment name.
 * @param seg_num The segment number used by matshare to identify the segment.
 */
void msh_WriteSegmentName(char* name_buffer, msh_segmentnumber_t seg_num);


/**
 * Pads the input size to the alignment specified by ALIGN_SIZE and ALIGN_SHIFT.
 *
 * @note Do not use for curr_sz = 0.
 * @param curr_sz The size to pad.
 * @return The padded size.
 */
size_t PadToAlignData(size_t curr_sz);


#endif /* MATSHARE_MATSHAREUTILS_H */
