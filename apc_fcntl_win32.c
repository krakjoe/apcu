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
  | Authors: George Schlossnagle <george@omniti.com>                     |
  |          Edin Kadribasic <edink@php.net>                             |
  |          Pierre Joye <pierre@php.net>                                |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_fcntl_win32.c 309203 2011-03-14 06:47:16Z pajoye $ */

#include "apc.h"
#include "apc_fcntl.h"
#include <php.h>
#include <win32/flock.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int apc_fcntl_create(const char* pathname TSRMLS_DC)
{
    char lock_file[MAXPATHLEN];
    HANDLE fd;
    DWORD tmp_dirname_len;
    char lock_filename_tpl[] = ".apc.XXXXXX";
    char *lock_filename;

    tmp_dirname_len = GetTempPath(MAXPATHLEN, lock_file);
    if (!tmp_dirname_len) {
        return -1;
    }

    lock_filename = _mktemp(lock_filename_tpl);
    if (lock_filename == NULL) {
      return -1;
    }

    snprintf(lock_file + tmp_dirname_len, MAXPATHLEN - tmp_dirname_len - 1, "%s", lock_filename);

    fd = CreateFile(lock_file,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (fd == INVALID_HANDLE_VALUE) {
        apc_error("apc_fcntl_create: could not open %s" TSRMLS_CC, lock_file);
        return -1;
    }

    return (int)fd;
}

void apc_fcntl_destroy(int fd)
{
    CloseHandle((HANDLE)fd);
}

void apc_fcntl_lock(int fd TSRMLS_DC)
{
    OVERLAPPED offset = {0, 0, 0, 0, NULL};

    if (!LockFileEx((HANDLE)fd, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &offset)) {
        apc_error("apc_fcntl_lock failed errno:%d" TSRMLS_CC, GetLastError());
    }
}

void apc_fcntl_rdlock(int fd TSRMLS_DC)
{
    OVERLAPPED offset = {0, 0, 0, 0, NULL};

    if (!LockFileEx((HANDLE)fd, 0, 0, 1, 0, &offset)) {
        apc_error("apc_fcntl_rdlock failed errno:%d" TSRMLS_CC, GetLastError());
    }
}

void apc_fcntl_unlock(int fd TSRMLS_DC)
{
    OVERLAPPED offset = {0, 0, 0, 0, NULL};

    if (!UnlockFileEx((HANDLE)fd, 0, 1, 0, &offset)) {
        DWORD error_code = GetLastError();
        /* Ignore already unlocked error */
        if (error_code != ERROR_NOT_LOCKED) {
            apc_error("apc_fcntl_unlock failed errno:%d" TSRMLS_CC, error_code);
        }
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
