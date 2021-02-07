/*
  +----------------------------------------------------------------------+
  | APCu                                                                 |
  +----------------------------------------------------------------------+
  | Copyright (c) 2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Joe Watkins <joe.watkins@live.co.uk>                        |
  +----------------------------------------------------------------------+
 */

#ifndef APC_LOCK_H
#define APC_LOCK_H

/*
 APCu works most efficiently where there is access to native read/write locks
 If the current system has native rwlocks present they will be used, if they are
	not present, APCu will emulate their behavior with standard mutex.
 While APCu is emulating read/write locks, reads and writes are exclusive,
	additionally the write lock prefers readers, as is the default behaviour of
	the majority of Posix rwlock implementations
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "apc.h"

#ifndef PHP_WIN32
# ifndef __USE_UNIX98
#  define __USE_UNIX98
# endif
# include "pthread.h"
# ifndef APC_SPIN_LOCK
#   ifndef APC_FCNTL_LOCK
#       ifdef APC_NATIVE_RWLOCK
		typedef pthread_rwlock_t apc_lock_t;
#		define APC_LOCK_SHARED
#       else
		typedef pthread_mutex_t apc_lock_t;
#		define APC_LOCK_RECURSIVE
#       endif
#   else
		typedef int apc_lock_t;
#		define APC_LOCK_FILE
#   endif
# else
# define APC_LOCK_NICE 1
typedef struct {
	unsigned long state;
} apc_lock_t;
# endif
#else
/* XXX kernel lock mode only for now, compatible through all the wins, add more ifdefs for others */
# include "apc_windows_srwlock_kernel.h"
typedef apc_windows_cs_rwlock_t apc_lock_t;
# define APC_LOCK_SHARED
#endif

/* {{{ functions */
/*
  The following functions should be called once per process:
	apc_lock_init initializes attributes suitable for all locks
	apc_lock_cleanup destroys those attributes
  This saves us from having to create and destroy attributes for
  every lock we use at runtime */
PHP_APCU_API zend_bool apc_lock_init();
PHP_APCU_API void      apc_lock_cleanup();
/*
  The following functions should be self explanitory:
*/
PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock);
PHP_APCU_API zend_bool apc_lock_rlock(apc_lock_t *lock);
PHP_APCU_API zend_bool apc_lock_wlock(apc_lock_t *lock);
PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock);
PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock);
PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock); /* }}} */

/* {{{ generic locking macros */
#define CREATE_LOCK(lock)     apc_lock_create(lock)
#define DESTROY_LOCK(lock)    apc_lock_destroy(lock)
#define WLOCK(lock)           apc_lock_wlock(lock)
#define WUNLOCK(lock)         { apc_lock_wunlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
#define RLOCK(lock)           apc_lock_rlock(lock)
#define RUNLOCK(lock)         { apc_lock_runlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
/* }}} */

/* atomic operations */
#ifdef PHP_WIN32
# ifdef _WIN64
#  define ATOMIC_INC(a) InterlockedIncrement64(&a)
#  define ATOMIC_DEC(a) InterlockedDecrement64(&a)
#  define ATOMIC_ADD(a, b) (InterlockedExchangeAdd64(&a, b) + b)
#  define ATOMIC_CAS(a, old, new) (InterlockedCompareExchange64(&a, new, old) == old)
# else
#  define ATOMIC_INC(a) InterlockedIncrement(&a)
#  define ATOMIC_DEC(a) InterlockedDecrement(&a)
#  define ATOMIC_ADD(a, b) (InterlockedExchangeAdd(&a, b) + b)
#  define ATOMIC_CAS(a, old, new) (InterlockedCompareExchange(&a, new, old) == old)
# endif
#else
# define ATOMIC_INC(a) __sync_add_and_fetch(&a, 1)
# define ATOMIC_DEC(a) __sync_sub_and_fetch(&a, 1)
# define ATOMIC_ADD(a, b) __sync_add_and_fetch(&a, b)
# define ATOMIC_CAS(a, old, new) __sync_bool_compare_and_swap(&a, old, new)
#endif

#endif
