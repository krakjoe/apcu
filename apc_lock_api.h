/*
  +----------------------------------------------------------------------+
  | APCu                                                                 |
  +----------------------------------------------------------------------+
  | Copyright (c) 2013 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Joe Watkins <joe.watkins@live.co.uk>                         |
  +----------------------------------------------------------------------+
 */

#ifndef APC_LOCK_API_H
#define APC_LOCK_API_H

/*
 APCu works most efficiently where there is access to native read/write locks
 If the current system has native rwlocks present they will be used, if they are
	not present, APCu will emulate their behavior with standard mutex.
 While APCu is emulating read/write locks, reads and writes are exclusive,
	additionally the write lock prefers readers, as is the default behaviour of
	the majority of Posix rwlock implementations
*/

#ifndef PHP_WIN32
# ifndef __USE_UNIX98
#  define __USE_UNIX98
# endif
# include "pthread.h"
# ifndef APC_SPIN_LOCK
#   ifndef APC_FCNTL_LOCK
#       if defined(APC_NATIVE_RWLOCK) && defined(HAVE_ATOMIC_OPERATIONS)
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

PHP_APCU_API int apc_lock_init(apc_lock_t* lock);
PHP_APCU_API int apc_lock_try(apc_lock_t* lock);
PHP_APCU_API int apc_lock_get(apc_lock_t* lock);
PHP_APCU_API int apc_lock_release(apc_lock_t* lock);
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
#define WLOCK(lock)           { HANDLE_BLOCK_INTERRUPTIONS(); apc_lock_wlock(lock); }
#define WUNLOCK(lock)         { apc_lock_wunlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
#define RLOCK(lock)           { HANDLE_BLOCK_INTERRUPTIONS(); apc_lock_rlock(lock); }
#define RUNLOCK(lock)         { apc_lock_runlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
#define LOCK                  WLOCK
#define UNLOCK                WUNLOCK
/* }}} */

/* {{{ object locking macros */
#define APC_WLOCK(o)          WLOCK(&(o)->lock)
#define APC_LOCK              APC_WLOCK
#define APC_WUNLOCK(o)        WUNLOCK(&(o)->lock)
#define APC_UNLOCK            APC_WUNLOCK
#define APC_RLOCK(o)          RLOCK(&(o)->lock)
#define APC_RUNLOCK(o)        RUNLOCK(&(o)->lock) /* }}} */

/* atomic operations */
#if defined(APC_LOCK_SHARED)
# ifdef PHP_WIN32
#  define ATOMIC_INC(c, a) InterlockedIncrement(&a)
#  define ATOMIC_DEC(c, a) InterlockedDecrement(&a)
# else
#  define ATOMIC_INC(c, a) __sync_add_and_fetch(&a, 1)
#  define ATOMIC_DEC(c, a) __sync_sub_and_fetch(&a, 1)
# endif
#elif defined(APC_LOCK_RECURSIVE)
# define ATOMIC_INC(c, a) do { \
	if (apc_lock_wlock(&(c)->header->lock)) { \
		(a)++; \
		apc_lock_wunlock(&(c)->header->lock); \
	} \
} while(0)
# define ATOMIC_DEC(c, a) do { \
	if (apc_lock_wlock(&(c)->header->lock)) { \
		(a)--; \
		apc_lock_wunlock(&(c)->header->lock); \
	} \
} while(0)
#else
# define ATOMIC_INC(c, a) (a)++
# define ATOMIC_DEC(c, a) (a)--
#endif

#endif
