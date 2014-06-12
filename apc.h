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

/* $Id: apc.h 328292 2012-11-09 07:05:17Z laruence $ */

#ifndef APC_H
#define APC_H

/*
 * This module defines utilities and helper functions used elsewhere in APC.
 */
#ifdef PHP_WIN32
# define PHP_APCU_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define PHP_APCU_API __attribute__ ((visibility("default")))
#else
# define PHP_APCU_API
#endif

/* Commonly needed C library headers. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* UNIX headers (needed for struct stat) */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef PHP_WIN32
#include <unistd.h>
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "php.h"
#include "main/php_streams.h"

/* typedefs for extensible memory allocators */
typedef void* (*apc_malloc_t)(size_t TSRMLS_DC);
typedef void  (*apc_free_t)  (void * TSRMLS_DC);

/* wrappers for memory allocation routines */
PHP_APCU_API void* apc_emalloc(size_t n TSRMLS_DC);
PHP_APCU_API void* apc_erealloc(void* p, size_t n TSRMLS_DC);
PHP_APCU_API void* apc_php_malloc(size_t n TSRMLS_DC);
PHP_APCU_API void  apc_php_free(void* p TSRMLS_DC);
PHP_APCU_API void  apc_efree(void* p TSRMLS_DC);
PHP_APCU_API char* apc_estrdup(const char* s TSRMLS_DC);
PHP_APCU_API void* apc_xstrdup(const char* s, apc_malloc_t f TSRMLS_DC);
PHP_APCU_API void* apc_xmemcpy(const void* p, size_t n, apc_malloc_t f TSRMLS_DC);

/* console display functions */
PHP_APCU_API void apc_error(const char *format TSRMLS_DC, ...);
PHP_APCU_API void apc_warning(const char *format TSRMLS_DC, ...);
PHP_APCU_API void apc_notice(const char *format TSRMLS_DC, ...);
PHP_APCU_API void apc_debug(const char *format TSRMLS_DC, ...);

/* string and text manipulation */
PHP_APCU_API char* apc_append(const char* s, const char* t TSRMLS_DC);
PHP_APCU_API char* apc_substr(const char* s, int start, int length TSRMLS_DC);
PHP_APCU_API char** apc_tokenize(const char* s, char delim TSRMLS_DC);

/* apc_crc32: returns the CRC-32 checksum of the first len bytes in buf */
PHP_APCU_API unsigned int apc_crc32(const unsigned char* buf, unsigned int len);

/* apc_flip_hash flips keys and values for faster searching */
PHP_APCU_API HashTable* apc_flip_hash(HashTable *hash);

#define APC_NEGATIVE_MATCH 1
#define APC_POSITIVE_MATCH 2

#define apc_time() \
    (APCG(use_request_time) ? (time_t) sapi_get_request_time(TSRMLS_C) : time(0));

#if defined(__GNUC__)
# define APC_UNUSED __attribute__((unused))
# define APC_USED __attribute__((used))
# define APC_ALLOC __attribute__((malloc))
# if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__  > 2)
#  define APC_HOTSPOT __attribute__((hot))
# else 
#  define APC_HOTSPOT
# endif
#else 
# define APC_UNUSED
# define APC_USED
# define APC_ALLOC 
# define APC_HOTSPOT 
#endif

/*
* Serializer API
*/
#define APC_SERIALIZER_ABI "0"
#define APC_SERIALIZER_CONSTANT "\000apc_register_serializer-" APC_SERIALIZER_ABI

#define APC_SERIALIZER_NAME(module) module##_apc_serializer
#define APC_UNSERIALIZER_NAME(module) module##_apc_unserializer

#define APC_SERIALIZER_ARGS unsigned char **buf, size_t *buf_len, const zval *value, void *config TSRMLS_DC
#define APC_UNSERIALIZER_ARGS zval **value, unsigned char *buf, size_t buf_len, void *config TSRMLS_DC

typedef int (*apc_serialize_t)(APC_SERIALIZER_ARGS);
typedef int (*apc_unserialize_t)(APC_UNSERIALIZER_ARGS);

/* {{{ struct definition: apc_serializer_t */
typedef struct apc_serializer_t {
    const char*        name;
    apc_serialize_t    serialize;
    apc_unserialize_t  unserialize;
    void*              config;
} apc_serializer_t;
/* }}} */

/* {{{ _apc_register_serializer
 registers the serializer using the given name and paramters */
PHP_APCU_API int _apc_register_serializer(const char* name,
                                               apc_serialize_t serialize,
                                               apc_unserialize_t unserialize,
                                               void *config TSRMLS_DC); /* }}} */

/* {{{ apc_get_serializers 
 fetches the list of serializers */
PHP_APCU_API apc_serializer_t* apc_get_serializers(TSRMLS_D); /* }}} */

/* {{{ apc_find_serializer
 finds a previously registered serializer by name */
PHP_APCU_API apc_serializer_t* apc_find_serializer(const char* name TSRMLS_DC); /* }}} */

/* {{{ default serializers */
PHP_APCU_API int APC_SERIALIZER_NAME(php) (APC_SERIALIZER_ARGS);
PHP_APCU_API int APC_UNSERIALIZER_NAME(php) (APC_UNSERIALIZER_ARGS); /* }}} */

/* {{{ eval serializers */
PHP_APCU_API int APC_SERIALIZER_NAME(eval) (APC_SERIALIZER_ARGS);
PHP_APCU_API int APC_UNSERIALIZER_NAME(eval) (APC_UNSERIALIZER_ARGS); /* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
