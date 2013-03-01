/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: George Schlossnagle <george@omniti.com>                     |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  |          Pierre Joye <pierre@php.net>                                |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_lock.h 311339 2011-05-22 17:18:49Z gopalv $ */

#ifndef APC_LOCK
#define APC_LOCK

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "apc.h"
#include "apc_sem.h"
#include "apc_fcntl.h"
#include "apc_pthreadmutex.h"
#include "apc_pthreadrwlock.h"
#include "apc_spin.h"
#include "apc_windows_srwlock_kernel.h"

/* {{{ generic locking macros */
#define CREATE_LOCK(lock)     apc_lck_create(NULL, 0, 1, lock)
#define DESTROY_LOCK(lock)    apc_lck_destroy(lock)
#define LOCK(lock)          { HANDLE_BLOCK_INTERRUPTIONS(); apc_lck_lock(lock); }
#define RDLOCK(lock)        { HANDLE_BLOCK_INTERRUPTIONS(); apc_lck_rdlock(lock); }
#define UNLOCK(lock)        { apc_lck_unlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
#define RDUNLOCK(lock)      { apc_lck_rdunlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
/* }}} */

/* atomic operations : rdlocks are impossible without these */
#if HAVE_ATOMIC_OPERATIONS
# ifdef PHP_WIN32
#  define ATOMIC_INC(a) InterlockedIncrement(&a)
#  define ATOMIC_DEC(a) InterlockedDecrement(&a)
# else
#  define ATOMIC_INC(a) __sync_add_and_fetch(&a, 1)
#  define ATOMIC_DEC(a) __sync_sub_and_fetch(&a, 1)
# endif
#endif

#if defined(APC_SEM_LOCKS)
# define APC_LOCK_TYPE "IPC Semaphore"
# define RDLOCK_AVAILABLE 0
# define NONBLOCKING_LOCK_AVAILABLE 1 
# define apc_lck_t int
# define apc_lck_create(a,b,c,d) d=apc_sem_create((b),(c) TSRMLS_CC)
# define apc_lck_destroy(a)    apc_sem_destroy(a)
# define apc_lck_lock(a)       apc_sem_lock(a TSRMLS_CC)
# define apc_lck_nb_lock(a)    apc_sem_nonblocking_lock(a TSRMLS_CC)
# define apc_lck_rdlock(a)     apc_sem_lock(a TSRMLS_CC)
# define apc_lck_unlock(a)     apc_sem_unlock(a TSRMLS_CC)
# define apc_lck_rdunlock(a)   apc_sem_unlock(a TSRMLS_CC)
#elif defined(APC_PTHREADMUTEX_LOCKS)
# define APC_LOCK_TYPE "pthread mutex Locks"
# define RDLOCK_AVAILABLE 0
# define NONBLOCKING_LOCK_AVAILABLE 1
# define apc_lck_t pthread_mutex_t 
# define apc_lck_create(a,b,c,d) apc_pthreadmutex_create((pthread_mutex_t*)&d TSRMLS_CC)
# define apc_lck_destroy(a)    apc_pthreadmutex_destroy(&a)
# define apc_lck_lock(a)       apc_pthreadmutex_lock(&a TSRMLS_CC)
# define apc_lck_nb_lock(a)    apc_pthreadmutex_nonblocking_lock(&a TSRMLS_CC)
# define apc_lck_rdlock(a)     apc_pthreadmutex_lock(&a TSRMLS_CC)
# define apc_lck_unlock(a)     apc_pthreadmutex_unlock(&a TSRMLS_CC)
# define apc_lck_rdunlock(a)   apc_pthreadmutex_unlock(&a TSRMLS_CC)
#elif defined(APC_PTHREADRW_LOCKS)
# define APC_LOCK_TYPE "pthread read/write Locks"
# define RDLOCK_AVAILABLE 1
# define NONBLOCKING_LOCK_AVAILABLE 1
# define apc_lck_t pthread_rwlock_t 
# define apc_lck_create(a,b,c,d) apc_pthreadrwlock_create((pthread_rwlock_t*)&d TSRMLS_CC)
# define apc_lck_destroy(a)    apc_pthreadrwlock_destroy(&a)
# define apc_lck_lock(a)       apc_pthreadrwlock_lock(&a TSRMLS_CC)
# define apc_lck_nb_lock(a)    apc_pthreadrwlock_nonblocking_lock(&a TSRMLS_CC)
# define apc_lck_rdlock(a)     apc_pthreadrwlock_rdlock(&a TSRMLS_CC)
# define apc_lck_unlock(a)     apc_pthreadrwlock_unlock(&a TSRMLS_CC)
# define apc_lck_rdunlock(a)   apc_pthreadrwlock_unlock(&a TSRMLS_CC)
#elif defined(APC_SPIN_LOCKS)
# define APC_LOCK_TYPE "spin Locks"
# define RDLOCK_AVAILABLE 0
# define NONBLOCKING_LOCK_AVAILABLE APC_SLOCK_NONBLOCKING_LOCK_AVAILABLE
# define apc_lck_t slock_t 
# define apc_lck_create(a,b,c,d) apc_slock_create((slock_t*)&(d))
# define apc_lck_destroy(a)    apc_slock_destroy(&a)
# define apc_lck_lock(a)       apc_slock_lock(&a TSRMLS_CC)
# define apc_lck_nb_lock(a)    apc_slock_nonblocking_lock(&a)
# define apc_lck_rdlock(a)     apc_slock_lock(&a TSRMLS_CC)
# define apc_lck_unlock(a)     apc_slock_unlock(&a)
# define apc_lck_rdunlock(a)   apc_slock_unlock(&a)
#elif defined(APC_SRWLOCK_NATIVE) && defined(PHP_WIN32)
# define APC_LOCK_TYPE "Windows Slim RWLOCK (native)"
# define RDLOCK_AVAILABLE 1
# define NONBLOCKING_LOCK_AVAILABLE 0
# define apc_lck_t SRWLOCK
# define apc_lck_create(a,b,c,d) InitializeSRWLock((SRWLOCK*)&(d))
# define apc_lck_destroy(a)
# define apc_lck_lock(a)       AcquireSRWLockExclusive(&a)
# define apc_lck_rdlock(a)     AcquireSRWLockShared(&a)
# define apc_lck_unlock(a)     ReleaseSRWLockExclusive(&a)
# define apc_lck_rdunlock(a)   ReleaseSRWLockShared(&a)
# if NONBLOCKING_LOCK_AVAILABLE==1 /* Only in win7/2008 */
#  define apc_lck_nb_lock(a)    (TryAcquireSRWLockExclusive(&a TSRMLS_CC) == 0 ? 1 : 0);
# endif
#elif defined(APC_SRWLOCK_KERNEL) && defined(PHP_WIN32)
# define APC_LOCK_TYPE "Windows Slim RWLOCK (kernel)"
# define RDLOCK_AVAILABLE 1
# define NONBLOCKING_LOCK_AVAILABLE 0
# define apc_lck_t apc_windows_cs_rwlock_t
# define apc_lck_create(a,b,c,d) apc_windows_cs_create((apc_windows_cs_rwlock_t*)&(d) TSRMLS_CC)
# define apc_lck_destroy(a)      apc_windows_cs_destroy(&a);
# define apc_lck_lock(a)         apc_windows_cs_lock(&a TSRMLS_CC)
# define apc_lck_rdlock(a)       apc_windows_cs_rdlock(&a TSRMLS_CC)
# define apc_lck_unlock(a)       apc_windows_cs_unlock_wr(&a TSRMLS_CC)
# define apc_lck_rdunlock(a)     apc_windows_cs_unlock_rd(&a TSRMLS_CC)
#else
# define APC_LOCK_TYPE "File Locks"
# ifdef HAVE_ATOMIC_OPERATIONS
#  define RDLOCK_AVAILABLE 1
# endif
# ifdef PHP_WIN32
#  define NONBLOCKING_LOCK_AVAILABLE 0
# else
#  define NONBLOCKING_LOCK_AVAILABLE 1
# endif
# define apc_lck_t int
# define apc_lck_create(a,b,c,d) d=apc_fcntl_create((a) TSRMLS_CC)
# define apc_lck_destroy(a)    apc_fcntl_destroy(a)
# define apc_lck_lock(a)       apc_fcntl_lock(a TSRMLS_CC)
# define apc_lck_nb_lock(a)    apc_fcntl_nonblocking_lock(a TSRMLS_CC)
# define apc_lck_rdlock(a)     apc_fcntl_rdlock(a TSRMLS_CC)
# define apc_lck_unlock(a)     apc_fcntl_unlock(a TSRMLS_CC)
# define apc_lck_rdunlock(a)   apc_fcntl_unlock(a TSRMLS_CC)
#endif

#endif
