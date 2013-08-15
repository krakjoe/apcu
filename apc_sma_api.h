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

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.
 */

#ifndef HAVE_APC_SMA_API_H
#define HAVE_APC_SMA_API_H
/* {{{ SMA API
	APC SMA API provides support for shared memory allocators to external libraries ( and to APC )
    Skip to the bottom macros for error free usage of the SMA API
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

/* {{{ function definitions for SMA API objects */
typedef void (*apc_sma_init_f) (zend_uint num, zend_ulong size, char *mask TSRMLS_DC);
typedef void (*apc_sma_cleanup_f) (TSRMLS_D); 
typedef void* (*apc_sma_malloc_f) (zend_ulong size TSRMLS_DC);
typedef void* (*apc_sma_malloc_ex_f) (zend_ulong size, zend_ulong fragment, zend_ulong *allocated TSRMLS_DC);
typedef void* (*apc_sma_realloc_f) (void* p, zend_ulong size TSRMLS_DC);
typedef char* (*apc_sma_strdup_f) (const char* str TSRMLS_DC);
typedef void (*apc_sma_free_f) (void *p TSRMLS_DC);
typedef void* (*apc_sma_protect_f) (void* p);
typedef void* (*apc_sma_unprotect_f) (void* p);
typedef apc_sma_info_t* (*apc_sma_info_f) (zend_bool limited TSRMLS_DC);
typedef void (*apc_sma_free_info_f) (apc_sma_info_t *info TSRMLS_DC);
typedef zend_ulong (*apc_sma_get_avail_mem_f) (void);
typedef zend_bool (*apc_sma_get_avail_size_f) (zend_ulong size);
typedef void (*apc_sma_check_integrity_f) (void); 
typedef void (*apc_sma_expunge_f)(void* pointer, zend_ulong size TSRMLS_DC); /* }}} */

/* {{{ struct definition: apc_sma_t */
typedef struct _apc_sma_t {
    zend_bool  initialized;                      /* flag to indicate this sma has been intialized */

    /* functions */
    apc_sma_init_f init;                         /* init */
    apc_sma_cleanup_f cleanup;                   /* cleanup */
    apc_sma_malloc_f smalloc;                    /* malloc */
    apc_sma_malloc_ex_f malloc_ex;               /* malloc_ex */
    apc_sma_realloc_f realloc;                   /* realloc */
    apc_sma_strdup_f strdup;                     /* strdup */
    apc_sma_free_f sfree;                        /* free */
    apc_sma_protect_f protect;                   /* protect */
    apc_sma_unprotect_f unprotect;               /* unprotect */
    apc_sma_info_f info;                         /* info */
    apc_sma_free_info_f free_info;               /* free info */
    apc_sma_get_avail_mem_f get_avail_mem;       /* get avail mem */
    apc_sma_get_avail_size_f get_avail_size;     /* get avail size */
    apc_sma_check_integrity_f check_integrity;   /* check integrity */

	/* callback */
	apc_sma_expunge_f expunge;                   /* expunge */
	void** data;                                 /* data */	
	
    /* info */
    zend_uint  num;                              /* number of segments */
    zend_ulong size;                             /* segment size */
    zend_uint  last;                             /* last segment */

    /* segments */
    apc_segment_t* segs;                         /* segments */
} apc_sma_t; /* }}} */

/*
* apc_sma_api_init will initialize a shared memory allocator with num segments of the given size
*
* should be called once per allocator per process
*/
PHP_APCU_API void apc_sma_api_init(apc_sma_t* sma,
                                   void** data,
							       apc_sma_expunge_f expunge,
                                   zend_uint num,
                                   zend_ulong size,
                                   char *mask TSRMLS_DC);

/*
* apc_sma_api_cleanup will free the sma allocator
*/
PHP_APCU_API void apc_sma_api_cleanup(apc_sma_t* sma TSRMLS_DC);

/*
* apc_smap_api_malloc will allocate a block from the sma of the given size
*/
PHP_APCU_API void* apc_sma_api_malloc(apc_sma_t* sma, 
                                      zend_ulong size TSRMLS_DC);

/*
* apc_sma_api_malloc_ex will allocate a block from the sma of the given size
*/
PHP_APCU_API void* apc_sma_api_malloc_ex(apc_sma_t* sma, 
                                         zend_ulong size, 
                                         zend_ulong fragment, 
                                         zend_ulong* allocated TSRMLS_DC);

/*
* apc_sma_api_realloc will reallocate p using a new block from sma (freeing the original p)
*/
PHP_APCU_API void* apc_sma_api_realloc(apc_sma_t* sma, 
                                       void* p, 
                                       zend_ulong size TSRMLS_DC);

/*
* apc_sma_api_strdup will duplicate the given string into a block from sma
*/
PHP_APCU_API char* apc_sma_api_strdup(apc_sma_t* sma, 
                                      const char* s TSRMLS_DC);

/*
* apc_sma_api_free will free p (which should be a pointer to a block allocated from sma)
*/
PHP_APCU_API void apc_sma_api_free(apc_sma_t* sma, 
                                   void* p TSRMLS_DC);

/*
* apc_sma_api_protect will protect p (which should be a pointer to a block allocated from sma)
*/
PHP_APCU_API void* apc_sma_api_protect(apc_sma_t* sma, 
                                       void* p);

/*
* apc_sma_api_protect will uprotect p (which should be a pointer to a block allocated from sma)
*/
PHP_APCU_API void* apc_sma_api_unprotect(apc_sma_t* sma, 
                                         void *p); 

/*
* apc_sma_api_info returns information about the allocator
*/
PHP_APCU_API apc_sma_info_t* apc_sma_api_info(apc_sma_t* sma, 
                                              zend_bool limited TSRMLS_DC); 

/*
* apc_sma_api_info_free_info is for freeing apc_sma_info_t* returned by apc_sma_api_info
*/
PHP_APCU_API void apc_sma_api_free_info(apc_sma_t* sma,
                                        apc_sma_info_t* info TSRMLS_DC); 

/*
* apc_sma_api_get_avail_mem will return the amount of memory available left to sma
*/
PHP_APCU_API zend_ulong apc_sma_api_get_avail_mem(apc_sma_t* sma); 

/*
* apc_sma_api_get_avail_size will return true if at least size bytes are available to the sma
*/
PHP_APCU_API zend_bool apc_sma_api_get_avail_size(apc_sma_t* sma, 
                                                  size_t size); 

/*
* apc_sma_api_check_integrity will check the integrity of sma
*/
PHP_APCU_API void apc_sma_api_check_integrity(apc_sma_t* sma); /* }}} */

/* {{{ ALIGNWORD: pad up x, aligned to the system's word boundary */
typedef union { void* p; int i; long l; double d; void (*f)(void); } apc_word_t;
#define ALIGNSIZE(x, size) ((size) * (1 + (((x)-1)/(size))))
#define ALIGNWORD(x) ALIGNSIZE(x, sizeof(apc_word_t))
/* }}} */

/*
* The following macros help to implement APC SMA in a few easy steps:
*  1) call apc_sma_api_decl(name) to intialize an APC SMA by the given name in a global project header
*  2) call apc_sma_api_impl(name) to implement the APC SMA in a compilation unit
*  3) call apc_sma_api_extern(name) in any compilation unit without direct access to your SMA
*
*  you can now call name.malloc, name.free etc from anywhere in your application, and there is no margin for error
*  your extension should call name.init during MINIT and name.cleanup during MSHUTDOWN
*/

/* {{{ Implementor macros */
#define apc_sma_api_name(name)       name
#define apc_sma_api_ptr(name)        &apc_sma_api_name(name)
#define apc_sma_api_func(name, func) name##_##func

/* {{{ Call in a header somewhere to extern all sma functions */
#define apc_sma_api_decl(name) \
    PHP_APCU_API void apc_sma_api_func(name, init)(zend_uint num, zend_ulong size, char* mask TSRMLS_DC); \
    PHP_APCU_API void apc_sma_api_func(name, cleanup)(TSRMLS_D); \
    PHP_APCU_API void* apc_sma_api_func(name, malloc)(zend_ulong size TSRMLS_DC); \
    PHP_APCU_API void* apc_sma_api_func(name, malloc_ex)(zend_ulong size, zend_ulong fragment, zend_ulong* allocated TSRMLS_DC); \
    PHP_APCU_API void* apc_sma_api_func(name, realloc)(void* p, zend_ulong size TSRMLS_DC); \
    PHP_APCU_API char* apc_sma_api_func(name, strdup)(const char* s TSRMLS_DC); \
    PHP_APCU_API void apc_sma_api_func(name, free)(void* p TSRMLS_DC); \
    PHP_APCU_API void* apc_sma_api_func(name, protect)(void* p); \
    PHP_APCU_API void* apc_sma_api_func(name, unprotect)(void* p); \
    PHP_APCU_API apc_sma_info_t* apc_sma_api_func(name, info)(zend_bool limited TSRMLS_DC); \
    PHP_APCU_API void apc_sma_api_func(name, free_info)(apc_sma_info_t* info TSRMLS_DC); \
    PHP_APCU_API zend_ulong apc_sma_api_func(name, get_avail_mem)(void); \
    PHP_APCU_API zend_bool apc_sma_api_func(name, get_avail_size)(zend_ulong size); \
    PHP_APCU_API void apc_sma_api_func(name, check_integrity)(void); /* }}} */

/* {{{ Call in a compilation unit */
#define apc_sma_api_impl(name, data, expunge) \
	apc_sma_t apc_sma_api_name(name) = {0, \
        &apc_sma_api_func(name, init), \
        &apc_sma_api_func(name, cleanup), \
        &apc_sma_api_func(name, malloc), \
        &apc_sma_api_func(name, malloc_ex), \
        &apc_sma_api_func(name, realloc), \
        &apc_sma_api_func(name, strdup), \
        &apc_sma_api_func(name, free), \
        &apc_sma_api_func(name, protect), \
        &apc_sma_api_func(name, unprotect), \
        &apc_sma_api_func(name, info), \
        &apc_sma_api_func(name, free_info), \
        &apc_sma_api_func(name, get_avail_mem), \
        &apc_sma_api_func(name, get_avail_size), \
        &apc_sma_api_func(name, check_integrity), \
    }; \
    PHP_APCU_API void apc_sma_api_func(name, init)(zend_uint num, zend_ulong size, char* mask TSRMLS_DC) \
        { apc_sma_api_init(apc_sma_api_ptr(name), (void**) data, (apc_sma_expunge_f) expunge, num, size, mask TSRMLS_CC); } \
    PHP_APCU_API void apc_sma_api_func(name, cleanup)(TSRMLS_D) \
        { apc_sma_api_cleanup(apc_sma_api_ptr(name) TSRMLS_CC); } \
    PHP_APCU_API void* apc_sma_api_func(name, malloc)(zend_ulong size TSRMLS_DC) \
        { return apc_sma_api_malloc(apc_sma_api_ptr(name), size TSRMLS_CC); } \
    PHP_APCU_API void* apc_sma_api_func(name, malloc_ex)(zend_ulong size, zend_ulong fragment, zend_ulong* allocated TSRMLS_DC) \
        { return apc_sma_api_malloc_ex(apc_sma_api_ptr(name), size, fragment, allocated TSRMLS_CC); } \
    PHP_APCU_API void* apc_sma_api_func(name, realloc)(void* p, zend_ulong size TSRMLS_DC) \
        { return apc_sma_api_realloc(apc_sma_api_ptr(name), p, size TSRMLS_CC); } \
    PHP_APCU_API char* apc_sma_api_func(name, strdup)(const char* s TSRMLS_DC) \
        { return apc_sma_api_strdup(apc_sma_api_ptr(name), s TSRMLS_CC); } \
    PHP_APCU_API void  apc_sma_api_func(name, free)(void* p TSRMLS_DC) \
        { apc_sma_api_free(apc_sma_api_ptr(name), p TSRMLS_CC); } \
    PHP_APCU_API void* apc_sma_api_func(name, protect)(void* p) \
        { return apc_sma_api_protect(apc_sma_api_ptr(name), p); } \
    PHP_APCU_API void* apc_sma_api_func(name, unprotect)(void* p) \
        { return apc_sma_api_unprotect(apc_sma_api_ptr(name), p); } \
    PHP_APCU_API apc_sma_info_t* apc_sma_api_func(name, info)(zend_bool limited TSRMLS_DC) \
        { return apc_sma_api_info(apc_sma_api_ptr(name), limited TSRMLS_CC); } \
    PHP_APCU_API void apc_sma_api_func(name, free_info)(apc_sma_info_t* info TSRMLS_DC) \
        { apc_sma_api_free_info(apc_sma_api_ptr(name), info TSRMLS_CC); } \
    PHP_APCU_API zend_ulong apc_sma_api_func(name, get_avail_mem)() \
        { return apc_sma_api_get_avail_mem(apc_sma_api_ptr(name)); } \
    PHP_APCU_API zend_bool apc_sma_api_func(name, get_avail_size)(zend_ulong size) \
        { return apc_sma_api_get_avail_size(apc_sma_api_ptr(name), size); } \
    PHP_APCU_API void apc_sma_api_func(name, check_integrity)() \
        { apc_sma_api_check_integrity(apc_sma_api_ptr(name)); }  /* }}} */

/* {{{ Call wherever access to the SMA object is required */
#define apc_sma_api_extern(name)     extern apc_sma_t apc_sma_api_name(name) /* }}} */

/* }}} */

#endif

