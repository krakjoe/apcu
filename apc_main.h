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
  |          George Schlossnagle <george@omniti.com>                     |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
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

/* $Id: apc_main.h 308594 2011-02-23 12:35:33Z gopalv $ */

#ifndef APC_MAIN_H
#define APC_MAIN_H

#include "apc_pool.h"
#include "apc_serializer.h"

/*
 * This module provides the primary interface between PHP and APC.
 */

extern int apc_module_init(int module_number TSRMLS_DC);
extern int apc_module_shutdown(TSRMLS_D);
extern int apc_process_init(int module_number TSRMLS_DC);
extern int apc_process_shutdown(TSRMLS_D);
extern int apc_request_init(TSRMLS_D);
extern int apc_request_shutdown(TSRMLS_D);

typedef enum _apc_copy_type {
    APC_NO_COPY = 0,
    APC_COPY_IN_OPCODE,
    APC_COPY_OUT_OPCODE,
    APC_COPY_IN_USER,
    APC_COPY_OUT_USER
} apc_copy_type;

typedef struct _apc_context_t
{
    apc_pool *pool;
    apc_copy_type copy;
    unsigned int force_update:1;
} apc_context_t;

/* {{{ struct apc_serializer_t */
typedef struct apc_serializer_t apc_serializer_t;
struct apc_serializer_t {
    const char *name;
    apc_serialize_t serialize;
    apc_unserialize_t unserialize;
    void *config;
};
/* }}} */

apc_serializer_t* apc_get_serializers(TSRMLS_D);
apc_serializer_t* apc_find_serializer(const char* name TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
