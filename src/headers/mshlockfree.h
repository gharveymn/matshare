#ifndef MATSHARE_MSHLOCKFREE_H
#define MATSHARE_MSHLOCKFREE_H

#include "mshbasictypes.h"

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


#endif /* MATSHARE_MSHLOCKFREE_H */
