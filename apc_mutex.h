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

#ifndef APC_MUTEX_H
#define APC_MUTEX_H

#include "apc.h"

#ifdef APC_HAS_PTHREAD_MUTEX

#include "pthread.h"

typedef pthread_mutex_t apc_mutex_t;

PHP_APCU_API zend_bool apc_mutex_init();
PHP_APCU_API void apc_mutex_cleanup();
PHP_APCU_API zend_bool apc_mutex_create(apc_mutex_t *lock);
PHP_APCU_API zend_bool apc_mutex_lock(apc_mutex_t *lock);
PHP_APCU_API zend_bool apc_mutex_unlock(apc_mutex_t *lock);
PHP_APCU_API void apc_mutex_destroy(apc_mutex_t *lock);

#define APC_MUTEX_INIT()          apc_mutex_init()
#define APC_MUTEX_CLEANUP()       apc_mutex_cleanup()

#define APC_CREATE_MUTEX(lock)    apc_mutex_create(lock)
#define APC_DESTROY_MUTEX(lock)   apc_mutex_destroy(lock)
#define APC_MUTEX_LOCK(lock)      apc_mutex_lock(lock)
#define APC_MUTEX_UNLOCK(lock)    apc_mutex_unlock(lock)

#else

#include "apc_lock.h"

typedef apc_lock_t apc_mutex_t;

// Fallback to normal locks

#define APC_MUTEX_INIT()          
#define APC_MUTEX_CLEANUP()       

#define APC_CREATE_MUTEX(lock)    CREATE_LOCK(lock)
#define APC_DESTROY_MUTEX(lock)   DESTROY_LOCK(lock)
#define APC_MUTEX_LOCK(lock)      WLOCK(lock)
#define APC_MUTEX_UNLOCK(lock)    WUNLOCK(lock)

#endif

#endif
