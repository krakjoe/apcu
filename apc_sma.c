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

/* True global APC default SMA */
static apc_sma_t apc_sma = {0,}; /* }}} */

enum { 
	DEFAULT_NUMSEG=1, 
	DEFAULT_SEGSIZE=30*1024*1024 };

typedef struct sma_header_t sma_header_t;
struct sma_header_t {
    apc_lock_t sma_lock;    /* segment lock */
    size_t segsize;         /* size of entire segment */
    size_t avail;           /* bytes available (not necessarily contiguous) */
#if ALLOC_DISTRIBUTION
    size_t adist[30];
#endif
};

#define SMA_HDR(sma, i)  ((sma_header_t*)((sma->segs[i]).shmaddr))
#define SMA_ADDR(sma, i) ((char*)(SMA_HDR(sma, i)))
#define SMA_RO(sma, i)   ((char*)(sma->segs[i]).roaddr)
#define SMA_LCK(sma, i)  ((SMA_HDR(sma, i))->sma_lock)

#ifdef APC_SMA_DEBUG
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
#ifdef APC_SMA_DEBUG
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
static APC_HOTSPOT size_t sma_allocate(sma_header_t* header, size_t size, size_t fragment, size_t *allocated)
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
#ifdef APC_SMA_DEBUG
        nxt->id = -1;
#endif
    }

    cur->fnext = 0;

    /* update the block header */
    header->avail -= cur->size;
#if ALLOC_DISTRIBUTION
    header->adist[(int)(log(size)/log(2))]++;
#endif

    SET_CANARY(cur);

#ifdef APC_SMA_DEBUG
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

#ifdef APC_SMA_DEBUG
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

/* {{{ APC SMA */
void apc_sma_init(int numseg, size_t segsize, char *mmap_file_mask TSRMLS_DC) { apc_sma_api_init(&apc_sma, numseg, segsize, mmap_file_mask TSRMLS_CC); }
void apc_sma_cleanup(TSRMLS_D) { apc_sma_api_cleanup(&apc_sma TSRMLS_CC); }
void* apc_sma_malloc_ex(size_t n, size_t fragment, size_t* allocated TSRMLS_DC) { return apc_sma_api_malloc_ex(&apc_sma, n, fragment, allocated TSRMLS_CC); }
void* apc_sma_malloc(size_t n TSRMLS_DC) { size_t allocated; return apc_sma_malloc_ex(n, MINBLOCKSIZE, &allocated TSRMLS_CC); }
void* apc_sma_realloc(void *p, size_t n TSRMLS_DC) { return apc_sma_api_realloc(&apc_sma, p, n TSRMLS_CC); }
char* apc_sma_strdup(const char* s TSRMLS_DC) { return apc_sma_api_strdup(&apc_sma, s TSRMLS_CC); }
void apc_sma_free(void* p TSRMLS_DC) { apc_sma_api_free(&apc_sma, p TSRMLS_CC); }

#ifdef APC_MEMPROTECT
void* apc_sma_protect(void *p) { return apc_sma_api_protect(&apc_sma, p); }
void* apc_sma_unprotect(void *p) { return apc_sma_api_unprotect(&apc_sma, p); }
#else
void* apc_sma_protect(void *p) { return p; }
void* apc_sma_unprotect(void *p) { return p; }
#endif

apc_sma_info_t* apc_sma_info(zend_bool limited TSRMLS_DC) { return apc_sma_api_info(&apc_sma, limited TSRMLS_CC); }
void apc_sma_free_info(apc_sma_info_t* info TSRMLS_DC) { apc_sma_api_free_info(&apc_sma, info TSRMLS_CC);    }
size_t apc_sma_get_avail_mem() { return apc_sma_api_get_avail_mem(&apc_sma); }
zend_bool apc_sma_get_avail_size(size_t size) { return apc_sma_api_get_avail_size(&apc_sma, size); }

#if ALLOC_DISTRIBUTION
size_t *apc_sma_get_alloc_distribution() { return apc_sma_api_get_alloc_distribution(&apc_sma); }
#endif /* }}} */

/* {{{ APC SMA API */
void apc_sma_api_init(apc_sma_t* sma, zend_uint num, zend_ulong size, char *mask TSRMLS_DC) {
	uint i;

    if (sma->initialized) {
        return;
    }

    sma->initialized = 1;

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
#if ALLOC_DISTRIBUTION
        {
           int j;
           for(j=0; j<30; j++) header->adist[j] = 0;
        }
#endif
        first = BLOCKAT(ALIGNWORD(sizeof(sma_header_t)));
        first->size = 0;
        first->fnext = ALIGNWORD(sizeof(sma_header_t)) + ALIGNWORD(sizeof(block_t));
        first->fprev = 0;
        first->prev_size = 0;
        SET_CANARY(first);
#ifdef APC_SMA_DEBUG
        first->id = -1;
#endif
        empty = BLOCKAT(first->fnext);
        empty->size = header->avail - ALIGNWORD(sizeof(block_t));
        empty->fnext = OFFSET(empty) + empty->size;
        empty->fprev = ALIGNWORD(sizeof(sma_header_t));
        empty->prev_size = 0;
        SET_CANARY(empty);
#ifdef APC_SMA_DEBUG
        empty->id = -1;
#endif
        last = BLOCKAT(empty->fnext);
        last->size = 0;
        last->fnext = 0;
        last->fprev =  OFFSET(empty);
        last->prev_size = empty->size;
        SET_CANARY(last);
#ifdef APC_SMA_DEBUG
        last->id = -1;
#endif
    }	
}

void apc_sma_api_cleanup(apc_sma_t* sma TSRMLS_DC) {
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

void* apc_sma_api_malloc_ex(apc_sma_t* sma, zend_ulong n, zend_ulong fragment, zend_ulong* allocated TSRMLS_CC) {
	size_t off;
    uint i;
    int nuked = 0;

restart:
    assert(sma->initialized);
    LOCK(&SMA_LCK(sma, sma->last));

    off = sma_allocate(SMA_HDR(sma, sma->last), n, fragment, allocated);

    if(off == -1) { 
        /* retry failed allocation after we expunge */
        UNLOCK(&SMA_LCK(sma, sma->last));
		apc_user_cache->expunge_cb(
			apc_user_cache, (n+fragment) TSRMLS_CC);
        LOCK(&SMA_LCK(sma, sma->last));
        off = sma_allocate(SMA_HDR(sma, sma->last), n, fragment, allocated);
    }

    if (off != -1) {
        void* p = (void *)(SMA_ADDR(sma, sma->last) + off);
        UNLOCK(&SMA_LCK(sma, sma->last));
#ifdef VALGRIND_MALLOCLIKE_BLOCK
        VALGRIND_MALLOCLIKE_BLOCK(p, n, 0, 0);
#endif
        return p;
    }
    
    UNLOCK(&SMA_LCK(sma, sma->last));

    for (i = 0; i < sma->num; i++) {
        if (i == sma->last) {
            continue;
        }
        LOCK(&SMA_LCK(sma, i));
        off = sma_allocate(SMA_HDR(sma, i), n, fragment, allocated);
        if(off == -1) { 
            /* retry failed allocation after we expunge */
            UNLOCK(&SMA_LCK(sma, i));
			apc_user_cache->expunge_cb(
				apc_user_cache, (n+fragment) TSRMLS_CC);
            LOCK(&SMA_LCK(sma, i));
            off = sma_allocate(SMA_HDR(sma, i), n, fragment, allocated);
        }
        if (off != -1) {
            void* p = (void *)(SMA_ADDR(sma, i) + off);
            UNLOCK(&SMA_LCK(sma, i));
            sma->last = i;
#ifdef VALGRIND_MALLOCLIKE_BLOCK
            VALGRIND_MALLOCLIKE_BLOCK(p, n, 0, 0);
#endif
            return p;
        }
        UNLOCK(&SMA_LCK(sma, i));
    }

    /* I've tried being nice, but now you're just asking for it */
    if(!nuked) {
        apc_user_cache->expunge_cb(apc_user_cache, (n+fragment) TSRMLS_CC);
        nuked = 1;
        goto restart;
    }

    /* now, I've truly and well given up */

    return NULL;
}

void* apc_sma_api_malloc(apc_sma_t* sma, zend_ulong n TSRMLS_DC) 
{
	size_t allocated;
	return apc_sma_api_malloc_ex(
		sma, n, MINBLOCKSIZE, &allocated TSRMLS_CC);
}

void* apc_sma_api_realloc(apc_sma_t* sma, void* p, zend_ulong n TSRMLS_DC) {
	apc_sma_api_free(sma, p TSRMLS_CC);
	return apc_sma_api_malloc(
		sma, n TSRMLS_CC);
}

char* apc_sma_api_strdup(apc_sma_t* sma, const char* s TSRMLS_DC) {
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

void apc_sma_api_free(apc_sma_t* sma, void* p TSRMLS_CC) {
	uint i;
    size_t offset;

    if (p == NULL) {
        return;
    }

    assert(sma->initialized);

    
    for (i = 0; i < sma->num; i++) {
        offset = (size_t)((char *)p - SMA_ADDR(sma, i));
        if (p >= (void*)SMA_ADDR(sma, i) && offset < sma->size) {
            LOCK(&SMA_LCK(sma, i));
            sma_deallocate(SMA_HDR(sma, i), offset);
            UNLOCK(&SMA_LCK(sma, i));
#ifdef VALGRIND_FREELIKE_BLOCK
            VALGRIND_FREELIKE_BLOCK(p, 0);
#endif
            return;
        }
    }

    apc_error("apc_sma_free: could not locate address %p" TSRMLS_CC, p);
}

#ifdef APC_MEMPROTECT
void* apc_sma_api_protect(apc_sma_t* sma, void* p) {
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

void* apc_sma_api_unprotect(apc_sma_t* sma, void* p){
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
void* apc_sma_api_protect(apc_sma_t* sma, void *p) { return p; }
void* apc_sma_api_unprotect(apc_sma_t* sma, void *p) { return p; }
#endif

apc_sma_info_t* apc_sma_api_info(apc_sma_t* sma, zend_bool limited TSRMLS_DC) {
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

#if ALLOC_DISTRIBUTION
            sma_header_t* header = (sma_header_t*) segment->shmaddr;
            memcpy(info->seginfo[i].adist, header->adist, sizeof(size_t) * 30);
#endif

        }
        RUNLOCK(&SMA_LCK(sma, i));
    }

    return info;
}

void apc_sma_api_free_info(apc_sma_t* sma, apc_sma_info_t* info TSRMLS_DC) {
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

zend_ulong apc_sma_api_get_avail_mem(apc_sma_t* sma) {
	size_t avail_mem = 0;
    uint i;

    for (i = 0; i < sma->num; i++) {
        sma_header_t* header = SMA_HDR(sma, i);
        avail_mem += header->avail;
    }
    return avail_mem;
}

zend_bool apc_sma_api_get_avail_size(apc_sma_t* sma, size_t size) {
	uint i;

    for (i = 0; i < sma->num; i++) {
    	sma_header_t* header = SMA_HDR(sma, i);
		if (header->avail > size) {
			return 1;
		}
    }
    return 0;
}
#if ALLOC_DISTRIBUTION
zend_ulong *apc_sma_api_get_alloc_distribution(apc_sma_t* sma) {
	/* ?? */
	sma_header_t* header = (sma_header_t*) segment->sma_shmaddr;
    return header->adist;
}
#endif /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
