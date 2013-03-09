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
/*
 These APIs are not actually exposed nor documented. But should work fine
 from a binary as available since XP without signature changes.
*/
/*
TODOs:
non blocking could be possible using the fWait argument (to 0). However
I'm not sure whether the wait handlers is actually implemented in all
supported platforms (xp+). could be enabled later once really tested.
 */
/* $Id: $ */

#include <php.h>

#ifdef APC_SRWLOCK_KERNEL
#include "apc_windows_srwlock_kernel.h"

/*
For references:
void WINAPI RtlInitializeResource(LPRTL_RWLOCK rwl);
void WINAPI RtlDeleteResource(LPRTL_RWLOCK rwl);
BYTE WINAPI RtlAcquireResourceExclusive(LPRTL_RWLOCK rwl, BYTE fWait);
BYTE WINAPI RtlAcquireResourceShared(LPRTL_RWLOCK rwl, BYTE fWait);
void WINAPI RtlReleaseResource(LPRTL_RWLOCK rwl);
*/
typedef void (WINAPI *tRtlInitializeResource)(LPRTL_RWLOCK rwl);
typedef void (WINAPI *tRtlDeleteResource)(LPRTL_RWLOCK rwl);
typedef BYTE (WINAPI *tRtlAcquireResourceExclusive)(LPRTL_RWLOCK rwl, BYTE fWait);
typedef BYTE (WINAPI *tRtlAcquireResourceShared)(LPRTL_RWLOCK rwl, BYTE fWait);
typedef void (WINAPI *tRtlReleaseResource)(LPRTL_RWLOCK rwl);
typedef void (WINAPI *tRtlDumpResource)(LPRTL_RWLOCK rwl);

tRtlInitializeResource        pRtlInitializeResource = 0;
tRtlDeleteResource            pRtlDeleteResource = 0;
tRtlAcquireResourceExclusive  pRtlAcquireResourceExclusive = 0;
tRtlAcquireResourceShared     pRtlAcquireResourceShared = 0;
tRtlReleaseResource           pRtlReleaseResource = 0;
tRtlDumpResource              pRtlDumpResource = 0;

HINSTANCE ntdll;

void apc_windows_cs_status(apc_windows_cs_rwlock_t *lock );
apc_windows_cs_rwlock_t *apc_windows_cs_create(apc_windows_cs_rwlock_t *lock TSRMLS_DC) 
{
    ntdll = LoadLibrary("ntdll.dll");
    if (ntdll == 0) {
      return NULL;
    }

    pRtlInitializeResource = (tRtlInitializeResource) GetProcAddress(ntdll, "RtlInitializeResource");
    pRtlDeleteResource = (tRtlDeleteResource) GetProcAddress(ntdll, "RtlDeleteResource");
    pRtlAcquireResourceExclusive = (tRtlAcquireResourceExclusive) GetProcAddress(ntdll, "RtlAcquireResourceExclusive");
    pRtlAcquireResourceShared = (tRtlAcquireResourceShared) GetProcAddress(ntdll, "RtlAcquireResourceShared");
    pRtlReleaseResource = (tRtlReleaseResource) GetProcAddress(ntdll, "RtlReleaseResource");
    pRtlDumpResource = (tRtlReleaseResource) GetProcAddress(ntdll, "RtlDumpResource");
    if (pRtlInitializeResource == 0 || pRtlDeleteResource == 0 || pRtlAcquireResourceExclusive == 0 || 
        pRtlAcquireResourceShared == 0 || pRtlReleaseResource == 0 || pRtlDumpResource == 0) {
        return NULL;
    }
    pRtlInitializeResource(lock);
    return lock;
}

void apc_windows_cs_destroy(apc_windows_cs_rwlock_t *lock)
{
    __try
    {
        pRtlDeleteResource(lock);
    }
        __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
               EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        /* Ignore exception (resource was freed during shutdown of another thread) */
    }
    FreeLibrary(ntdll);
    return;
}

void apc_windows_cs_lock(apc_windows_cs_rwlock_t *lock TSRMLS_DC)
{
    pRtlAcquireResourceExclusive(lock, 1);
}

void apc_windows_cs_rdlock(apc_windows_cs_rwlock_t *lock TSRMLS_DC)
{
    pRtlAcquireResourceShared(lock, 1);
}

void apc_windows_cs_unlock_rd(apc_windows_cs_rwlock_t *lock TSRMLS_DC)
{
    pRtlReleaseResource(lock);
}

void apc_windows_cs_unlock_wr(apc_windows_cs_rwlock_t *lock TSRMLS_DC)
{
    pRtlReleaseResource(lock);
}

/* debugging purposes, output using trace msgs */
void apc_windows_cs_status(apc_windows_cs_rwlock_t *lock)
{
    pRtlDumpResource(lock);
    return;
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
