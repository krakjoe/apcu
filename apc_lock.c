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

/* {{{ There's very little point in initializing a billion sets of attributes */
#ifndef _WIN32
# ifndef APC_NATIVE_RWLOCK
	static pthread_mutexattr_t apc_lock_attr;
# else
	static pthread_rwlockattr_t apc_lock_attr;
# endif
static zend_bool apc_lock_ready = 0;
#endif /* }}} */

/* {{{ Initialize the global lock attributes */
zend_bool apc_lock_init(TSRMLS_D) {
#ifndef _WIN32
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
			return (apc_lock_ready=1);
		}
	}
# endif
#else
	{
		/* windoze */
	}
#endif
	return 0;
} /* }}} */

/* {{{ Cleanup attributes and statics */
void apc_lock_cleanup(TSRMLS_D) {
#ifndef _WIN32
	if (!apc_lock_ready)
		return;
	/* once per process please */
	apc_lock_ready = 1;

# ifndef APC_NATIVE_RWLOCK
	pthread_mutexattr_destroy(&apc_lock_attr);
# else
	pthread_rwlockattr_destroy(&apc_lock_attr);
# endif
#else
	{
		/* windoze */
	}
#endif
} /* }}} */

zend_bool apc_lock_create(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
# ifndef APC_NATIVE_RWLOCK
	{
		/* Emulated */
		pthread_mutex_init(&lock->read, &apc_lock_attr);
		pthread_mutex_init(&lock->write, &apc_lock_attr);

		return 1;
	}
# else
	{
		/* Native */
		return (pthread_rwlock_init(lock, &apc_lock_attr)==SUCCESS);
	}
# endif
#else
	{
		/* windoze */
	}
#endif

	return 0;
}

zend_bool apc_lock_rlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
# ifndef APC_NATIVE_RWLOCK
	return (pthread_mutex_lock(&lock->read)==SUCCESS);
# else
	return (pthread_rwlock_rdlock(lock)==SUCCESS);
# endif
#else
	{
		/* windoze */
	}	
#endif

	return 0;
}

zend_bool apc_lock_wlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
# ifndef APC_NATIVE_RWLOCK
	return ((pthread_mutex_lock(&lock->read)==SUCCESS) && (pthread_mutex_lock(&lock->write)==SUCCESS));
# else
	return (pthread_rwlock_wrlock(lock)==SUCCESS);
# endif
#else
	{
		/* windoze */
	}
#endif
	
	return 0;
}

zend_bool apc_lock_wunlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
# ifndef APC_NATIVE_RWLOCK
	return ((pthread_mutex_unlock(&lock->read)==SUCCESS) && (pthread_mutex_unlock(&lock->write)==SUCCESS));
# else
	return (pthread_rwlock_unlock(lock)==SUCCESS);
# endif
#else
	{
		/* windoze */
	}
#endif

	return 0;
}

zend_bool apc_lock_runlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
# ifndef APC_NATIVE_RWLOCK
	return (pthread_mutex_unlock(&lock->read)==SUCCESS);
# else
	return (pthread_rwlock_unlock(lock)==SUCCESS);
# endif
#else
	{
		/* windoze */
	}
#endif

	return 0;
}

void apc_lock_destroy(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
# ifndef APC_NATIVE_RWLOCK
	pthread_mutex_destroy(lock->read);
	pthread_mutex_destroy(lock->write);
# else
	pthread_rwlock_destroy(lock);
# endif
#else
	
#endif
} 
#endif

