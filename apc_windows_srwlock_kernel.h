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
  | Authors: Pierre Joye <pierre@php.net>                                |
  +----------------------------------------------------------------------+
 */
/* $Id$ */

#ifndef APC_WINDOWS_CS_RWLOCK_H
#define APC_WINDOWS_CS_RWLOCK_H

#include "apc.h"

#ifdef APC_SRWLOCK_KERNEL

typedef struct _RTL_RWLOCK {
   RTL_CRITICAL_SECTION rtlCS;

   HANDLE hSharedReleaseSemaphore;
   UINT   uSharedWaiters;

   HANDLE hExclusiveReleaseSemaphore;
   UINT   uExclusiveWaiters;

   INT    iNumberActive;
   HANDLE hOwningThreadId;
   DWORD  dwTimeoutBoost;
   PVOID  pDebugInfo;
} RTL_RWLOCK, *LPRTL_RWLOCK;

#define apc_windows_cs_rwlock_t RTL_RWLOCK

struct apc_windows_cs_rwlock_t {
    CRITICAL_SECTION cs;
    LONG writers_waiting_count;
    LONG readers_waiting_count;
    DWORD active_writers_readers_flag;
    HANDLE ready_to_read;
    HANDLE ready_to_write;
    DWORD reader_races_lost;
};

apc_windows_cs_rwlock_t *apc_windows_cs_create(apc_windows_cs_rwlock_t *lock TSRMLS_DC);
void apc_windows_cs_destroy(apc_windows_cs_rwlock_t *lock);
void apc_windows_cs_lock(apc_windows_cs_rwlock_t *lock TSRMLS_DC);
void apc_windows_cs_rdlock(apc_windows_cs_rwlock_t *lock TSRMLS_DC);
void apc_windows_cs_unlock_rd(apc_windows_cs_rwlock_t *lock TSRMLS_DC);
void apc_windows_cs_unlock_wr(apc_windows_cs_rwlock_t *lock TSRMLS_DC);
# if NONBLOCKING_LOCK_AVAILABLE==1 /* Only in win7/2008 */
zend_bool apc_pthreadrwlock_nonblocking_lock(apc_windows_cs_rwlock_t *lock TSRMLS_DC);
# endif
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
