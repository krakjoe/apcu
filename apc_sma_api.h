#ifndef HAVE_APC_SMA_API_H
#define HAVE_APC_SMA_API_H
/* {{{ struct definition: apc_segment_t */
typedef struct _apc_segment_t {
    size_t size;
    void* shmaddr;
#ifdef APC_MEMPROTECT
    void* roaddr;
#endif
} apc_segment_t; /* }}} */

/* {{{ struct definition: apc_sma_t */
typedef struct _apc_sma_t {
	zend_bool  initialized;
	zend_uint  num;
	zend_ulong size;
	zend_uint  last;
	apc_segment_t* segs;
} apc_sma_t; /* }}} */

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

/* {{{ SMA API 
	APC SMA API provides support for shared memory allocators to external library
    A guide to imlpementation to come ( and is contained in the APC source code )
*/
extern void apc_sma_api_init(apc_sma_t* sma, zend_uint num, zend_ulong size, char *mask TSRMLS_DC);
extern void apc_sma_api_cleanup(apc_sma_t* sma TSRMLS_DC);
extern void* apc_sma_api_malloc(apc_sma_t* sma, zend_ulong size TSRMLS_DC);
extern void* apc_sma_api_malloc_ex(apc_sma_t* sma, zend_ulong size, zend_ulong fragment, zend_ulong* allocated TSRMLS_DC);
extern void* apc_sma_api_realloc(apc_sma_t* sma, void* p, zend_ulong size TSRMLS_DC);
extern char* apc_sma_api_strdup(apc_sma_t* sma, const char* s TSRMLS_DC);
extern void apc_sma_api_free(apc_sma_t* sma, void* p TSRMLS_DC);
#if ALLOC_DISTRIBUTION 
extern zend_ulong *apc_sma_api_get_alloc_distribution(apc_sma_t* sma);
#endif
extern void* apc_sma_api_protect(apc_sma_t* sma, void* p);
extern void* apc_sma_api_unprotect(apc_sma_t* sma, void *p); 

extern apc_sma_info_t* apc_sma_api_info(apc_sma_t* sma, zend_bool limited TSRMLS_DC); 
extern void apc_sma_api_free_info(apc_sma_t* sma, apc_sma_info_t* info TSRMLS_DC); 
extern zend_ulong apc_sma_api_get_avail_mem(apc_sma_t* sma); 
extern zend_bool apc_sma_api_get_avail_size(apc_sma_t* sma, size_t size); 
extern void apc_sma_api_check_integrity(apc_sma_t* sma); /* }}} */

#endif
