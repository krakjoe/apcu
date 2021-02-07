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

#ifndef HAVE_APC_LOCK_H
# include "apc_lock.h"
#endif

/*
 * While locking calls should never fail, apcu checks for the success of write-lock
 * acquisitions, to prevent more damage when a deadlock is detected.
 */

#ifdef PHP_WIN32
PHP_APCU_API zend_bool apc_lock_init() {
	return 1;
}

PHP_APCU_API void apc_lock_cleanup() {
}

PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock) {
	return NULL != apc_windows_cs_create(lock);
}

static inline zend_bool apc_lock_rlock_impl(apc_lock_t *lock) {
	apc_windows_cs_rdlock(lock);
	return 1;
}

static inline zend_bool apc_lock_wlock_impl(apc_lock_t *lock) {
	apc_windows_cs_lock(lock);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock) {
	apc_windows_cs_unlock_wr(lock);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock) {
	apc_windows_cs_unlock_rd(lock);
	return 1;
}

PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock) {
	apc_windows_cs_destroy(lock);
}

#elif defined(APC_NATIVE_RWLOCK)

static zend_bool apc_lock_ready = 0;
static pthread_rwlockattr_t apc_lock_attr;

PHP_APCU_API zend_bool apc_lock_init() {
	if (apc_lock_ready) {
		return 1;
	}
	apc_lock_ready = 1;

	if (pthread_rwlockattr_init(&apc_lock_attr) != SUCCESS) {
		return 0;
	}
	if (pthread_rwlockattr_setpshared(&apc_lock_attr, PTHREAD_PROCESS_SHARED) != SUCCESS) {
		return 0;
	}
	return 1;
}

PHP_APCU_API void apc_lock_cleanup() {
	if (!apc_lock_ready) {
		return;
	}
	apc_lock_ready = 0;

	pthread_rwlockattr_destroy(&apc_lock_attr);
}

PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock) {
	return pthread_rwlock_init(lock, &apc_lock_attr) == SUCCESS;
}

static inline zend_bool apc_lock_rlock_impl(apc_lock_t *lock) {
	return pthread_rwlock_rdlock(lock) == 0;
}

static inline zend_bool apc_lock_wlock_impl(apc_lock_t *lock) {
	return pthread_rwlock_wrlock(lock) == 0;
}

PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock) {
	pthread_rwlock_unlock(lock);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock) {
	pthread_rwlock_unlock(lock);
	return 1;
}

PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock) {
	pthread_rwlock_destroy(lock);
}

#elif defined(APC_LOCK_RECURSIVE)

static zend_bool apc_lock_ready = 0;
static pthread_mutexattr_t apc_lock_attr;

PHP_APCU_API zend_bool apc_lock_init() {
	if (apc_lock_ready) {
		return 1;
	}
	apc_lock_ready = 1;

	if (pthread_mutexattr_init(&apc_lock_attr) != SUCCESS) {
		return 0;
	}

	if (pthread_mutexattr_setpshared(&apc_lock_attr, PTHREAD_PROCESS_SHARED) != SUCCESS) {
		return 0;
	}

	pthread_mutexattr_settype(&apc_lock_attr, PTHREAD_MUTEX_RECURSIVE);
	return 1;
}

PHP_APCU_API void apc_lock_cleanup() {
	if (!apc_lock_ready) {
		return;
	}
	apc_lock_ready = 0;

	pthread_mutexattr_destroy(&apc_lock_attr);
}

PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock) {
	pthread_mutex_init(lock, &apc_lock_attr);
	return 1;
}

static inline zend_bool apc_lock_rlock_impl(apc_lock_t *lock) {
	return pthread_mutex_lock(lock) == 0;
}

static inline zend_bool apc_lock_wlock_impl(apc_lock_t *lock) {
	return pthread_mutex_lock(lock) == 0;
}

PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock) {
	pthread_mutex_unlock(lock);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock) {
	pthread_mutex_unlock(lock);
	return 1;
}

PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock) {
	pthread_mutex_destroy(lock);
}

#elif defined(APC_SPIN_LOCK)

static int apc_lock_try(apc_lock_t *lock) {
	int failed = 1;

	asm volatile
	(
		"xchgl %0, 0(%1)" :
		"=r" (failed) : "r" (&lock->state),
		"0" (failed)
	);

	return failed;
}

static int apc_lock_get(apc_lock_t *lock) {
	int failed = 1;

	do {
		failed = apc_lock_try(lock);
#ifdef APC_LOCK_NICE
		usleep(0);
#endif
	} while (failed);

	return failed;
}

static int apc_lock_release(apc_lock_t *lock) {
	int released = 0;

	asm volatile (
		"xchg %0, 0(%1)" : "=r" (released) : "r" (&lock->state),
		"0" (released)
	);

	return !released;
}

PHP_APCU_API zend_bool apc_lock_init() {
	return 0;
}

PHP_APCU_API void apc_lock_cleanup() {
}

PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock) {
	lock->state = 0;
}

static inline zend_bool apc_lock_rlock_impl(apc_lock_t *lock) {
	apc_lock_get(lock);
	return 1;
}

static inline zend_bool apc_lock_wlock_impl(apc_lock_t *lock) {
	apc_lock_get(lock);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock) {
	apc_lock_release(lock);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock) {
	apc_lock_release(lock);
	return 1;
}

PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock) {
}

#else

#include <unistd.h>
#include <fcntl.h>

static int apc_fcntl_call(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
	int ret;
	struct flock lock;

	lock.l_type = type;
	lock.l_start = offset;
	lock.l_whence = whence;
	lock.l_len = len;
	lock.l_pid = 0;

	do {
		ret = fcntl(fd, cmd, &lock) ;
	} while(ret < 0 && errno == EINTR);

	return(ret);
}

PHP_APCU_API zend_bool apc_lock_init() {
	return 0;
}

PHP_APCU_API void apc_lock_cleanup() {
}

PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock) {
	char lock_path[] = "/tmp/.apc.XXXXXX";

	*lock = mkstemp(lock_path);
	if (*lock > 0) {
		unlink(lock_path);
		return 1;
	} else {
		return 0;
	}
}

static inline zend_bool apc_lock_rlock_impl(apc_lock_t *lock) {
	apc_fcntl_call((*lock), F_SETLKW, F_RDLCK, 0, SEEK_SET, 0);
	return 1;
}

static inline zend_bool apc_lock_wlock_impl(apc_lock_t *lock) {
	apc_fcntl_call((*lock), F_SETLKW, F_WRLCK, 0, SEEK_SET, 0);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock) {
	apc_fcntl_call((*lock), F_SETLKW, F_UNLCK, 0, SEEK_SET, 0);
	return 1;
}

PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock) {
	apc_fcntl_call((*lock), F_SETLKW, F_UNLCK, 0, SEEK_SET, 0);
	return 1;
}

PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock) {
	close(*lock);
}

#endif

/* Shared for all lock implementations */

PHP_APCU_API zend_bool apc_lock_wlock(apc_lock_t *lock) {
	HANDLE_BLOCK_INTERRUPTIONS();
	if (apc_lock_wlock_impl(lock)) {
		return 1;
	}

	HANDLE_UNBLOCK_INTERRUPTIONS();
	apc_warning("Failed to acquire write lock");
	return 0;
}

PHP_APCU_API zend_bool apc_lock_rlock(apc_lock_t *lock) {
	HANDLE_BLOCK_INTERRUPTIONS();
	if (apc_lock_rlock_impl(lock)) {
		return 1;
	}

	HANDLE_UNBLOCK_INTERRUPTIONS();
	apc_warning("Failed to acquire read lock");
	return 0;
}
