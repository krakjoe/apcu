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
#ifndef HAVE_APC_LOCK
#define HAVE_APC_LOCK

#ifndef HAVE_APC_LOCK_H
# include "apc_lock.h"
#endif

/*
 APCu never checks the return value of a locking call, it assumes it should not fail
 and execution continues regardless, therefore it is pointless to check the return
 values of calls to locking functions, lets save ourselves the comparison.
*/

/* {{{ There's very little point in initializing a billion sets of attributes */
#ifndef PHP_WIN32
# ifndef APC_NATIVE_RWLOCK
	static pthread_mutexattr_t apc_lock_attr;
# else
	static pthread_rwlockattr_t apc_lock_attr;
# endif
static zend_bool apc_lock_ready = 0;
#endif /* }}} */

/* {{{ Initialize the global lock attributes */
PHP_APCU_API zend_bool apc_lock_init(TSRMLS_D) {
#ifndef PHP_WIN32
	if (apc_lock_ready)
		return 1;

	/* once per process please */
	apc_lock_ready = 1;

# ifndef APC_NATIVE_RWLOCK
	if (pthread_mutexattr_init(&apc_lock_attr) == SUCCESS) {
		if (pthread_mutexattr_setpshared(&apc_lock_attr, PTHREAD_PROCESS_SHARED) == SUCCESS) {
			pthread_mutexattr_settype(&apc_lock_attr, PTHREAD_MUTEX_RECURSIVE);
			return 1;
		}
	}
# else
	if (pthread_rwlockattr_init(&apc_lock_attr) == SUCCESS) {
		if (pthread_rwlockattr_setpshared(&apc_lock_attr, PTHREAD_PROCESS_SHARED) == SUCCESS) {
			return 1;
		}
	}
# endif
	return 0;
#else
	return 1;
#endif
} /* }}} */

/* {{{ Cleanup attributes and statics */
PHP_APCU_API void apc_lock_cleanup(TSRMLS_D) {
#ifndef PHP_WIN32
	if (!apc_lock_ready)
		return;

	/* once per process please */
	apc_lock_ready = 0;

# ifndef APC_NATIVE_RWLOCK
	pthread_mutexattr_destroy(&apc_lock_attr);
# else
	pthread_rwlockattr_destroy(&apc_lock_attr);
# endif
#endif
} /* }}} */

PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock TSRMLS_DC) {
#ifndef PHP_WIN32
# ifndef APC_NATIVE_RWLOCK
	{
		pthread_mutex_init(lock, &apc_lock_attr);
		return 1;
	}
# else
	{
		/* Native */
		return (pthread_rwlock_init(lock, &apc_lock_attr)==SUCCESS);
	}
# endif
#else
	lock = (apc_lock_t *)apc_windows_cs_create((apc_windows_cs_rwlock_t *)lock TSRMLS_CC);

	return (NULL != lock);
#endif
}

PHP_APCU_API zend_bool apc_lock_rlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef PHP_WIN32
# ifndef APC_NATIVE_RWLOCK
	pthread_mutex_lock(lock);
# else
	pthread_rwlock_rdlock(lock);
# endif
#else
	apc_windows_cs_rdlock((apc_windows_cs_rwlock_t *)lock TSRMLS_CC);
#endif
	return 1;
}

PHP_APCU_API zend_bool apc_lock_wlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef PHP_WIN32
# ifndef APC_NATIVE_RWLOCK
	pthread_mutex_lock(lock);
# else	
	pthread_rwlock_wrlock(lock);
# endif
#else
	apc_windows_cs_lock((apc_windows_cs_rwlock_t *)lock TSRMLS_CC);
#endif
	return 1;
}

PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef PHP_WIN32
# ifndef APC_NATIVE_RWLOCK
	pthread_mutex_unlock(lock);
# else
	pthread_rwlock_unlock(lock);
# endif
#else
	apc_windows_cs_unlock_wr((apc_windows_cs_rwlock_t *)lock TSRMLS_CC);
#endif
	return 1;
}

PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef PHP_WIN32
# ifndef APC_NATIVE_RWLOCK
	pthread_mutex_unlock(lock);
# else
	pthread_rwlock_unlock(lock);
# endif
#else
	apc_windows_cs_unlock_rd((apc_windows_cs_rwlock_t *)lock TSRMLS_CC);
#endif
	return 1;
}

PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock TSRMLS_DC) {
#ifndef PHP_WIN32
# ifndef APC_NATIVE_RWLOCK
	pthread_mutex_destroy(lock);
# else
	pthread_rwlock_destroy(lock);
# endif
#else
	apc_windows_cs_destroy((apc_windows_cs_rwlock_t *)lock);
#endif
} 
#endif

