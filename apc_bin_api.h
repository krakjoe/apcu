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
  | Authors: Brian Shire <shire@php.net>                                 |
  +----------------------------------------------------------------------+

 */

/* $Id: apc_bin_api.h 328743 2012-12-12 07:58:32Z ab $ */

#ifndef APC_BIN_API_H
#define APC_BIN_API_H

#include "ext/standard/basic_functions.h"

/* APC binload flags */
#define APC_BIN_VERIFY_MD5    1 << 0
#define APC_BIN_VERIFY_CRC32  1 << 1

typedef struct _apc_bd_entry_t {
    unsigned char type;
    uint num_functions;
    uint num_classes;
	struct {
		char*  str;
		uint   len;
	} key;
    apc_cache_entry_t val;
} apc_bd_entry_t;

typedef struct _apc_bd_t {
    unsigned int size;
    int swizzled;
    unsigned char md5[16];
    php_uint32 crc;
    unsigned int num_entries;
    apc_bd_entry_t *entries;
    int num_swizzled_ptrs;
    void ***swizzled_ptrs;
} apc_bd_t;

PHP_APCU_API apc_bd_t* apc_bin_dump(apc_cache_t* cache, HashTable *user_vars TSRMLS_DC);
PHP_APCU_API int apc_bin_load(apc_cache_t* cache, apc_bd_t *bd, int flags TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
