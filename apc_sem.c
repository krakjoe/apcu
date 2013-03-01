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
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_sem.c 307048 2011-01-03 23:53:17Z kalle $ */

#include "apc.h"

#ifdef APC_SEM_LOCKS

#include "apc_sem.h"
#include "php.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>

#if HAVE_SEMUN
/* we have semun, no need to define */
#else
#undef HAVE_SEMUN
union semun {
    int val;                  /* value for SETVAL */
    struct semid_ds *buf;     /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array;    /* array for GETALL, SETALL */
                              /* Linux specific part: */
    struct seminfo *__buf;    /* buffer for IPC_INFO */
};
#define HAVE_SEMUN 1
#endif

#ifndef SEM_R
# define SEM_R 0444
#endif
#ifndef SEM_A
# define SEM_A 0222
#endif

/* always use SEM_UNDO, otherwise we risk deadlock */
#define USE_SEM_UNDO

#ifdef USE_SEM_UNDO
# define UNDO SEM_UNDO
#else
# define UNDO 0
#endif

int apc_sem_create(int proj, int initval TSRMLS_DC)
{
    int semid;
    int perms = 0777;
    union semun arg;
    key_t key = IPC_PRIVATE;

    if ((semid = semget(key, 1, IPC_CREAT | IPC_EXCL | perms)) >= 0) {
        /* sempahore created for the first time, initialize now */
        arg.val = initval;
        if (semctl(semid, 0, SETVAL, arg) < 0) {
            apc_error("apc_sem_create: semctl(%d,...) failed:" TSRMLS_CC, semid);
        }
    }
    else if (errno == EEXIST) {
        /* sempahore already exists, don't initialize */
        if ((semid = semget(key, 1, perms)) < 0) {
            apc_error("apc_sem_create: semget(%u,...) failed:" TSRMLS_CC, key);
        }
        /* insert <sleazy way to avoid race condition> here */
    }
    else {
        apc_error("apc_sem_create: semget(%u,...) failed:" TSRMLS_CC, key);
    }

    return semid;
}

void apc_sem_destroy(int semid)
{
    /* we expect this call to fail often, so we do not check */
    union semun arg;
    semctl(semid, 0, IPC_RMID, arg);
}

void apc_sem_lock(int semid TSRMLS_DC)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op  = -1;
    op.sem_flg = UNDO;

    if (semop(semid, &op, 1) < 0) {
        if (errno != EINTR) {
            apc_error("apc_sem_lock: semop(%d) failed:" TSRMLS_CC, semid);
        }
    }
}

int apc_sem_nonblocking_lock(int semid TSRMLS_DC) 
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op  = -1;
    op.sem_flg = UNDO | IPC_NOWAIT;

    if (semop(semid, &op, 1) < 0) {
      if (errno == EAGAIN) {
        return 0;  /* Lock is already held */
      } else if (errno != EINTR) {
        apc_error("apc_sem_lock: semop(%d) failed:" TSRMLS_CC, semid);
      }
    }

    return 1;  /* Lock obtained */
}

void apc_sem_unlock(int semid TSRMLS_DC)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op  = 1;
    op.sem_flg = UNDO;

    if (semop(semid, &op, 1) < 0) {
        if (errno != EINTR) {
            apc_error("apc_sem_unlock: semop(%d) failed:" TSRMLS_CC, semid);
        }
    }
}

void apc_sem_wait_for_zero(int semid TSRMLS_DC)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op  = 0;
    op.sem_flg = UNDO;

    if (semop(semid, &op, 1) < 0) {
        if (errno != EINTR) {
            apc_error("apc_sem_waitforzero: semop(%d) failed:" TSRMLS_CC, semid);
        }
    }
}

int apc_sem_get_value(int semid TSRMLS_DC)
{
    union semun arg;
    unsigned short val[1];

    arg.array = val;
    if (semctl(semid, 0, GETALL, arg) < 0) {
        apc_error("apc_sem_getvalue: semctl(%d,...) failed:" TSRMLS_CC, semid);
    }
    return val[0];
}

#endif /* APC_SEM_LOCKS */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
