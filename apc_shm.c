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
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_shm.c 307259 2011-01-08 12:05:24Z gopalv $ */

#include "apc_shm.h"
#include "apc.h"
#ifdef PHP_WIN32
/* shm functions are available in TSRM */
#include <tsrm/tsrm_win32.h>
#define key_t long
#else
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#endif

#ifndef SHM_R
# define SHM_R 0444 /* read permission */
#endif
#ifndef SHM_A
# define SHM_A 0222 /* write permission */
#endif

int apc_shm_create(int proj, size_t size TSRMLS_DC)
{
    int shmid;			/* shared memory id */
    int oflag;			/* permissions on shm */
    key_t key = IPC_PRIVATE;	/* shm key */

    oflag = IPC_CREAT | SHM_R | SHM_A;
    if ((shmid = shmget(key, size, oflag)) < 0) {
        apc_error("apc_shm_create: shmget(%d, %d, %d) failed: %s. It is possible that the chosen SHM segment size is higher than the operation system allows. Linux has usually a default limit of 32MB per segment." TSRMLS_CC, key, size, oflag, strerror(errno));
    }

    return shmid;
}

void apc_shm_destroy(int shmid)
{
    /* we expect this call to fail often, so we do not check */
    shmctl(shmid, IPC_RMID, 0);
}

apc_segment_t apc_shm_attach(int shmid, size_t size TSRMLS_DC)
{
    apc_segment_t segment; /* shm segment */

    if ((long)(segment.shmaddr = shmat(shmid, 0, 0)) == -1) {
        apc_error("apc_shm_attach: shmat failed:" TSRMLS_CC);
    }

#ifdef APC_MEMPROTECT
    
    if ((long)(segment.roaddr = shmat(shmid, 0, SHM_RDONLY)) == -1) {
        segment.roaddr = NULL;
    }

#endif

    segment.size = size;

    /*
     * We set the shmid for removal immediately after attaching to it. The
     * segment won't disappear until all processes have detached from it.
     */
    apc_shm_destroy(shmid);
    return segment;
}

void apc_shm_detach(apc_segment_t* segment TSRMLS_DC)
{
    if (shmdt(segment->shmaddr) < 0) {
        apc_error("apc_shm_detach: shmdt failed:" TSRMLS_CC);
    }

#ifdef APC_MEMPROTECT
    if (segment->roaddr && shmdt(segment->roaddr) < 0) {
        apc_error("apc_shm_detach: shmdt failed:" TSRMLS_CC);
    }
#endif
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
