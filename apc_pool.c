/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Yahoo! Inc. in 2008.

   Future revisions and derivatives of this source code must acknowledge
   Yahoo! Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_pool.c 327069 2012-08-12 08:56:55Z laruence $ */


#include "apc_pool.h"
#include <assert.h>

#ifdef APC_POOL_DEBUG
# ifdef HAVE_VALGRIND_MEMCHECK_H
#  include <valgrind/memcheck.h>
# endif
#endif

/* {{{ forward references */
static apc_pool* apc_unpool_create(apc_pool_type type, apc_malloc_t, apc_free_t, apc_protect_t, apc_unprotect_t TSRMLS_DC);
static apc_pool* apc_realpool_create(apc_pool_type type, apc_malloc_t, apc_free_t, apc_protect_t, apc_unprotect_t TSRMLS_DC);
/* }}} */

/* {{{ apc_pool_create */
PHP_APCU_API apc_pool* apc_pool_create(apc_pool_type pool_type,
                                       apc_malloc_t allocate, 
                                       apc_free_t deallocate,
                                       apc_protect_t protect,
                                       apc_unprotect_t unprotect TSRMLS_DC) 
{
    if(pool_type == APC_UNPOOL) {
        return apc_unpool_create(pool_type, allocate, deallocate, protect, unprotect TSRMLS_CC);
    }

    return apc_realpool_create(pool_type, allocate, deallocate, protect,  unprotect TSRMLS_CC);
}
/* }}} */

/* {{{ apc_pool_destroy */
PHP_APCU_API void apc_pool_destroy(apc_pool *pool TSRMLS_DC)
{
    apc_free_t deallocate = pool->deallocate;
    apc_pcleanup_t cleanup = pool->cleanup;

    cleanup(pool TSRMLS_CC);
    deallocate(pool TSRMLS_CC);
}
/* }}} */

/* {{{ apc_unpool implementation */

typedef struct _apc_unpool apc_unpool;

struct _apc_unpool {
    apc_pool parent;
    /* apc_unpool is a lie! */
};

static void* apc_unpool_alloc(apc_pool* pool, 
                              size_t size TSRMLS_DC) 
{
    apc_unpool *upool = (apc_unpool*)pool;

    apc_malloc_t allocate = upool->parent.allocate;

    upool->parent.size += size;
    upool->parent.used += size;

    return allocate(size TSRMLS_CC);
}

static void apc_unpool_free(apc_pool* pool, 
                            void *ptr TSRMLS_DC)
{
    apc_unpool *upool = (apc_unpool*) pool;

    apc_free_t deallocate = upool->parent.deallocate;

    deallocate(ptr TSRMLS_CC);
}

static void apc_unpool_cleanup(apc_pool* pool TSRMLS_DC)
{
}

static apc_pool* apc_unpool_create(apc_pool_type type, 
                                   apc_malloc_t allocate, 
                                   apc_free_t deallocate,
                                   apc_protect_t protect, 
                                   apc_unprotect_t unprotect TSRMLS_DC)
{
    apc_unpool* upool = allocate(sizeof(apc_unpool) TSRMLS_CC);

    if (!upool) {
        return NULL;
    }

    upool->parent.type = type;
    upool->parent.allocate = allocate;
    upool->parent.deallocate = deallocate;

    upool->parent.protect = protect;
    upool->parent.unprotect = unprotect;

    upool->parent.palloc = apc_unpool_alloc;
    upool->parent.pfree  = apc_unpool_free;

    upool->parent.cleanup = apc_unpool_cleanup;

    upool->parent.used = 0;
    upool->parent.size = 0;

    return &(upool->parent);
}
/* }}} */

/*{{{ apc_realpool implementation */

/* {{{ typedefs */
typedef struct _pool_block
{
    size_t              avail;
    size_t              capacity;
    unsigned char       *mark;
    struct _pool_block  *next;
    unsigned             :0; /* this should align to word */
    /* data comes here */
}pool_block;

/*
   parts in ? are optional and turned on for fun, memory loss,
   and for something else that I forgot about ... ah, debugging

                 |--------> data[]         |<-- non word boundary (too)
   +-------------+--------------+-----------+-------------+-------------->>>
   | pool_block  | ?sizeinfo<1> | block<1>  | ?redzone<1> | ?sizeinfo<2>
   |             |  (size_t)    |           | padded left |
   +-------------+--------------+-----------+-------------+-------------->>>
 */

typedef struct _apc_realpool apc_realpool;

struct _apc_realpool
{
    struct _apc_pool parent;

    size_t     dsize;
    void       *owner;

    unsigned long count;

    pool_block *head;
    pool_block first; 
};

/* }}} */

/* {{{ redzone code */
static const unsigned char decaff[] =  {
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
    0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad
};

/* a redzone is at least 4 (0xde,0xca,0xc0,0xff) bytes */
#define REDZONE_SIZE(size) \
    ((ALIGNWORD((size)) > ((size) + 4)) ? \
        (ALIGNWORD((size)) - (size)) : /* does not change realsize */\
        ALIGNWORD((size)) - (size) + ALIGNWORD((sizeof(char)))) /* adds 1 word to realsize */

#define SIZEINFO_SIZE ALIGNWORD(sizeof(size_t))

#define MARK_REDZONE(block, redsize) do {\
       memcpy(block, decaff, redsize );\
    } while(0)

#define CHECK_REDZONE(block, redsize) (memcmp(block, decaff, redsize) == 0)

/* }}} */

#define INIT_POOL_BLOCK(rpool, entry, size) do {\
    (entry)->avail = (entry)->capacity = (size);\
    (entry)->mark =  ((unsigned char*)(entry)) + ALIGNWORD(sizeof(pool_block));\
    (entry)->next = (rpool)->head;\
    (rpool)->head = (entry);\
} while(0)

/* {{{ create_pool_block */
static pool_block* create_pool_block(apc_realpool *rpool, 
                                     size_t size TSRMLS_DC)
{
    apc_malloc_t allocate = rpool->parent.allocate;

    size_t realsize = sizeof(pool_block) + ALIGNWORD(size);

    pool_block* entry = allocate(realsize TSRMLS_CC);

    if (!entry) {
        return NULL;
    }

    INIT_POOL_BLOCK(rpool, entry, size);
    
    rpool->parent.size += realsize;

    rpool->count++;

    return entry;
}
/* }}} */

/* {{{ apc_realpool_alloc */
static void* apc_realpool_alloc(apc_pool *pool, 
                                size_t size TSRMLS_DC)
{
    apc_realpool *rpool = (apc_realpool*)pool;
    unsigned char *p = NULL;
    size_t realsize = ALIGNWORD(size);
    size_t poolsize;
    unsigned char *redzone  = NULL;
    size_t redsize  = 0;
    size_t *sizeinfo= NULL;
    pool_block *entry = NULL;
    unsigned long i;
    
    if(APC_POOL_HAS_REDZONES(pool)) {
        redsize = REDZONE_SIZE(size); /* redsize might be re-using word size padding */
        realsize = size + redsize;    /* recalculating realsize */
    } else {
        redsize = realsize - size; /* use padding space */
    }

    if(APC_POOL_HAS_SIZEINFO(pool)) {
        realsize += ALIGNWORD(sizeof(size_t));
    }

    /* minimize look-back, a value of 8 seems to give similar fill-ratios (+2%)
     * as looping through the entire list. And much faster in allocations. */
    for(entry = rpool->head, i = 0; entry != NULL && (i < 8); entry = entry->next, i++) {
        if(entry->avail >= realsize) {
            goto found;
        }
    }

    /* upgrade the pool type to reduce overhead */
    if(rpool->count > 4 && rpool->dsize < 4096) {
        rpool->dsize = 4096;
    } else if(rpool->count > 8 && rpool->dsize < 8192) {
        rpool->dsize = 8192;
    }

    poolsize = ALIGNSIZE(realsize, rpool->dsize);

    entry = create_pool_block(rpool, poolsize TSRMLS_CC);

    if(!entry) {
        return NULL;
    }

found:
    p = entry->mark;

    if(APC_POOL_HAS_SIZEINFO(pool)) {
        sizeinfo = (size_t*)p;
        p += SIZEINFO_SIZE;
        *sizeinfo = size;
    }

    redzone = p + size;

    if(APC_POOL_HAS_REDZONES(pool)) {
        MARK_REDZONE(redzone, redsize);
    }

#ifdef VALGRIND_MAKE_MEM_NOACCESS
    if(redsize != 0) {
        VALGRIND_MAKE_MEM_NOACCESS(redzone, redsize);
    }
#endif

    entry->avail -= realsize;
    entry->mark  += realsize;
    pool->used   += realsize;

#ifdef VALGRIND_MAKE_MEM_UNDEFINED
    /* need to write before reading data off this */
    VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif

    return (void*)p;
}
/* }}} */

/* {{{ apc_realpool_check_integrity */
/*
 * Checking integrity at runtime, does an
 * overwrite check only when the sizeinfo
 * is set.
 *
 * Marked as used in gcc, so that this function
 * is accessible from gdb, eventhough it is never
 * used in code in non-debug builds.
 */
static APC_USED int apc_realpool_check_integrity(apc_realpool *rpool) 
{
    apc_pool *pool = &(rpool->parent); 
    pool_block *entry;
    size_t *sizeinfo = NULL;
    unsigned char *start;
    size_t realsize;
    unsigned char   *redzone;
    size_t redsize;

    for(entry = rpool->head; entry != NULL; entry = entry->next) {
        start = (unsigned char *)entry + ALIGNWORD(sizeof(pool_block));
        if((entry->mark - start) != (entry->capacity - entry->avail)) {
            return 0;
        }
    }

    if(!APC_POOL_HAS_REDZONES(pool) ||
        !APC_POOL_HAS_SIZEINFO(pool)) {
        (void)pool; /* remove unused warning */
        return 1;
    }

    for(entry = rpool->head; entry != NULL; entry = entry->next) {
        start = (unsigned char *)entry + ALIGNWORD(sizeof(pool_block));

        while(start < entry->mark) {
            sizeinfo = (size_t*)start;
            /* redzone starts where real data ends, in a non-word boundary
             * redsize is at least 4 bytes + whatever's needed to make it
             * to another word boundary.
             */
            redzone = start + SIZEINFO_SIZE + (*sizeinfo);
            redsize = REDZONE_SIZE(*sizeinfo);
#ifdef VALGRIND_MAKE_MEM_DEFINED
            VALGRIND_MAKE_MEM_DEFINED(redzone, redsize);
#endif
            if(!CHECK_REDZONE(redzone, redsize))
            {
                /*
                fprintf(stderr, "Redzone check failed for %p\n", 
                                start + ALIGNWORD(sizeof(size_t)));*/
                return 0;
            }
#ifdef VALGRIND_MAKE_MEM_NOACCESS
            VALGRIND_MAKE_MEM_NOACCESS(redzone, redsize);
#endif
            realsize = SIZEINFO_SIZE + *sizeinfo + redsize;
            start += realsize;
        }
    }

    return 1;
}
/* }}} */

/* {{{ apc_realpool_free */
/*
 * free does not do anything
 */
static void apc_realpool_free(apc_pool *pool, 
                              void *p TSRMLS_DC)
{
}
/* }}} */

/* {{{ apc_realpool_cleanup */
static void apc_realpool_cleanup(apc_pool *pool TSRMLS_DC) 
{
    pool_block *entry;
    pool_block *tmp;
    apc_realpool *rpool = (apc_realpool*)pool;
    apc_free_t deallocate = pool->deallocate;

    assert(apc_realpool_check_integrity(rpool)!=0);

    entry = rpool->head;

    while(entry->next != NULL) {
        tmp = entry->next;
        deallocate(entry TSRMLS_CC);
        entry = tmp;
    }
}
/* }}} */

/* {{{ apc_realpool_create */
static apc_pool* apc_realpool_create(apc_pool_type type, 
                                     apc_malloc_t allocate, 
                                     apc_free_t deallocate, 
                                     apc_protect_t protect, 
                                     apc_unprotect_t unprotect TSRMLS_DC)
{

    size_t dsize = 0;
    apc_realpool *rpool;

    switch(type & APC_POOL_SIZE_MASK) {
        case APC_SMALL_POOL:
            dsize = 512;
            break;

        case APC_LARGE_POOL:
            dsize = 8192;
            break;

        case APC_MEDIUM_POOL:
            dsize = 4096;
            break;

        default:
            return NULL;
    }

    rpool = (apc_realpool*)allocate((sizeof(apc_realpool) + ALIGNWORD(dsize)) TSRMLS_CC);

    if(!rpool) {
        return NULL;
    }

    rpool->parent.type = type;

    rpool->parent.allocate = allocate;
    rpool->parent.deallocate = deallocate;

    rpool->parent.size = sizeof(apc_realpool) + ALIGNWORD(dsize);

    rpool->parent.palloc = apc_realpool_alloc;
    rpool->parent.pfree  = apc_realpool_free;

    rpool->parent.protect = protect;
    rpool->parent.unprotect = unprotect;

    rpool->parent.cleanup = apc_realpool_cleanup;

    rpool->dsize = dsize;
    rpool->head = NULL;
    rpool->count = 0;

    INIT_POOL_BLOCK(rpool, &(rpool->first), dsize);

    return &(rpool->parent);
}


/* }}} */

/* }}} */

/* {{{ apc_pool_init */
PHP_APCU_API void apc_pool_init()
{
    /* put all ye sanity checks here */
    assert(sizeof(decaff) > REDZONE_SIZE(ALIGNWORD(sizeof(char))));
    assert(sizeof(pool_block) == ALIGNWORD(sizeof(pool_block)));
#if APC_POOL_DEBUG
    assert((APC_POOL_SIZE_MASK & (APC_POOL_SIZEINFO | APC_POOL_REDZONES)) == 0);
#endif
}
/* }}} */

/* {{{ apc_pstrdup */
PHP_APCU_API void* APC_ALLOC apc_pstrdup(const char* s, 
                            apc_pool* pool TSRMLS_DC)
{
    return s != NULL ? apc_pmemcpy(s, (strlen(s) + 1), pool TSRMLS_CC) : NULL;
}
/* }}} */

/* {{{ apc_pmemcpy */
PHP_APCU_API void* APC_ALLOC apc_pmemcpy(const void* p, 
                            size_t n, 
                            apc_pool* pool TSRMLS_DC)
{
    void* q;

    if (p != NULL && (q = pool->palloc(pool, n TSRMLS_CC)) != NULL) {
        memcpy(q, p, n);
        return q;
    }
    return NULL;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
