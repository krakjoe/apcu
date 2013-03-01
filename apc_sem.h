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

/* $Id: apc_sem.h 307048 2011-01-03 23:53:17Z kalle $ */

#ifndef APC_SEM_H
#define APC_SEM_H

/* Wrapper functions for SysV sempahores */

extern int apc_sem_create(int proj, int initval TSRMLS_DC);
extern void apc_sem_destroy(int semid);
extern void apc_sem_lock(int semid TSRMLS_DC);
extern int apc_sem_nonblocking_lock(int semid TSRMLS_DC); 
extern void apc_sem_unlock(int semid TSRMLS_DC);
extern void apc_sem_wait_for_zero(int semid TSRMLS_DC);
extern int apc_sem_get_value(int semid TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
