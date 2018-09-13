/** mshlockfree.h
 * Declares functions for lock-free mechanisms.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef MATSHARE_MSHLOCKFREE_H
#define MATSHARE_MSHLOCKFREE_H

#include "mshbasictypes.h"

#define VO_FCN_CASNAME_(TYPEN) msh_AtomicCompareSet##TYPEN
#define VO_FCN_CASNAME(TYPEN) VO_FCN_CASNAME_(TYPEN)

#define VO_FCN_SNAME_(TYPEN) msh_AtomicSet##TYPEN
#define VO_FCN_SNAME(TYPEN) VO_FCN_SNAME_(TYPEN)


/* for lcc compatibility --- matshare requires at least Windows XP */
#if defined(MSH_WIN) && defined(__LCC__)
/*LONG __cdecl InterlockedCompareExchange(_Inout_ LONG volatile *Destination, _In_ LONG Exchange, _In_ LONG Comparand);*/
LONG STDCALL InterlockedCompareExchange(LPLONG, LONG, LONG);
#endif

/**
 * Gets the current count of the lock free counter.
 *
 * @param counter The counter.
 * @return The current count.
 */
unsigned long msh_GetCounterCount(volatile LockFreeCounter_T* counter);


/**
 * Gets the current value of the counter flag.
 *
 * @param counter The counter.
 * @return The value of the counter flag.
 */
unsigned long msh_GetCounterFlag(volatile LockFreeCounter_T* counter);


/**
 * Gets the current value of the counter post flag.
 *
 * @param counter The counter.
 * @return The value of the counter post flag.
 */
unsigned long msh_GetCounterPost(volatile LockFreeCounter_T* counter);


/**
 * Performs an atomic increment of the specified counter.
 *
 * @param counter The counter.
 * @return The counter immediately after the increment.
 */
LockFreeCounter_T msh_IncrementCounter(volatile LockFreeCounter_T* counter);


/**
 * Performs an atomic decrement of the specified counter. If set_flag is
 * TRUE then the counter will set the counter flag if the count is 0.
 *
 * @param counter The counter.
 * @param set_flag A flag determining whether to set the counter flag to TRUE if the count is 0.
 * @return Whether the counter flipped the counter flag from FALSE to TRUE.
 */
bool_T msh_DecrementCounter(volatile LockFreeCounter_T* counter, bool_T set_flag);


/**
 * Sets the counter flag.
 *
 * @param counter The counter.
 * @param val The value for the flag.
 */
void msh_SetCounterFlag(volatile LockFreeCounter_T* counter, unsigned long val);


/**
 * Sets the counter post flag.
 *
 * @param counter The counter.
 * @param val The value for the post flag.
 */
void msh_SetCounterPost(volatile LockFreeCounter_T* counter, unsigned long val);


/**
 * Sets the counter flag to the specified value, but
 * busy-waits until the counter is 0 to do so.
 *
 * @param counter The counter.
 * @param val The value for the flag.
 */
void msh_WaitSetCounter(volatile LockFreeCounter_T* counter, unsigned long val);


/**
 * Adds the size_t value to the pointer up to a certain maximum value.
 *
 * @param dest A pointer to the destination.
 * @param add_value The value to be added.
 * @param max_value The maximum value of dest.
 * @return Whether the operation succeeded.
 */
bool_T msh_AtomicAddSizeWithMax(volatile size_t* dest, size_t add_value, size_t max_value);


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
 * @param comp The value to compare with the value at the destination.
 * @param set_val The value to set the destination if the comparison is true.
 * @return The value of the destination during comparison.
 */
long msh_AtomicCompareSetLong(volatile long* dest, long comp, long set_val);


int8_T  VO_FCN_CASNAME(Int8)(volatile int8_T* dest, int8_T comp_val, int8_T set_val);
int16_T VO_FCN_CASNAME(Int16)(volatile int16_T* dest, int16_T comp_val, int16_T set_val);
int32_T VO_FCN_CASNAME(Int32)(volatile int32_T* dest, int32_T comp_val, int32_T set_val);

uint8_T  VO_FCN_CASNAME(UInt8)(volatile uint8_T* dest, uint8_T comp_val, uint8_T set_val);
uint16_T VO_FCN_CASNAME(UInt16)(volatile uint16_T* dest, uint16_T comp_val, uint16_T set_val);
uint32_T VO_FCN_CASNAME(UInt32)(volatile uint32_T* dest, uint32_T comp_val, uint32_T set_val);

int64_T VO_FCN_CASNAME(Int64)(volatile int64_T* dest, int64_T comp_val, int64_T set_val);
uint64_T VO_FCN_CASNAME(UInt64)(volatile uint64_T* dest, uint64_T comp_val, uint64_T set_val);

single VO_FCN_CASNAME(Single)(volatile single* dest, single comp_val, single set_val);
double VO_FCN_CASNAME(Double)(volatile double* dest, double comp_val, double set_val);

int8_T  VO_FCN_SNAME(Int8)(volatile int8_T* dest, int8_T set_val);
int16_T VO_FCN_SNAME(Int16)(volatile int16_T* dest, int16_T set_val);
int32_T VO_FCN_SNAME(Int32)(volatile int32_T* dest, int32_T set_val);

uint8_T  VO_FCN_SNAME(UInt8)(volatile uint8_T* dest, uint8_T set_val);
uint16_T VO_FCN_SNAME(UInt16)(volatile uint16_T* dest, uint16_T set_val);
uint32_T VO_FCN_SNAME(UInt32)(volatile uint32_T* dest, uint32_T set_val);

int64_T VO_FCN_SNAME(Int64)(volatile int64_T* dest, int64_T set_val);
uint64_T VO_FCN_SNAME(UInt64)(volatile uint64_T* dest, uint64_T set_val);

single VO_FCN_SNAME(Single)(volatile single* dest, single set_val);
double VO_FCN_SNAME(Double)(volatile double* dest, double set_val);

size_t msh_AtomicSetSize(volatile size_t* dest, size_t set_val);


#endif /* MATSHARE_MSHLOCKFREE_H */
