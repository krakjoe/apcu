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

/* $Id: apc_spin.c 307048 2011-01-03 23:53:17Z kalle $ */

#include "apc_spin.h"

#ifdef APC_SPIN_LOCKS

slock_t *apc_slock_create(slock_t *lock)
{
   S_INIT_LOCK(lock);
   return lock;
}

void apc_slock_destroy(slock_t *lock)
{
    return;
}

void apc_slock_lock(slock_t *lock TSRMLS_DC)
{
    S_LOCK(lock);
}

void apc_slock_unlock(slock_t *lock)
{
    S_UNLOCK(lock);
}

zend_bool apc_slock_nonblocking_lock(slock_t *lock)
{
    /* Technically we aren't supposed to call this directly, but the original
     *  code provides no method for absolute non-blocking locks, so we'll call into
     *  the TAS (test and set) functionality directly 
     */
    return !(TAS(lock));  /* if TAS returns 0 we obtained the lock, otherwise we failed */
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
