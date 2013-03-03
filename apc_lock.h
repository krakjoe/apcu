/*
  +----------------------------------------------------------------------+
  | APCu                                                                 |
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
  | Author: Joe Watkins <joe.watkins@live.co.uk>                        |
  +----------------------------------------------------------------------+
 */

#ifndef APC_LOCK
#define APC_LOCK

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "apc.h"

/* Generic implementation of read/write locks for APC */
#ifndef _WIN32
#include "pthread.h"
typedef struct _apc_lock_t {
	pthread_mutex_t read;
	pthread_mutex_t write;
} apc_lock_t;
#else

#endif

int apc_lock_create(apc_lock_t *lock TSRMLS_DC);
int apc_lock_rlock(apc_lock_t *lock TSRMLS_DC);
int apc_lock_wlock(apc_lock_t *lock TSRMLS_DC);
int apc_lock_runlock(apc_lock_t *lock TSRMLS_DC);
int apc_lock_wunlock(apc_lock_t *lock TSRMLS_DC);
void apc_lock_destroy(apc_lock_t *lock TSRMLS_DC);

void apc_lock_test(TSRMLS_DC);

/* {{{ generic locking macros */
#define CREATE_LOCK(lock)     apc_lock_create(lock TSRMLS_CC)
#define DESTROY_LOCK(lock)    apc_lock_destroy(lock TSRMLS_CC)
#define WLOCK(lock)           { HANDLE_BLOCK_INTERRUPTIONS(); apc_lock_wlock(lock); }
#define LOCK                  WLOCK
#define RLOCK(lock)           { HANDLE_BLOCK_INTERRUPTIONS(); apc_lock_rlock(lock); }
#define WUNLOCK(lock)         { apc_lock_wunlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
#define UNLOCK                WUNLOCK
#define RUNLOCK(lock)         { apc_lock_runlock(lock); HANDLE_UNBLOCK_INTERRUPTIONS(); }
/* }}} */

#endif
