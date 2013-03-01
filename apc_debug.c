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
  |          Arun C. Murthy <arunc@yahoo-inc.com>                        |
  |          Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.
*/

/* $Id: apc_debug.c 307048 2011-01-03 23:53:17Z kalle $ */
#include "apc.h"
#include <stdio.h>
#include "zend.h"
#include "zend_compile.h"

#if defined(__DEBUG_APC__)

/* keep track of vld_dump_oparray() signature */
typedef void (*vld_dump_f) (zend_op_array * TSRMLS_DC);

#endif

void dump(zend_op_array *op_array TSRMLS_DC)
{
#if defined(__DEBUG_APC__)
  vld_dump_f dump_op_array;
  DL_HANDLE handle = NULL;

#ifdef PHP_WIN32
  handle = GetModuleHandle(NULL);
  
  if (!handle) {
	apc_warning("unable to fetch current module handle." TSRMLS_CC);
  }
#endif
  
  dump_op_array = (vld_dump_f) DL_FETCH_SYMBOL(handle, "vld_dump_oparray");
  
#ifdef PHP_WIN32
  DL_UNLOAD(handle);
#endif

  if(dump_op_array) {
    dump_op_array(op_array TSRMLS_CC);
  
    return;
  }
  
  apc_warning("vld is not installed or something even worse." TSRMLS_CC);
#endif
}
