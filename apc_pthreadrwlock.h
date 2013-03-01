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

/* $Id: apc_pthreadrwlock.h 302175 2010-08-13 06:20:28Z kalle $ */

#ifndef APC_PTHREADRWLOCK_H
#define APC_PTHREADRWLOCK_H

#include "apc.h"

#ifdef APC_PTHREADRW_LOCKS

#include <pthread.h>

pthread_rwlock_t *apc_pthreadrwlock_create(pthread_rwlock_t *lock TSRMLS_DC);
void apc_pthreadrwlock_destroy(pthread_rwlock_t *lock);
void apc_pthreadrwlock_lock(pthread_rwlock_t *lock TSRMLS_DC);
void apc_pthreadrwlock_rdlock(pthread_rwlock_t *lock TSRMLS_DC);
void apc_pthreadrwlock_unlock(pthread_rwlock_t *lock TSRMLS_DC);
zend_bool apc_pthreadrwlock_nonblocking_lock(pthread_rwlock_t *lock TSRMLS_DC);

#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
