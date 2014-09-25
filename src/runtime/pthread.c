#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <assert.h>

#define FUTEX_WAIT		0
#define FUTEX_WAKE		1
#define FUTEX_REQUEUE		3
#define FUTEX_CMP_REQUEUE	4
#define FUTEX_WAKE_OP		5
#define FUTEX_OP_CLEAR_WAKE_IF_GT_ONE	((4 << 24) | 1)
#define FUTEX_LOCK_PI		6
#define FUTEX_UNLOCK_PI		7
#define FUTEX_TRYLOCK_PI	8
#define FUTEX_WAIT_BITSET	9
#define FUTEX_WAKE_BITSET	10
#define FUTEX_PRIVATE_FLAG	128
#define FUTEX_CLOCK_REALTIME	256

#define LLL_PRIVATE	0
#define LLL_SHARED	FUTEX_PRIVATE_FLAG

#define lll_futex_wait(futexp, val, private) \
  lll_futex_timed_wait(futexp, val, NULL, private)

#define lll_futex_timed_wait(futexp, val, timespec, private) \
  ({									      \
    syscall(SYS_futex, futexp, FUTEX_WAIT, val, timespec);				      \
  })

#define lll_futex_wake(futexp, nr, private) \
  ({		\
    syscall(SYS_futex, futexp, FUTEX_WAKE, nr);					      \
  })

#define atomic_compare_and_exchange_val_acq(mem, newval, oldval) \
  __sync_val_compare_and_swap ((mem), (oldval), (newval))

#define atomic_compare_and_exchange_bool_acq(mem, newval, oldval) \
  ({ /* Cannot use __oldval here, because macros later in this file might     \
	call this macro with __oldval argument.	 */			      \
     __typeof (oldval) __atg3_old = (oldval);				      \
     atomic_compare_and_exchange_val_acq (mem, newval, __atg3_old)	      \
       != __atg3_old;							      \
  })

#define __lll_lock(futex, private)					      \
  ((void) ({								      \
    int *__futex = (futex);						      \
    if (__builtin_expect (atomic_compare_and_exchange_val_acq (__futex,       \
								1, 0), 0))    \
      {									      \
	if (__builtin_constant_p (private) && (private) == LLL_PRIVATE)	      \
	  __lll_lock_wait_private (__futex);				      \
	else								      \
	  assert(0);/*lll_lock_wait (__futex, private);*/				      \
      }									      \
  }))
#define lll_lock(futex, private) __lll_lock (&(futex), private)

#if LLL_PRIVATE == 0 && LLL_SHARED == 128
# define PTHREAD_MUTEX_PSHARED(m) \
  ((m)->__data.__kind & 128)
#else
# define PTHREAD_MUTEX_PSHARED(m) \
  (((m)->__data.__kind & 128) ? LLL_SHARED : LLL_PRIVATE)
#endif

#ifndef LLL_MUTEX_LOCK
# define LLL_MUTEX_LOCK(mutex) \
  lll_lock ((mutex)->__data.__lock, PTHREAD_MUTEX_PSHARED (mutex))
# define LLL_MUTEX_TRYLOCK(mutex) \
  lll_trylock ((mutex)->__data.__lock)
# define LLL_ROBUST_MUTEX_LOCK(mutex, id) \
  lll_robust_lock ((mutex)->__data.__lock, id, \
		   PTHREAD_ROBUST_MUTEX_PSHARED (mutex))
#endif

#ifndef atomic_exchange_acq
# define atomic_exchange_acq(mem, newvalue) \
  ({ __typeof (*(mem)) __atg5_oldval;					      \
     __typeof (mem) __atg5_memp = (mem);				      \
     __typeof (*(mem)) __atg5_value = (newvalue);			      \
									      \
     do									      \
       __atg5_oldval = *__atg5_memp;					      \
     while (__builtin_expect						      \
	    (atomic_compare_and_exchange_bool_acq (__atg5_memp, __atg5_value, \
						   __atg5_oldval), 0));	      \
									      \
     __atg5_oldval; })
#endif

#ifndef atomic_exchange_rel
# define atomic_exchange_rel(mem, newvalue) atomic_exchange_acq (mem, newvalue)
#endif

#define __lll_unlock(futex, private) \
  (void)							\
    ({ int *__futex = (futex);					\
       int __oldval = atomic_exchange_rel (__futex, 0);		\
       if (__builtin_expect (__oldval > 1, 0))			\
	 lll_futex_wake (__futex, 1, private);			\
    })
#define lll_unlock(futex, private) __lll_unlock(&(futex), private)

void __lll_lock_wait_private (int *futex)
{
  do
    {
      int oldval = atomic_compare_and_exchange_val_acq (futex, 2, 1);
      if (oldval != 0)
	lll_futex_wait (futex, 2, LLL_PRIVATE);
    }
  while (atomic_compare_and_exchange_bool_acq (futex, 2, 0) != 0);
}

/* lock api */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    lll_lock((mutex)->__data.__lock, 0);

    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    lll_unlock((mutex)->__data.__lock, 0);

    return 0;
}
