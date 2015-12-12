/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2015 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Anatol Belski <ab@php.net>                                  |
  +----------------------------------------------------------------------+
 */

#ifndef APC_WINDOWS_GLOBAL_MUTEX_H
#define APC_WINDOWS_GLOBAL_MUTEX_H

#include "apc.h"

#ifdef APC_WINDOWS_GLOBAL_MUTEX


#define WINDOWS_MUTEX_NAME "apc" ARCHITECTURE 

typedef struct _apc_windows_global_mutex_t {
    HANDLE mutex;
    char   mutex_name[64];
} apc_windows_global_mutex_t;

apc_windows_global_mutex_t *apc_windows_global_mutex_create(apc_windows_global_mutex_t *obj);
zend_bool apc_windows_global_mutex_lock(apc_windows_global_mutex_t *lock);
zend_bool apc_windows_global_mutex_unlock(apc_windows_global_mutex_t *obj);

#endif

#endif /* APC_WINDOWS_GLOBAL_MUTEX_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
