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
  | Author: Fabian Franz <fabian@lionsad.de>                             |
  +----------------------------------------------------------------------+
 */
#include "apc_mutex.h"

#ifdef APC_HAS_PTHREAD_MUTEX

static zend_bool apc_mutex_ready = 0;
static pthread_mutexattr_t apc_mutex_attr;

PHP_APCU_API zend_bool apc_mutex_init() {
	if (apc_mutex_ready) {
		return 1;
	}
	apc_mutex_ready = 1;

	if (pthread_mutexattr_init(&apc_mutex_attr) != SUCCESS) {
		return 0;
	}

	if (pthread_mutexattr_setpshared(&apc_mutex_attr, PTHREAD_PROCESS_SHARED) != SUCCESS) {
		return 0;
	}

	return 1;
}

PHP_APCU_API void apc_mutex_cleanup() {
	if (!apc_mutex_ready) {
		return;
	}
	apc_mutex_ready = 0;

	pthread_mutexattr_destroy(&apc_mutex_attr);
}

PHP_APCU_API zend_bool apc_mutex_create(apc_mutex_t *lock) {
	pthread_mutex_init(lock, &apc_mutex_attr);
	return 1;
}

PHP_APCU_API zend_bool apc_mutex_lock(apc_mutex_t *lock) {
	HANDLE_BLOCK_INTERRUPTIONS();
	if (pthread_mutex_lock(lock) == 0) {
		return 1;
	}

	HANDLE_UNBLOCK_INTERRUPTIONS();
	apc_warning("Failed to acquire lock");
	return 0;
}

PHP_APCU_API zend_bool apc_mutex_unlock(apc_mutex_t *lock) {
	pthread_mutex_unlock(lock);
	HANDLE_UNBLOCK_INTERRUPTIONS();
	return 1;
}

PHP_APCU_API void apc_mutex_destroy(apc_mutex_t *lock) {
	pthread_mutex_destroy(lock);
}

#endif
