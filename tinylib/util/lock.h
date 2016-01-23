
#ifndef UTIL_LOCK_H
#define UTIL_LOCK_H

#include "util/log.h"

#ifdef WIN32

#include <windows.h>

typedef CRITICAL_SECTION lock_t;

static inline
int lock_init(lock_t *lock)
{
	if (InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)lock, 1000) == 0)
	{
		log_error("lock_init: InitializeCriticalSectionAndSpinCount(), errno: %d", GetLastError());
		return -1;
	}
	
	return 0;
}

static inline
void lock_uninit(lock_t *lock)
{
	DeleteCriticalSection((CRITICAL_SECTION*)lock);
	
	return;
}

static inline
int lock_it(lock_t *lock)
{
	EnterCriticalSection((CRITICAL_SECTION*)lock);
	
	return 0;
}

static inline
int trylock_it(lock_t *lock)
{
	return TryEnterCriticalSection((CRITICAL_SECTION*)lock) == 0 ? -1 : 0;
}

static inline
void unlock_it(lock_t *lock)
{
	LeaveCriticalSection((CRITICAL_SECTION*)lock);
	
	return;
}

#else
	
#include <pthread.h>

typedef pthread_spinlock_t lock_t;

static inline
int lock_init(lock_t *lock)
{
	int ret = pthread_spin_init((pthread_spinlock_t*)lock, 0);
	if (0 != ret)
	{
		log_error("lock_init: pthread_spin_init(), errno: %d", ret);
	}

	return ret;
}

static inline
void lock_uninit(lock_t *lock)
{
	int ret = pthread_spin_destroy((pthread_spinlock_t*)lock);
	if (0 != ret)
	{
		log_error("lock_uninit: pthread_spin_destroy(), errno: %d", ret);
	}

	return;
}

static inline
int lock_it(lock_t *lock)
{
	int ret = pthread_spin_lock((pthread_spinlock_t*)lock);
	if (0 != ret)
	{
		log_error("lock_uninit: pthread_spin_lock(), errno: %d", ret);
	}

	return ret;
}

static inline
int trylock_it(lock_t *lock)
{
	return pthread_spin_trylock((pthread_spinlock_t*)lock);
}

static inline
void unlock_it(lock_t *lock)
{
	int ret =  pthread_spin_unlock((pthread_spinlock_t*)lock);
	if (0 != ret)
	{
		log_error("lock_uninit:  pthread_spin_unlock(), errno: %d", ret);
	}

	return;
}

#endif

#endif
