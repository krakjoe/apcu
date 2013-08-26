/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Yahoo! Inc. in 2008.

   Future revisions and derivatives of this source code must acknowledge
   Yahoo! Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.
 */

/* $Id: apc_pool.h 307048 2011-01-03 23:53:17Z kalle $ */

#ifndef APC_POOL_H
#define APC_POOL_H

#include "apc.h"
#include "apc_sma.h"

#ifndef APC_POOL_API_H
# include "apc_pool_api.h"
#endif

/*
 performs alignment checking on pool internals during MINIT
*/
extern PHP_APCU_API void apc_pool_init();
#endif

