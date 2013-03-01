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

/* $Id: apc_pthreadmutex.h 307048 2011-01-03 23:53:17Z kalle $ */

#ifndef APC_PTHREADMUTEX_H
#define APC_PTHREADMUTEX_H

#include "apc.h"

#ifdef APC_PTHREADMUTEX_LOCKS

#include <pthread.h>

pthread_mutex_t *apc_pthreadmutex_create(pthread_mutex_t *lock TSRMLS_DC);
void apc_pthreadmutex_destroy(pthread_mutex_t *lock);
void apc_pthreadmutex_lock(pthread_mutex_t *lock TSRMLS_DC);
void apc_pthreadmutex_unlock(pthread_mutex_t *lock TSRMLS_DC);
zend_bool apc_pthreadmutex_nonblocking_lock(pthread_mutex_t *lock TSRMLS_DC);

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
