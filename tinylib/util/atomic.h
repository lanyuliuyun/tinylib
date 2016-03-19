
#ifndef UTIL_ATOMIC_H
#define UTIL_ATOMIC_H

typedef long atomic_t;

#ifdef WINNT

#include <windows.h>

/* 返回旧值 */
#define atomic_set(atomic, value) (InterlockedExchange((LPLONG)(atomic), (value)))

#define atomic_set_ptr(pptr, ptr) (InterlockedExchangePointer((PVOID *)(pptr), (ptr)))

#define atomic_get(atomic) (InterlockedCompareExchange((LPLONG)(atomic), 0, 0))

/* 返回旧值 */
#define atomic_inc(atomic) (InterlockedExchangeAdd((LPLONG)(atomic), 1))

/* 返回旧值 */
#define atomic_dec(atomic) (InterlockedExchangeAdd((LPLONG)(atomic), -1))

#define atomic_cas(atomic, comp, value)    (InterlockedCompareExchange((LPLONG)(atomic), (value), (comp)))

#define atomic_cas_ptr(pptr, ptr_comp, ptr_value)  (InterlockedCompareExchangePointer((PVOID*)(pptr), (ptr_value), (ptr_comp)))

#elif defined(__GNUC__)

/* 返回旧值 */
#define atomic_set(atomic, value)    (__sync_lock_test_and_set((atomic), (value)))

#define atomic_set_ptr(pptr, ptr)  (__sync_lock_test_and_set((pptr), (ptr)))

#define atomic_get(atomic)     (__sync_val_compare_and_swap((atomic), 0, 0))

/* 返回旧值 */
#define atomic_inc(atomic)    (__sync_fetch_and_add((atomic), 1))

/* 返回旧值 */
#define atomic_dec(atomic)        (__sync_fetch_and_sub((atomic), 1))

#define atomic_cas(atomic, comp, value)    (__sync_val_compare_and_swap((atomic), (comp), (value)))

#define atomic_cas_ptr(pptr, ptr_comp, ptr_value)        (__sync_val_compare_and_swap((pptr), (ptr_comp), (ptr_value)))

#endif

#endif /* !UTIL_ATOMIC_H */
