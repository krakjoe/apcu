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
  | Authors: Gopal V <gopalv@php.net>                                    |
  +----------------------------------------------------------------------+

 */

/* $Id: $ */

#include "apc_pthreadrwlock.h"

#ifdef APC_PTHREADRW_LOCKS

pthread_rwlock_t *apc_pthreadrwlock_create(pthread_rwlock_t *lock TSRMLS_DC) 
{
    int result;
    pthread_rwlockattr_t attr;

    result = pthread_rwlockattr_init(&attr);
    if(result == ENOMEM) {
        apc_error("pthread rwlock error: Insufficient memory exists to create the rwlock attribute object." TSRMLS_CC);
    } else if(result == EINVAL) {
        apc_error("pthread rwlock error: attr does not point to writeable memory." TSRMLS_CC);
    } else if(result == EFAULT) {
        apc_error("pthread rwlock error: attr is an invalid pointer." TSRMLS_CC);
    }

#ifdef	__USE_UNIX98
	pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

    result = pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if(result == EINVAL) {
        apc_error("pthread rwlock error: attr is not an initialized rwlock attribute object, or pshared is not a valid process-shared state setting." TSRMLS_CC);
    } else if(result == EFAULT) {
        apc_error("pthread rwlock error: attr is an invalid pointer." TSRMLS_CC);
    } else if(result == ENOTSUP) {
        apc_error("pthread rwlock error: pshared was set to PTHREAD_PROCESS_SHARED." TSRMLS_CC);
    }

    if(pthread_rwlock_init(lock, &attr)) { 
        apc_error("unable to initialize pthread rwlock" TSRMLS_CC);
    }

    pthread_rwlockattr_destroy(&attr);

    return lock;
}

void apc_pthreadrwlock_destroy(pthread_rwlock_t *lock)
{
    return; /* we don't actually destroy the rwlock, as it would destroy it for all processes */
}

void apc_pthreadrwlock_lock(pthread_rwlock_t *lock TSRMLS_DC)
{
    int result;
    result = pthread_rwlock_wrlock(lock);
    if(result == EINVAL) {
        apc_error("unable to obtain pthread lock (EINVAL)" TSRMLS_CC);
    } else if(result == EDEADLK) {
        apc_error("unable to obtain pthread lock (EDEADLK)" TSRMLS_CC);
    }
}

void apc_pthreadrwlock_rdlock(pthread_rwlock_t *lock TSRMLS_DC)
{
    int result;
    result = pthread_rwlock_rdlock(lock);
    if(result == EINVAL) {
        apc_error("unable to obtain pthread lock (EINVAL)" TSRMLS_CC);
    } else if(result == EDEADLK) {
        apc_error("unable to obtain pthread lock (EDEADLK)" TSRMLS_CC);
    }
}

void apc_pthreadrwlock_unlock(pthread_rwlock_t *lock TSRMLS_DC)
{
    if(pthread_rwlock_unlock(lock)) {
        apc_error("unable to unlock pthread lock" TSRMLS_CC);
    }
}

zend_bool apc_pthreadrwlock_nonblocking_lock(pthread_rwlock_t *lock TSRMLS_DC)
{
    int rval;
    rval = pthread_rwlock_trywrlock(lock);
    if(rval == EBUSY) {     /* Lock is already held */
        return 0;
    } else if(rval == 0) {  /* Obtained lock */
        return 1;
    } else {                /* Other error */
        apc_error("unable to obtain pthread trylock" TSRMLS_CC);
        return 0;
    }
}


#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
