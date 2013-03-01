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

/* $Id: apc_sma.h 307048 2011-01-03 23:53:17Z kalle $ */

#ifndef APC_SMA_H
#define APC_SMA_H

#define ALLOC_DISTRIBUTION 0

#include "apc.h"

/* Simple shared memory allocator */

typedef struct _apc_segment_t apc_segment_t;

struct _apc_segment_t {
    size_t size;
    void* shmaddr;
#ifdef APC_MEMPROTECT
    void* roaddr;
#endif
};

extern void apc_sma_init(int numseg, size_t segsize, char *mmap_file_mask TSRMLS_DC);
extern void apc_sma_cleanup(TSRMLS_D);
extern void* apc_sma_malloc(size_t size TSRMLS_DC);
extern void* apc_sma_malloc_ex(size_t size, size_t fragment, size_t* allocated TSRMLS_DC);
extern void* apc_sma_realloc(void* p, size_t size TSRMLS_DC);
extern char* apc_sma_strdup(const char *s TSRMLS_DC);
extern void apc_sma_free(void* p TSRMLS_DC);
#if ALLOC_DISTRIBUTION 
extern size_t *apc_sma_get_alloc_distribution();
#endif

extern void* apc_sma_protect(void *p);
extern void* apc_sma_unprotect(void *p);

/* {{{ struct definition: apc_sma_link_t */
typedef struct apc_sma_link_t apc_sma_link_t;
struct apc_sma_link_t {
    long size;               /* size of this free block */
    long offset;             /* offset in segment of this block */
    apc_sma_link_t* next;   /* link to next free block */
};
/* }}} */

/* {{{ struct definition: apc_sma_info_t */
typedef struct apc_sma_info_t apc_sma_info_t;
struct apc_sma_info_t {
    int num_seg;            /* number of shared memory segments */
    size_t seg_size;           /* size of each shared memory segment */
    apc_sma_link_t** list;  /* there is one list per segment */
};
/* }}} */

extern apc_sma_info_t* apc_sma_info(zend_bool limited TSRMLS_DC);
extern void apc_sma_free_info(apc_sma_info_t* info TSRMLS_DC);

extern size_t apc_sma_get_avail_mem();
extern zend_bool apc_sma_get_avail_size(size_t size);
extern void apc_sma_check_integrity();

/* {{{ ALIGNWORD: pad up x, aligned to the system's word boundary */
typedef union { void* p; int i; long l; double d; void (*f)(); } apc_word_t;
#define ALIGNSIZE(x, size) ((size) * (1 + (((x)-1)/(size))))
#define ALIGNWORD(x) ALIGNSIZE(x, sizeof(apc_word_t))
/* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
