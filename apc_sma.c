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
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_sma.c 328896 2012-12-26 05:28:14Z laruence $ */

#include "apc_sma.h"
#include "apc.h"
#include "apc_globals.h"
#include "apc_lock.h"
#include "apc_shm.h"
#include "apc_cache.h"

#include <limits.h>
#include "apc_mmap.h"

#ifdef APC_SMA_DEBUG
# ifdef HAVE_VALGRIND_MEMCHECK_H
#  include <valgrind/memcheck.h>
# endif
# define APC_SMA_CANARIES 1
#endif

enum { 
	DEFAULT_NUMSEG=1, 
	DEFAULT_SEGSIZE=30*1024*1024 };

typedef struct sma_header_t sma_header_t;
struct sma_header_t {
    apc_lock_t sma_lock;    /* segment lock */
    size_t segsize;         /* size of entire segment */
    size_t avail;           /* bytes available (not necessarily contiguous) */
};

#define SMA_HDR(sma, i)  ((sma_header_t*)((sma->segs[i]).shmaddr))
#define SMA_ADDR(sma, i) ((char*)(SMA_HDR(sma, i)))
#define SMA_RO(sma, i)   ((char*)(sma->segs[i]).roaddr)
#define SMA_LCK(sma, i)  ((SMA_HDR(sma, i))->sma_lock)

#if 0
/* global counter for identifying blocks
 * Technically it is possible to do the same
 * using offsets, but double allocations of the
 * same offset can happen. */
static volatile size_t block_id = 0;
#endif

typedef struct block_t block_t;
struct block_t {
    size_t size;       /* size of this block */
    size_t prev_size;  /* size of sequentially previous block, 0 if prev is allocated */
    size_t fnext;      /* offset in segment of next free block */
    size_t fprev;      /* offset in segment of prev free block */
#ifdef APC_SMA_CANARIES
    size_t canary;     /* canary to check for memory overwrites */
#endif
#if 0
    size_t id;         /* identifier for the memory block */
#endif
};

/* The macros BLOCKAT and OFFSET are used for convenience throughout this
 * module. Both assume the presence of a variable shmaddr that points to the
 * beginning of the shared memory segment in question. */

#define BLOCKAT(offset) ((block_t*)((char *)shmaddr + offset))
#define OFFSET(block) ((size_t)(((char*)block) - (char*)shmaddr))

/* macros for getting the next or previous sequential block */
#define NEXT_SBLOCK(block) ((block_t*)((char*)block + block->size))
#define PREV_SBLOCK(block) (block->prev_size ? ((block_t*)((char*)block - block->prev_size)) : NULL)

/* Canary macros for setting, checking and resetting memory canaries */
#ifdef APC_SMA_CANARIES
    #define SET_CANARY(v) (v)->canary = 0x42424242
    #define CHECK_CANARY(v) assert((v)->canary == 0x42424242)
    #define RESET_CANARY(v) (v)->canary = -42
#else
    #define SET_CANARY(v) 
    #define CHECK_CANARY(v)
    #define RESET_CANARY(v)
#endif

/* {{{ MINBLOCKSIZE */
#define MINBLOCKSIZE (ALIGNWORD(1) + ALIGNWORD(sizeof(block_t)))
/* }}} */

/* {{{ sma_allocate: tries to allocate at least size bytes in a segment */
static APC_HOTSPOT size_t sma_allocate(sma_header_t* header, zend_ulong size, zend_ulong fragment, zend_ulong *allocated)
{
    void* shmaddr;          /* header of shared memory segment */
    block_t* prv;           /* block prior to working block */
    block_t* cur;           /* working block in list */
    block_t* prvnextfit;    /* block before next fit */
    size_t realsize;        /* actual size of block needed, including header */
    const size_t block_size = ALIGNWORD(sizeof(struct block_t));

    realsize = ALIGNWORD(size + block_size);

    /*
     * First, insure that the segment contains at least realsize free bytes,
     * even if they are not contiguous.
     */
    shmaddr = header;

    if (header->avail < realsize) {
        return -1;
    }

    prvnextfit = 0;     /* initially null (no fit) */
    prv = BLOCKAT(ALIGNWORD(sizeof(sma_header_t)));

    CHECK_CANARY(prv);

    while (prv->fnext != 0) {
        cur = BLOCKAT(prv->fnext);

		CHECK_CANARY(cur);

        /* If it can fit realsize bytes in cur block, stop searching */
        if (cur->size >= realsize) {
            prvnextfit = prv;
            break;
        }
        prv = cur;
    }

    if (prvnextfit == 0) {
        return -1;
    }

    prv = prvnextfit;
    cur = BLOCKAT(prv->fnext);

    CHECK_CANARY(prv);
    CHECK_CANARY(cur);

    if (cur->size == realsize || (cur->size > realsize && cur->size < (realsize + (MINBLOCKSIZE + fragment)))) {
        /* cur is big enough for realsize, but too small to split - unlink it */
        *(allocated) = cur->size - block_size;
        prv->fnext = cur->fnext;
        BLOCKAT(cur->fnext)->fprev = OFFSET(prv);
        NEXT_SBLOCK(cur)->prev_size = 0;  /* block is alloc'd */
    } else {
        /* nextfit is too big; split it into two smaller blocks */
        block_t* nxt;      /* the new block (chopped part of cur) */
        size_t oldsize;    /* size of cur before split */

        oldsize = cur->size;
        cur->size = realsize;
        *(allocated) = cur->size - block_size;
        nxt = NEXT_SBLOCK(cur);
        nxt->prev_size = 0;                       /* block is alloc'd */
        nxt->size = oldsize - realsize;           /* and fix the size */
        NEXT_SBLOCK(nxt)->prev_size = nxt->size;  /* adjust size */
        SET_CANARY(nxt);

        /* replace cur with next in free list */
        nxt->fnext = cur->fnext;
        nxt->fprev = cur->fprev;
        BLOCKAT(nxt->fnext)->fprev = OFFSET(nxt);
        BLOCKAT(nxt->fprev)->fnext = OFFSET(nxt);
#if 0
        nxt->id = -1;
#endif
    }

    cur->fnext = 0;

    /* update the block header */
    header->avail -= cur->size;

    SET_CANARY(cur);

#if 0
    cur->id = ++block_id;
    fprintf(stderr, "allocate(realsize=%d,size=%d,id=%d)\n", (int)(size), (int)(cur->size), cur->id);
#endif

    return OFFSET(cur) + block_size;
}
/* }}} */

/* {{{ sma_deallocate: deallocates the block at the given offset */
static APC_HOTSPOT size_t sma_deallocate(void* shmaddr, size_t offset)
{
    sma_header_t* header;   /* header of shared memory segment */
    block_t* cur;       /* the new block to insert */
    block_t* prv;       /* the block before cur */
    block_t* nxt;       /* the block after cur */
    size_t size;        /* size of deallocated block */

    offset -= ALIGNWORD(sizeof(struct block_t));
    assert(offset >= 0);

    /* find position of new block in free list */
    cur = BLOCKAT(offset);

    /* update the block header */
    header = (sma_header_t*) shmaddr;
    header->avail += cur->size;
    size = cur->size;

    if (cur->prev_size != 0) {
        /* remove prv from list */
        prv = PREV_SBLOCK(cur);
        BLOCKAT(prv->fnext)->fprev = prv->fprev;
        BLOCKAT(prv->fprev)->fnext = prv->fnext;
        /* cur and prv share an edge, combine them */
        prv->size +=cur->size;
       
		RESET_CANARY(cur);
        cur = prv;
    }

    nxt = NEXT_SBLOCK(cur);
    if (nxt->fnext != 0) {
        assert(NEXT_SBLOCK(NEXT_SBLOCK(cur))->prev_size == nxt->size);
        /* cur and nxt shared an edge, combine them */
        BLOCKAT(nxt->fnext)->fprev = nxt->fprev;
        BLOCKAT(nxt->fprev)->fnext = nxt->fnext;
        cur->size += nxt->size;

		CHECK_CANARY(nxt);

#if 0
        nxt->id = -1; /* assert this or set it ? */
#endif

        RESET_CANARY(nxt);
    }

    NEXT_SBLOCK(cur)->prev_size = cur->size;

    /* insert new block after prv */
    prv = BLOCKAT(ALIGNWORD(sizeof(sma_header_t)));
    cur->fnext = prv->fnext;
    prv->fnext = OFFSET(cur);
    cur->fprev = OFFSET(prv);
    BLOCKAT(cur->fnext)->fprev = OFFSET(cur);

    return size;
}
/* }}} */

/* {{{ APC SMA API */
PHP_APCU_API void apc_sma_api_init(apc_sma_t* sma, void** data, apc_sma_expunge_f expunge, zend_uint num, zend_ulong size, char *mask TSRMLS_DC) {
	uint i;

    if (sma->initialized) {
        return;
    }

    sma->initialized = 1;
	sma->expunge = expunge;
	sma->data = data;
	
#if APC_MMAP
    /*
     * I don't think multiple anonymous mmaps makes any sense
     * so force sma_numseg to 1 in this case
     */
    if(!mask || 
       (mask && !strlen(mask)) ||
       (mask && !strcmp(mask, "/dev/zero"))) {
        sma->num = 1;
    } else {
        sma->num = num > 0 ? num : DEFAULT_NUMSEG;
    }
#else
    sma->num = num > 0 ? num : DEFAULT_NUMSEG;
#endif

    sma->size = size > 0 ? size : DEFAULT_SEGSIZE;

    sma->segs = (apc_segment_t*) apc_emalloc((sma->num * sizeof(apc_segment_t)) TSRMLS_CC);

    for (i = 0; i < sma->num; i++) {
        sma_header_t*   header;
        block_t     *first, *empty, *last;
        void*       shmaddr;

#if APC_MMAP
        sma->segs[i] = apc_mmap(mask, sma->size TSRMLS_CC);
        if(sma->num != 1) 
			memcpy(&mask[strlen(mask)-6], "XXXXXX", 6);
#else
        sma->segs[i] = apc_shm_attach(
			apc_shm_create(i, sma->size TSRMLS_CC), 
			sma->size TSRMLS_CC
		);
#endif
        
        sma->segs[i].size = sma->size;

        shmaddr = sma->segs[i].shmaddr;

        header = (sma_header_t*) shmaddr;
        CREATE_LOCK(&header->sma_lock);
        header->segsize = sma->size;
        header->avail = sma->size - ALIGNWORD(sizeof(sma_header_t)) - ALIGNWORD(sizeof(block_t)) - ALIGNWORD(sizeof(block_t));

        first = BLOCKAT(ALIGNWORD(sizeof(sma_header_t)));
        first->size = 0;
        first->fnext = ALIGNWORD(sizeof(sma_header_t)) + ALIGNWORD(sizeof(block_t));
        first->fprev = 0;
        first->prev_size = 0;
        SET_CANARY(first);
#if 0
        first->id = -1;
#endif
        empty = BLOCKAT(first->fnext);
        empty->size = header->avail - ALIGNWORD(sizeof(block_t));
        empty->fnext = OFFSET(empty) + empty->size;
        empty->fprev = ALIGNWORD(sizeof(sma_header_t));
        empty->prev_size = 0;
        SET_CANARY(empty);
#if 0
        empty->id = -1;
#endif
        last = BLOCKAT(empty->fnext);
        last->size = 0;
        last->fnext = 0;
        last->fprev =  OFFSET(empty);
        last->prev_size = empty->size;
        SET_CANARY(last);
#if 0
        last->id = -1;
#endif
    }	
}

PHP_APCU_API void apc_sma_api_cleanup(apc_sma_t* sma TSRMLS_DC) {
	uint i;

    assert(sma->initialized);

    for (i = 0; i < sma->num; i++) {
        DESTROY_LOCK(&SMA_LCK(sma, i));
#if APC_MMAP
        apc_unmap(&sma->segs[i] TSRMLS_CC);
#else
        apc_shm_detach(&sma->segs[i] TSRMLS_CC);
#endif
    }
    sma->initialized = 0;

    apc_efree(sma->segs TSRMLS_CC);
}

PHP_APCU_API void* apc_sma_api_malloc_ex(apc_sma_t* sma, zend_ulong n, zend_ulong fragment, zend_ulong* allocated TSRMLS_DC) {
	size_t off;
    uint i;
    int nuked = 0;

restart:
    assert(sma->initialized);

    WLOCK(&SMA_LCK(sma, sma->last));

    off = sma_allocate(SMA_HDR(sma, sma->last), n, fragment, allocated);

    if(off == -1) {
        /* retry failed allocation after we expunge */
        WUNLOCK(&SMA_LCK(sma, sma->last));
		sma->expunge(
			*(sma->data), (n+fragment) TSRMLS_CC);
        WLOCK(&SMA_LCK(sma, sma->last));
        off = sma_allocate(SMA_HDR(sma, sma->last), n, fragment, allocated);
    }

    if (off != -1) {
        void* p = (void *)(SMA_ADDR(sma, sma->last) + off);
        WUNLOCK(&SMA_LCK(sma, sma->last));
#ifdef VALGRIND_MALLOCLIKE_BLOCK
        VALGRIND_MALLOCLIKE_BLOCK(p, n, 0, 0);
#endif
        return p;
    }
    
    WUNLOCK(&SMA_LCK(sma, sma->last));

    for (i = 0; i < sma->num; i++) {
        if (i == sma->last) {
            continue;
        }
        WLOCK(&SMA_LCK(sma, i));
        off = sma_allocate(SMA_HDR(sma, i), n, fragment, allocated);
        if(off == -1) { 
            /* retry failed allocation after we expunge */
            WUNLOCK(&SMA_LCK(sma, i));
			sma->expunge(
				*(sma->data), (n+fragment) TSRMLS_CC);
            WLOCK(&SMA_LCK(sma, i));
            off = sma_allocate(SMA_HDR(sma, i), n, fragment, allocated);
        }
        if (off != -1) {
            void* p = (void *)(SMA_ADDR(sma, i) + off);
            WUNLOCK(&SMA_LCK(sma, i));
            sma->last = i;
#ifdef VALGRIND_MALLOCLIKE_BLOCK
            VALGRIND_MALLOCLIKE_BLOCK(p, n, 0, 0);
#endif
            return p;
        }
        WUNLOCK(&SMA_LCK(sma, i));
    }

    /* I've tried being nice, but now you're just asking for it */
    if(!nuked) {
        sma->expunge(*(sma->data), (n+fragment) TSRMLS_CC);
        nuked = 1;
        goto restart;
    }

    /* now, I've truly and well given up */

    return NULL;
}

PHP_APCU_API void* apc_sma_api_malloc(apc_sma_t* sma, zend_ulong n TSRMLS_DC) 
{
	zend_ulong allocated;
	return apc_sma_api_malloc_ex(
		sma, n, MINBLOCKSIZE, &allocated TSRMLS_CC);
}

PHP_APCU_API void* apc_sma_api_realloc(apc_sma_t* sma, void* p, zend_ulong n TSRMLS_DC) {
	apc_sma_api_free(sma, p TSRMLS_CC);
	return apc_sma_api_malloc(
		sma, n TSRMLS_CC);
}

PHP_APCU_API char* apc_sma_api_strdup(apc_sma_t* sma, const char* s TSRMLS_DC) {
	void* q;
    int len;

    if(!s) {
		return NULL;
	}

    len = strlen(s)+1;
    q = apc_sma_api_malloc(
		sma, len TSRMLS_CC);

    if(!q) {
		return NULL;
	}

    memcpy(q, s, len);
    return q;
}

PHP_APCU_API void apc_sma_api_free(apc_sma_t* sma, void* p TSRMLS_DC) {
	uint i;
    size_t offset;

    if (p == NULL) {
        return;
    }

    assert(sma->initialized);

    for (i = 0; i < sma->num; i++) {
        offset = (size_t)((char *)p - SMA_ADDR(sma, i));
        if (p >= (void*)SMA_ADDR(sma, i) && offset < sma->size) {
            WLOCK(&SMA_LCK(sma, i));
            sma_deallocate(SMA_HDR(sma, i), offset);
            WUNLOCK(&SMA_LCK(sma, i));
#ifdef VALGRIND_FREELIKE_BLOCK
            VALGRIND_FREELIKE_BLOCK(p, 0);
#endif
            return;
        }
    }

    apc_error("apc_sma_free: could not locate address %p" TSRMLS_CC, p);
}

#ifdef APC_MEMPROTECT
PHP_APCU_API void* apc_sma_api_protect(apc_sma_t* sma, void* p) {
	unsigned int i = 0;
    size_t offset;

    if (p == NULL) {
        return NULL;
    }

    if(SMA_RO(sma, sma->last) == NULL) return p;

    offset = (size_t)((char *)p - SMA_ADDR(sma, sma->last));

    if(p >= (void*)SMA_ADDR(sma, sma->last) && offset < sma->size) {
        return SMA_RO(sma, sma->last) + offset;
    }

    for (i = 0; i < sma->num; i++) {
        offset = (size_t)((char *)p - SMA_ADDR(sma, i));
        if (p >= (void*)SMA_ADDR(sma, i) && offset < sma->size) {
            return SMA_RO(sma, i) + offset;
        }
    }

    return NULL;
}

PHP_APCU_API void* apc_sma_api_unprotect(apc_sma_t* sma, void* p){
	unsigned int i = 0;
    size_t offset;

    if (p == NULL) {
        return NULL;
    }

    if(SMA_RO(sma, sma->last) == NULL) return p;

    offset = (size_t)((char *)p - SMA_RO(sma, sma->last));

    if(p >= (void*)SMA_RO(sma, sma->last) && offset < sma->size) {
        return SMA_ADDR(sma, sma->last) + offset;
    }

    for (i = 0; i < sma->num; i++) {
        offset = (size_t)((char *)p - SMA_RO(sma, i));
        if (p >= (void*)SMA_RO(sma, i) && offset < sma->size) {
            return SMA_ADDR(sma, i) + offset;
        }
    }

    return NULL;
}
#else
PHP_APCU_API void* apc_sma_api_protect(apc_sma_t* sma, void *p) { return p; }
PHP_APCU_API void* apc_sma_api_unprotect(apc_sma_t* sma, void *p) { return p; }
#endif

PHP_APCU_API apc_sma_info_t* apc_sma_api_info(apc_sma_t* sma, zend_bool limited TSRMLS_DC) {
	apc_sma_info_t* info;
    apc_sma_link_t** link;
    uint i;
    char* shmaddr;
    block_t* prv;

    if (!sma->initialized) {
        return NULL;
    }

    info = (apc_sma_info_t*) apc_emalloc(sizeof(apc_sma_info_t) TSRMLS_CC);
    info->num_seg = sma->num;
    info->seg_size = sma->size - (ALIGNWORD(sizeof(sma_header_t)) + ALIGNWORD(sizeof(block_t)) + ALIGNWORD(sizeof(block_t)));

    info->list = apc_emalloc(info->num_seg * sizeof(apc_sma_link_t*) TSRMLS_CC);
    for (i = 0; i < sma->num; i++) {
        info->list[i] = NULL;
    }

    if(limited) {
		return info;
	}

    /* For each segment */
    for (i = 0; i < sma->num; i++) {
        RLOCK(&SMA_LCK(sma, i));
        shmaddr = SMA_ADDR(sma, i);
        prv = BLOCKAT(ALIGNWORD(sizeof(sma_header_t)));

        link = &info->list[i];

        /* For each block in this segment */
        while (BLOCKAT(prv->fnext)->fnext != 0) {
            block_t* cur = BLOCKAT(prv->fnext);
			
			CHECK_CANARY(cur);

            *link = apc_emalloc(sizeof(apc_sma_link_t) TSRMLS_CC);
            (*link)->size = cur->size;
            (*link)->offset = prv->fnext;
            (*link)->next = NULL;
            link = &(*link)->next;

            prv = cur;
        }
        RUNLOCK(&SMA_LCK(sma, i));
    }

    return info;
}

PHP_APCU_API void apc_sma_api_free_info(apc_sma_t* sma, apc_sma_info_t* info TSRMLS_DC) {
	int i;

    for (i = 0; i < info->num_seg; i++) {
        apc_sma_link_t* p = info->list[i];
        while (p) {
            apc_sma_link_t* q = p;
            p = p->next;
            apc_efree(q TSRMLS_CC);
        }
    }
    apc_efree(info->list TSRMLS_CC);
    apc_efree(info TSRMLS_CC);
}

PHP_APCU_API zend_ulong apc_sma_api_get_avail_mem(apc_sma_t* sma) {
	size_t avail_mem = 0;
    uint i;

    for (i = 0; i < sma->num; i++) {
        sma_header_t* header = SMA_HDR(sma, i);
        avail_mem += header->avail;
    }
    return avail_mem;
}

PHP_APCU_API zend_bool apc_sma_api_get_avail_size(apc_sma_t* sma, size_t size) {
	uint i;

    for (i = 0; i < sma->num; i++) {
    	sma_header_t* header = SMA_HDR(sma, i);
		if (header->avail > size) {
			return 1;
		}
    }
    return 0;
}

PHP_APCU_API void apc_sma_api_check_integrity(apc_sma_t* sma)
{
    /* dummy */
}

/* {{{ APC SMA */
apc_sma_api_impl(apc_sma, &apc_user_cache, apc_cache_default_expunge); 
/* }}} */

 /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
