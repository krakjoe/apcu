/*
  +----------------------------------------------------------------------+
  | APCu                                                                 |
  +----------------------------------------------------------------------+
  | Copyright (c) 2013 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Joe Watkins <joe.watkins@live.co.uk>                         |
  +----------------------------------------------------------------------+
 */

/*
 This module defines an SMA API for third parties
*/

#ifndef HAVE_APC_SMA_API_H
#define HAVE_APC_SMA_API_H
/* {{{ SMA API
	APC SMA API provides support for shared memory allocators to external library
    A guide to imlpementation to come ( and is contained in the APC source code )
*/

/* {{{ struct definition: apc_segment_t */
typedef struct _apc_segment_t {
    size_t size;            /* size of this segment */
    void* shmaddr;          /* address of shared memory */
#ifdef APC_MEMPROTECT
    void* roaddr;           /* read only (mprotect'd) address */
#endif
} apc_segment_t; /* }}} */

/* {{{ struct definition: apc_sma_link_t */
typedef struct apc_sma_link_t apc_sma_link_t;
struct apc_sma_link_t {
    long size;              /* size of this free block */
    long offset;            /* offset in segment of this block */
    apc_sma_link_t* next;   /* link to next free block */
};
/* }}} */

/* {{{ struct definition: apc_sma_info_t */
typedef struct apc_sma_info_t apc_sma_info_t;
struct apc_sma_info_t {
    int num_seg;            /* number of segments */
    size_t seg_size;        /* segment size */
    apc_sma_link_t** list;  /* one list per segment of links */
};
/* }}} */

/* {{{ struct definition: apc_sma_t */
typedef struct _apc_sma_t {
	zend_bool  initialized; /* flag to signify this sma has been intialized */
	zend_uint  num;         /* number of segments */
	zend_ulong size;        /* segment size */
	zend_uint  last;        /* last segment */
	apc_segment_t* segs;    /* segments */
} apc_sma_t; /* }}} */

/*
* apc_sma_api_init will initialize a shared memory allocator with num segments of the given size
*
* should be called once per allocator per process
*/
extern void apc_sma_api_init(apc_sma_t* sma, zend_uint num, zend_ulong size, char *mask TSRMLS_DC);

/*
* apc_sma_api_cleanup will free the sma allocator
*/
extern void apc_sma_api_cleanup(apc_sma_t* sma TSRMLS_DC);

/*
* apc_smap_api_malloc will allocate a block from the sma of the given size
*/
extern void* apc_sma_api_malloc(apc_sma_t* sma, zend_ulong size TSRMLS_DC);

/*
* apc_sma_api_malloc_ex will allocate a block from the sma of the given size
*/
extern void* apc_sma_api_malloc_ex(apc_sma_t* sma, zend_ulong size, zend_ulong fragment, zend_ulong* allocated TSRMLS_DC);

/*
* apc_sma_api_realloc will reallocate p using a new block from sma (freeing the original p)
*/
extern void* apc_sma_api_realloc(apc_sma_t* sma, void* p, zend_ulong size TSRMLS_DC);

/*
* apc_sma_api_strdup will duplicate the given string into a block from sma
*/
extern char* apc_sma_api_strdup(apc_sma_t* sma, const char* s TSRMLS_DC);

/*
* apc_sma_api_free will free p (which should be a pointer to a block allocated from sma)
*/
extern void apc_sma_api_free(apc_sma_t* sma, void* p TSRMLS_DC);

#if ALLOC_DISTRIBUTION 
extern zend_ulong *apc_sma_api_get_alloc_distribution(apc_sma_t* sma);
#endif

/*
* apc_sma_api_protect will protect p (which should be a pointer to a block allocated from sma)
*/
extern void* apc_sma_api_protect(apc_sma_t* sma, void* p);

/*
* apc_sma_api_protect will uprotect p (which should be a pointer to a block allocated from sma)
*/
extern void* apc_sma_api_unprotect(apc_sma_t* sma, void *p); 

/*
* apc_sma_api_info returns information about the allocator
*/
extern apc_sma_info_t* apc_sma_api_info(apc_sma_t* sma, zend_bool limited TSRMLS_DC); 

/*
* apc_sma_api_info_free_info is for freeing apc_sma_info_t* returned by apc_sma_api_info
*/
extern void apc_sma_api_free_info(apc_sma_t* sma, apc_sma_info_t* info TSRMLS_DC); 

/*
* apc_sma_api_get_avail_mem will return the amount of memory available left to sma
*/
extern zend_ulong apc_sma_api_get_avail_mem(apc_sma_t* sma); 

/*
* apc_sma_api_get_avail_size will return true if at least size bytes are available to the sma
*/
extern zend_bool apc_sma_api_get_avail_size(apc_sma_t* sma, size_t size); 

/*
* apc_sma_api_check_integrity will check the integrity of sma
*/
extern void apc_sma_api_check_integrity(apc_sma_t* sma); /* }}} */

/* {{{ ALIGNWORD: pad up x, aligned to the system's word boundary */
typedef union { void* p; int i; long l; double d; void (*f)(); } apc_word_t;
#define ALIGNSIZE(x, size) ((size) * (1 + (((x)-1)/(size))))
#define ALIGNWORD(x) ALIGNSIZE(x, sizeof(apc_word_t))
/* }}} */

#endif

