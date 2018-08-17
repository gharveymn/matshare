#include "mshlockfree.h"

/* returns the state of the flag after the operation */
LockFreeCounter_t msh_IncrementCounter(volatile LockFreeCounter_t* counter)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.count += 1;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	return new_counter;
}


/* returns whether the decrement changed the flag to TRUE */
bool_t msh_DecrementCounter(volatile LockFreeCounter_t* counter, bool_t set_flag)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.count -= 1;
		if(set_flag && new_counter.values.count == 0)
		{
			new_counter.values.flag = TRUE;
		}
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
	return (old_counter.values.flag != new_counter.values.flag);
}


void msh_SetCounterFlag(volatile LockFreeCounter_t* counter, unsigned long val)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.flag = val;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
}


void msh_SetCounterPost(volatile LockFreeCounter_t* counter, unsigned long val)
{
	LockFreeCounter_t old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.post = val;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
}


unsigned long msh_GetCounterCount(volatile LockFreeCounter_t* counter)
{
	return counter->values.count;
}


unsigned long msh_GetCounterFlag(volatile LockFreeCounter_t* counter)
{
	return counter->values.flag;
}


unsigned long msh_GetCounterPost(volatile LockFreeCounter_t* counter)
{
	return counter->values.post;
}


void msh_WaitSetCounter(volatile LockFreeCounter_t* counter, unsigned long val)
{
	LockFreeCounter_t old_counter, new_counter;
	
	msh_SetCounterPost(counter, FALSE);
	old_counter.values.count = 0;
	old_counter.values.post = FALSE;
	new_counter.values.count = 0;
	new_counter.values.flag = val;
	new_counter.values.post = TRUE;
	do
	{
		old_counter.values.flag = counter->values.flag;
	} while(msh_AtomicCompareSwap(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	/* this also sets post to TRUE */
}


bool_t msh_AtomicAddSizeWithMax(volatile size_t* dest, size_t add_value, size_t max_value)
{
	size_t old_value, new_value;
#ifdef MSH_WIN
	#  if MSH_BITNESS==64
	do
	{
		old_value = *dest;
		new_value = old_value + add_value;
		if(new_value > max_value)
		{
			return FALSE;
		}
	} while((size_t)InterlockedCompareExchange64((volatile long long*)dest, new_value, old_value) != old_value);
#  elif MSH_BITNESS==32
	do
	{
		old_value = *dest;
		new_value = old_value + add_value;
		if(new_value > max_value)
		{
			return FALSE;
		}
	} while((size_t)InterlockedCompareExchange((volatile long*)dest, new_value, old_value) != old_value);
#  endif
#else
	do
	{
		old_value = *dest;
		new_value = old_value + add_value;
		if(new_value > max_value)
		{
			return FALSE;
		}
	} while(__sync_val_compare_and_swap(dest, old_value, new_value) != old_value);
#endif
	return TRUE;
}


size_t msh_AtomicSubtractSize(volatile size_t* dest, size_t subtract_value)
{
#ifdef MSH_WIN
	/* using compare swaps because windows doesn't have unsigned interlocked functions */
	size_t old_value, new_value;
#  if MSH_BITNESS==64
	do
	{
		old_value = *dest;
		new_value = old_value - subtract_value;
	} while(InterlockedCompareExchange64((volatile long long*)dest, new_value, old_value) != (long long)old_value);
#  elif MSH_BITNESS==32
	do
	{
		old_value = *dest;
		new_value = old_value - subtract_value;
	} while(InterlockedCompareExchange((volatile long*)dest, new_value, old_value) != (long)old_value);
#  endif
	return new_value;
#else
	return __sync_sub_and_fetch(dest, subtract_value);
#endif
}


/*
long msh_AtomicAddLong(volatile long* dest, long add_value)
{
#ifdef MSH_WIN
	return InterlockedAdd(dest, add_value);
#else
	return __sync_add_and_fetch(dest, add_value);
#endif
}
*/


long msh_AtomicIncrement(volatile long* dest)
{
#ifdef MSH_WIN
	return InterlockedIncrement(dest);
#else
	return __sync_add_and_fetch(dest, 1);
#endif
}


long msh_AtomicDecrement(volatile long* dest)
{
#ifdef MSH_WIN
	return InterlockedDecrement(dest);
#else
	return __sync_sub_and_fetch(dest, 1);
#endif
}


long msh_AtomicCompareSwap(volatile long* dest, long compare_value, long swap_value)
{
#ifdef MSH_WIN
	return InterlockedCompareExchange(dest, swap_value, compare_value);
#else
	return __sync_val_compare_and_swap(dest, compare_value, swap_value);
#endif
}
