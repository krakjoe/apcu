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
  | Authors: Brian Shire <shire@php.net>                                 |
  +----------------------------------------------------------------------+

 */

/* $Id: apc_pthreadmutex.c 307048 2011-01-03 23:53:17Z kalle $ */

#include "apc_pthreadmutex.h"

#ifdef APC_PTHREADMUTEX_LOCKS

pthread_mutex_t *apc_pthreadmutex_create(pthread_mutex_t *lock TSRMLS_DC) 
{
    int result;
    pthread_mutexattr_t* attr;
    attr = malloc(sizeof(pthread_mutexattr_t));

    result = pthread_mutexattr_init(attr);
    if(result == ENOMEM) {
        apc_error("pthread mutex error: Insufficient memory exists to create the mutex attribute object." TSRMLS_CC);
    } else if(result == EINVAL) {
        apc_error("pthread mutex error: attr does not point to writeable memory." TSRMLS_CC);
    } else if(result == EFAULT) {
        apc_error("pthread mutex error: attr is an invalid pointer." TSRMLS_CC);
    }

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
       result = pthread_mutexattr_settype(attr, PTHREAD_MUTEX_ADAPTIVE_NP);
       if (result == EINVAL) {
               apc_error("pthread_mutexattr_settype: unable to set adaptive mutexes" TSRMLS_CC);
       }
#endif

    /* pthread_mutexattr_settype(attr, PTHREAD_MUTEX_ERRORCHECK); */
    result = pthread_mutexattr_setpshared(attr, PTHREAD_PROCESS_SHARED);
    if(result == EINVAL) {
        apc_error("pthread mutex error: attr is not an initialized mutex attribute object, or pshared is not a valid process-shared state setting." TSRMLS_CC);
    } else if(result == EFAULT) {
        apc_error("pthread mutex error: attr is an invalid pointer." TSRMLS_CC);
    } else if(result == ENOTSUP) {
        apc_error("pthread mutex error: pshared was set to PTHREAD_PROCESS_SHARED." TSRMLS_CC);
    }

    if(pthread_mutex_init(lock, attr)) { 
        apc_error("unable to initialize pthread lock" TSRMLS_CC);
    }
    return lock;
}

void apc_pthreadmutex_destroy(pthread_mutex_t *lock)
{
    return; /* we don't actually destroy the mutex, as it would destroy it for all processes */
}

void apc_pthreadmutex_lock(pthread_mutex_t *lock TSRMLS_DC)
{
    int result;
    result = pthread_mutex_lock(lock);
    if(result == EINVAL) {
        apc_error("unable to obtain pthread lock (EINVAL)" TSRMLS_CC);
    } else if(result == EDEADLK) {
        apc_error("unable to obtain pthread lock (EDEADLK)" TSRMLS_CC);
    }
}

void apc_pthreadmutex_unlock(pthread_mutex_t *lock TSRMLS_DC)
{
    if(pthread_mutex_unlock(lock)) {
        apc_error("unable to unlock pthread lock" TSRMLS_CC);
    }
}

zend_bool apc_pthreadmutex_nonblocking_lock(pthread_mutex_t *lock TSRMLS_DC)
{
    int rval;
    rval = pthread_mutex_trylock(lock);
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
