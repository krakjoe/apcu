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
  | Authors: Brian Shire <shire@.php.net>                                |
  +----------------------------------------------------------------------+

 */

/* $Id: apc_iterator.h 307048 2011-01-03 23:53:17Z kalle $ */

#ifndef APC_ITERATOR_H
#define APC_ITERATOR_H

#include "apc.h"
#include "apc_stack.h"

#if HAVE_PCRE || HAVE_BUNDLED_PCRE
/*  Deal with problem present until php-5.2.2 where php_pcre.h was not installed correctly */
#   if !HAVE_BUNDLED_PCRE && PHP_MAJOR_VERSION == 5 && (PHP_MINOR_VERSION < 2 || (PHP_MINOR_VERSION == 2 && PHP_RELEASE_VERSION < 2))
#       include "apc_php_pcre.h"
#   else
#       include "ext/pcre/php_pcre.h"
#   endif
#   include "ext/standard/php_smart_str.h"
#   define ITERATOR_PCRE 1
#endif


#define APC_ITERATOR_NAME "APCIterator"

#define APC_DEFAULT_CHUNK_SIZE 100

#define APC_LIST_ACTIVE   0x1
#define APC_LIST_DELETED  0x2

#define APC_ITER_TYPE       (1L << 0) 
#define APC_ITER_KEY        (1L << 1) 
#define APC_ITER_FILENAME   (1L << 2) 
#define APC_ITER_DEVICE     (1L << 3) 
#define APC_ITER_INODE      (1L << 4) 
#define APC_ITER_VALUE      (1L << 5) 
#define APC_ITER_MD5        (1L << 6) 
#define APC_ITER_NUM_HITS   (1L << 7) 
#define APC_ITER_MTIME      (1L << 8) 
#define APC_ITER_CTIME      (1L << 9) 
#define APC_ITER_DTIME      (1L << 10) 
#define APC_ITER_ATIME      (1L << 11) 
#define APC_ITER_REFCOUNT   (1L << 12) 
#define APC_ITER_MEM_SIZE   (1L << 13) 
#define APC_ITER_TTL        (1L << 14)

#define APC_ITER_NONE       (0x00000000L)
#define APC_ITER_ALL        (0xffffffffL)

typedef void* (*apc_iterator_item_cb_t)(slot_t **slot);


/* {{{ apc_iterator_t */
typedef struct _apc_iterator_t {
    zend_object obj;         /* must always be first */
    short int initialized;   /* sanity check in case __construct failed */
    long format;             /* format bitmask of the return values ie: key, value, info */
    int (*fetch)(struct _apc_iterator_t *iterator TSRMLS_DC);
                             /* fetch callback to fetch items from cache slots or lists */
    apc_cache_t *cache;      /* cache which we are iterating on */
    long slot_idx;           /* index to the slot array or linked list */
    long chunk_size;         /* number of entries to pull down per fetch */
    apc_stack_t *stack;      /* stack of entries pulled from cache */
    int stack_idx;           /* index into the current stack */
#ifdef ITERATOR_PCRE
    pcre *re;                /* regex filter on entry identifiers */
#endif
    char *regex;             /* original regex expression or NULL */
    int regex_len;           /* regex length */
    HashTable *search_hash;  /* hash of keys to iterate over */
    long key_idx;            /* incrementing index for numerical keys */
    short int totals_flag;   /* flag if totals have been calculated */
    long hits;               /* hit total */
    size_t size;             /* size total */
    long count;              /* count total */
} apc_iterator_t;
/* }}} */

/* {{{ apc_iterator_item */
typedef struct _apc_iterator_item_t {
    char *key;              /* string key */
    long key_len;           /* strlen of key */
    char *filename_key;     /* filename key used for deletion */
    zval *value;
} apc_iterator_item_t;
/* }}} */


extern int apc_iterator_init(int module_number TSRMLS_DC);
extern int apc_iterator_delete(zval *zobj TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
