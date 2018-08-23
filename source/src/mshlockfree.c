/** mshlockfree.c
 * Defines functions for lock-free mechanisms.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include <intrin.h>

#include "mshlockfree.h"

/* returns the state of the flag after the operation */
LockFreeCounter_T msh_IncrementCounter(volatile LockFreeCounter_T* counter)
{
	LockFreeCounter_T old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.count += 1;
	} while(msh_AtomicCompareSetLong(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	return new_counter;
}


/* returns whether the decrement changed the flag to TRUE */
bool_T msh_DecrementCounter(volatile LockFreeCounter_T* counter, bool_T set_flag)
{
	LockFreeCounter_T old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.count -= 1;
		if(set_flag && new_counter.values.count == 0)
		{
			new_counter.values.flag = TRUE;
		}
	} while(msh_AtomicCompareSetLong(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
	return (old_counter.values.flag != new_counter.values.flag);
}


void msh_SetCounterFlag(volatile LockFreeCounter_T* counter, unsigned long val)
{
	LockFreeCounter_T old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.flag = val;
	} while(msh_AtomicCompareSetLong(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	
}


void msh_SetCounterPost(volatile LockFreeCounter_T* counter, unsigned long val)
{
	LockFreeCounter_T old_counter, new_counter;
	do
	{
		old_counter.span = counter->span;
		new_counter.span = old_counter.span;
		new_counter.values.post = val;
	} while(msh_AtomicCompareSetLong(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
}


unsigned long msh_GetCounterCount(volatile LockFreeCounter_T* counter)
{
	return counter->values.count;
}


unsigned long msh_GetCounterFlag(volatile LockFreeCounter_T* counter)
{
	return counter->values.flag;
}


unsigned long msh_GetCounterPost(volatile LockFreeCounter_T* counter)
{
	return counter->values.post;
}


void msh_WaitSetCounter(volatile LockFreeCounter_T* counter, unsigned long val)
{
	LockFreeCounter_T old_counter, new_counter;
	
	msh_SetCounterPost(counter, FALSE);
	old_counter.values.count = 0;
	old_counter.values.post = FALSE;
	new_counter.values.count = 0;
	new_counter.values.flag = val;
	new_counter.values.post = TRUE;
	do
	{
		old_counter.values.flag = counter->values.flag;
	} while(msh_AtomicCompareSetLong(&counter->span, old_counter.span, new_counter.span) != old_counter.span);
	/* this also sets post to TRUE */
}


bool_T msh_AtomicAddSizeWithMax(volatile size_t* dest, size_t add_value, size_t max_value)
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
	} while(InterlockedCompareExchange64((volatile __int64*)dest, new_value, old_value) != (__int64)old_value);
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


long msh_AtomicCompareSetLong(volatile long* dest, long comp_val, long set_val)
{
#ifdef MSH_WIN
	return InterlockedCompareExchange(dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}


int8_T msh_AtomicCompareSetInt8(volatile int8_T* dest, int8_T comp_val, int8_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return _InterlockedCompareExchange8((volatile char*)dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}

int16_T msh_AtomicCompareSetInt16(volatile int16_T* dest, int16_T comp_val, int16_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return _InterlockedCompareExchange16(dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}


int32_T msh_AtomicCompareSetInt32(volatile int32_T* dest, int32_T comp_val, int32_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return _InterlockedCompareExchange((volatile long*)dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}

#if MSH_BITNESS==64
int64_T msh_AtomicCompareSetInt64(volatile int64_T* dest, int64_T comp_val, int64_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return _InterlockedCompareExchange64((volatile __int64*)dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}
#endif


uint8_T msh_AtomicCompareSetUInt8(volatile uint8_T* dest, uint8_T comp_val, uint8_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return (uint8_T)_InterlockedCompareExchange8((volatile char*)dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}

uint16_T msh_AtomicCompareSetUInt16(volatile uint16_T* dest, uint16_T comp_val, uint16_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return (uint16_T)_InterlockedCompareExchange16((volatile short*)dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}


uint32_T msh_AtomicCompareSetUInt32(volatile uint32_T* dest, uint32_T comp_val, uint32_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return (uint32_T)_InterlockedCompareExchange((volatile long*)dest, (long)set_val, (long)comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}

#if MSH_BITNESS==64
uint64_T msh_AtomicCompareSetUInt64(volatile uint64_T* dest, uint64_T comp_val, uint64_T set_val)
{
	/* returns the initial value of dest */
#ifdef MSH_WIN
	return (uint64_T)_InterlockedCompareExchange64((volatile __int64*)dest, set_val, comp_val);
#else
	return __sync_val_compare_and_swap(dest, comp_val, set_val);
#endif
}
#endif


single msh_AtomicCompareSetSingle(volatile single* dest, single comp_val, single set_val)
{
	union singlepun_T
	{
		single val;
		int32_T as_int;
	};
	
	union singlepun_T comp_val_pun = {comp_val};
	union singlepun_T set_val_pun = {set_val};
	union singlepun_T ret = {0};
#ifdef MSH_WIN
	ret.as_int = _InterlockedCompareExchange((volatile long*)dest, set_val_pun.as_int, comp_val_pun.as_int);
	return ret.val;
#else
	ret.as_int = __sync_val_compare_and_swap((volatile int32_T*)dest, comp_val_pun.as_int, set_val_pun.as_int);
	return ret.val;
#endif
}

double msh_AtomicCompareSetDouble(volatile double* dest, double comp_val, double set_val)
{
#ifdef MSH_WIN

#  if MSH_BITNESS==64
	
	union doublepun_T
	{
		double val;
		int64_T as_int;
	};
	
	union doublepun_T ret;
	union doublepun_T comp_val_pun = {comp_val};
	union doublepun_T set_val_pun = {set_val};
	ret.as_int = _InterlockedCompareExchange64((volatile long long*)dest, set_val_pun.as_int, comp_val_pun.as_int);
	return ret.val;
	
#  else
	
	/* set EDI as dest (pointer)
	 * load comp_val into EDX:EAX
	 * load set_val into ECX:EBX
	 * perform interlocked exchange
	 * return EDX:EAX which set if not equal
	 *
	 * note ret is res here because ret is an asm keyword
	 */
	
	union doublepun_T
	{
		double val;
		struct
		{
			uint32_T lower;
			uint32_T upper;
		} as_int;
	};
	
	union doublepun_T res;
	union doublepun_T comp_val_pun = {comp_val};
	union doublepun_T set_val_pun = {set_val};
	__asm {
		mov EDI, dest;
		mov EAX, comp_val_pun.as_int.lower;
		mov EDX, comp_val_pun.as_int.upper;
		mov EBX, set_val_pun.as_int.lower;
		mov ECX, set_val_pun.as_int.upper;
		lock cmpxchg8b [EDI];
		mov DWORD PTR res.as_int.lower, EAX;
		mov DWORD PTR res.as_int.upper, EDX;
	};
	return res.val;
#  endif

#else
	union doublepun_T
	{
		double val;
		int64_T as_int;
	};
	
	union doublepun_T ret;
	union doublepun_T comp_val_pun = {comp_val};
	union doublepun_T set_val_pun = {set_val};
	ret.as_int = __sync_val_compare_and_swap((volatile int64_T*)dest, comp_val_pun.as_int, set_val_pun.as_int);
	return ret.val;
#endif
}

