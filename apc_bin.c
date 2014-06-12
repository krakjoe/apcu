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
  | Authors: Brian Shire <shire@php.net>                                 |
  |          Xinchen Hui <laruence@php.net>                              |
  +----------------------------------------------------------------------+

 */

/* $Id: apc_bin.c 328828 2012-12-18 15:18:39Z remi $ */

/* Creates an architecture specific binary output to a string or file containing
 * the current cache contents.  This is accomplished via the apc_copy_* functions and 
 * "swizzling" pointers to achieve position independence.
 */

#include "apc_globals.h"
#include "apc_bin.h"
#include "apc_php.h"
#include "apc_sma.h"
#include "apc_pool.h"
#include "apc_cache.h"

#include "ext/standard/md5.h"

/* in apc_cache.c */
extern zval* apc_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC);

#define APC_BINDUMP_DEBUG 0

#if APC_BINDUMP_DEBUG

#define SWIZZLE(bd, ptr)  \
    do { \
        if((ptrdiff_t)bd < (ptrdiff_t)ptr && (size_t)ptr < ((size_t)bd + bd->size)) { \
            printf("SWIZZLE: %x ~> ", ptr); \
            ptr = (void*)((ptrdiff_t)(ptr) - (ptrdiff_t)(bd)); \
            printf("%x in %s on line %d", ptr, __FILE__, __LINE__); \
        } else if((ptrdiff_t)ptr > bd->size) { /* not swizzled */ \
            apc_error("pointer to be swizzled is not within allowed memory range! (%x < %x < %x) in %s on %d" TSRMLS_CC, (ptrdiff_t)bd, ptr, ((size_t)bd + bd->size), __FILE__, __LINE__); \
            return; \
        } \
        printf("\n"); \
    } while(0);

#define UNSWIZZLE(bd, ptr)  \
    do { \
      printf("UNSWIZZLE: %x -> ", ptr); \
      ptr = (void*)((ptrdiff_t)(ptr) + (ptrdiff_t)(bd)); \
      printf("%x in %s on line %d \n", ptr, __FILE__, __LINE__); \
    } while(0);

#else    /* !APC_BINDUMP_DEBUG */

#define SWIZZLE(bd, ptr) \
    do { \
        if((ptrdiff_t)bd < (ptrdiff_t)ptr && (size_t)ptr < ((size_t)bd + bd->size)) { \
            ptr = (void*)((ptrdiff_t)(ptr) - (ptrdiff_t)(bd)); \
        } else if((size_t)ptr > bd->size) { /* not swizzled */ \
            apc_error("pointer to be swizzled is not within allowed memory range! (%x < %x < %x) in %s on %d" TSRMLS_CC, (ptrdiff_t)bd, ptr, ((size_t)bd + bd->size), __FILE__, __LINE__); \
            return NULL; \
        } \
    } while(0);

#define UNSWIZZLE(bd, ptr) \
    do { \
      ptr = (void*)((ptrdiff_t)(ptr) + (ptrdiff_t)(bd)); \
    } while(0);

#endif

static void *apc_bd_alloc(size_t size TSRMLS_DC);
static void apc_bd_free(void *ptr TSRMLS_DC);
static void *apc_bd_alloc_ex(void *ptr_new, size_t size TSRMLS_DC);

typedef void (*apc_swizzle_cb_t)(apc_bd_t *bd, zend_llist *ll, void *ptr TSRMLS_DC);

#if APC_BINDUMP_DEBUG
#define apc_swizzle_ptr(bd, ctxt, ll, ptr) _apc_swizzle_ptr(bd, ctxt, ll, (void*)ptr, __FILE__, __LINE__ TSRMLS_CC)
#else
#define apc_swizzle_ptr(bd, ctxt, ll, ptr) _apc_swizzle_ptr(bd, ctxt, ll, (void*)ptr, NULL, 0 TSRMLS_CC)
#endif

static void _apc_swizzle_ptr(apc_bd_t *bd, apc_context_t* ctxt, zend_llist *ll, void **ptr, const char* file, int line TSRMLS_DC);
static void apc_swizzle_hashtable(apc_bd_t *bd, apc_context_t* ctxt, zend_llist *ll, HashTable *ht, apc_swizzle_cb_t swizzle_cb, int is_ptr TSRMLS_DC);
static void apc_swizzle_zval(apc_bd_t *bd, apc_context_t* ctxt, zend_llist *ll, zval *zv TSRMLS_DC);

static apc_bd_t* apc_swizzle_bd(apc_bd_t* bd, zend_llist *ll TSRMLS_DC);
static int apc_unswizzle_bd(apc_bd_t *bd, int flags TSRMLS_DC);

/* {{{ apc_bd_alloc
 *  callback for copy_* functions */
static void *apc_bd_alloc(size_t size TSRMLS_DC) {
    return apc_bd_alloc_ex(NULL, size TSRMLS_CC);
} /* }}} */

/* {{{ apc_bd_free
 *  callback for copy_* functions */
static void apc_bd_free(void *ptr TSRMLS_DC) {
    size_t *size;
    if(zend_hash_index_find(&APCG(apc_bd_alloc_list), (ulong)ptr, (void**)&size) == FAILURE) {
        apc_error("apc_bd_free could not free pointer (not found in list: %x)" TSRMLS_CC, ptr);
        return;
    }
    APCG(apc_bd_alloc_ptr) = (void*)((size_t)APCG(apc_bd_alloc_ptr) - *size);
    zend_hash_index_del(&APCG(apc_bd_alloc_list), (ulong)ptr);
} /* }}} */

/* {{{ apc_bd_alloc_ex
 *  set ranges or allocate a block of data from an already (e)malloc'd range.
 *  if ptr_new is not NULL, it will reset the pointer to start at ptr_new,
 *  with a range of size.  If ptr_new is NULL, returns the next available
 *  block of given size.
 */
static void *apc_bd_alloc_ex(void *ptr_new, size_t size TSRMLS_DC) {
    void *rval;

    rval = APCG(apc_bd_alloc_ptr);
    if(ptr_new != NULL) {  /* reset ptrs */
      APCG(apc_bd_alloc_ptr) = ptr_new;
      APCG(apc_bd_alloc_ubptr) = (void*)((unsigned char *) ptr_new + size);
    } else {  /* alloc block */
      APCG(apc_bd_alloc_ptr) = (void*)((size_t)APCG(apc_bd_alloc_ptr) + size);
#if APC_BINDUMP_DEBUG
      apc_notice("apc_bd_alloc: rval: 0x%x  ptr: 0x%x  ubptr: 0x%x  size: %d" TSRMLS_CC, rval, APCG(apc_bd_alloc_ptr), APCG(apc_bd_alloc_ubptr), size);
#endif
      if(APCG(apc_bd_alloc_ptr) > APCG(apc_bd_alloc_ubptr)) {
          apc_error("Exceeded bounds check in apc_bd_alloc_ex by %d bytes." TSRMLS_CC, (unsigned char *) APCG(apc_bd_alloc_ptr) - (unsigned char *) APCG(apc_bd_alloc_ubptr));
          return NULL;
      }
      zend_hash_index_update(&APCG(apc_bd_alloc_list), (ulong)rval, &size, sizeof(size_t), NULL);
    }

    return rval;
} /* }}} */

/* {{{ _apc_swizzle_ptr */
static void _apc_swizzle_ptr(apc_bd_t *bd, apc_context_t* ctxt, zend_llist *ll, void **ptr, const char* file, int line TSRMLS_DC) {
    if(*ptr) {
        if((ptrdiff_t)bd < (ptrdiff_t)*ptr && (size_t)*ptr < ((size_t)bd + bd->size)) {
            zend_llist_add_element(ll, &ptr);
#if APC_BINDUMP_DEBUG
            printf("[%06d] apc_swizzle_ptr: %x -> %x ", zend_llist_count(ll), ptr, *ptr);
            printf(" in %s on line %d \n", file, line);
#endif
        } else if((size_t)ptr > bd->size) {
            apc_error("pointer to be swizzled is not within allowed memory range! (%x < %x < %x) in %s on %d" TSRMLS_CC, (ptrdiff_t)bd, *ptr, ((ptrdiff_t)bd + bd->size), file, line); \
            return;
        }
    }
} /* }}} */

/* {{{ apc_swizzle_hashtable */
static void apc_swizzle_hashtable(apc_bd_t *bd, apc_context_t* ctxt, zend_llist *ll, HashTable *ht, apc_swizzle_cb_t swizzle_cb, int is_ptr TSRMLS_DC) {
    uint i;
    Bucket **bp, **bp_prev;

    bp = &ht->pListHead;
    while(*bp) {
        bp_prev = bp;
        bp = &(*bp)->pListNext;
        if(is_ptr) {
            swizzle_cb(bd, ll, *(void**)(*bp_prev)->pData TSRMLS_CC);
            apc_swizzle_ptr(bd, ctxt, ll, (*bp_prev)->pData);
        } else {
            swizzle_cb(bd, ll, (void**)(*bp_prev)->pData TSRMLS_CC);
        }
#ifdef ZEND_ENGINE_2_4
        if ((*bp_prev)->nKeyLength) {
            if (IS_INTERNED((*bp_prev)->arKey)) {
                /* we should dump that internal string out */
                char *tmp = apc_bd_alloc((*bp_prev)->nKeyLength TSRMLS_CC);
                memcpy(tmp, (*bp_prev)->arKey, (*bp_prev)->nKeyLength);
                (*bp_prev)->arKey = tmp;
            }
            apc_swizzle_ptr(bd, ctxt, ll, &(*bp_prev)->arKey);
        }
#endif
        apc_swizzle_ptr(bd, ctxt, ll, &(*bp_prev)->pData);
        if((*bp_prev)->pDataPtr) {
            apc_swizzle_ptr(bd, ctxt, ll, &(*bp_prev)->pDataPtr);
        }
        if((*bp_prev)->pListLast) {
            apc_swizzle_ptr(bd, ctxt, ll, &(*bp_prev)->pListLast);
        }
        if((*bp_prev)->pNext) {
            apc_swizzle_ptr(bd, ctxt, ll, &(*bp_prev)->pNext);
        }
        if((*bp_prev)->pLast) {
            apc_swizzle_ptr(bd, ctxt, ll, &(*bp_prev)->pLast);
        }
        apc_swizzle_ptr(bd, ctxt, ll, bp_prev);
    }
    for(i=0; i < ht->nTableSize; i++) {
        if(ht->arBuckets[i]) {
            apc_swizzle_ptr(bd, ctxt, ll, &ht->arBuckets[i]);
        }
    }
    apc_swizzle_ptr(bd, ctxt, ll, &ht->pListTail);

    apc_swizzle_ptr(bd, ctxt, ll, &ht->arBuckets);
} /* }}} */

/* {{{ apc_swizzle_zval */
static void apc_swizzle_zval(apc_bd_t *bd, apc_context_t* ctxt, zend_llist *ll, zval *zv TSRMLS_DC) {

    if(ctxt->copied.nTableSize) {
        if(zend_hash_index_exists(&ctxt->copied, (ulong)zv)) {
          return;
        }
        zend_hash_index_update(&ctxt->copied, (ulong)zv, (void**)&zv, sizeof(zval*), NULL);
    }

    switch(Z_TYPE_P(zv) & IS_CONSTANT_TYPE_MASK) {
        case IS_NULL:
        case IS_LONG:
        case IS_DOUBLE:
        case IS_BOOL:
        case IS_RESOURCE:
            /* nothing to do */
            break;
        case IS_CONSTANT:
        case IS_STRING:
            apc_swizzle_ptr(bd, ctxt, ll, &zv->value.str.val);
            break;
        case IS_ARRAY:
            apc_swizzle_hashtable(bd, ctxt, ll, zv->value.ht, (apc_swizzle_cb_t)apc_swizzle_zval, 1 TSRMLS_CC);
            apc_swizzle_ptr(bd, ctxt, ll, &zv->value.ht);
            break;
        case IS_OBJECT:
            break;
        default:
            assert(0); /* shouldn't happen */
    }
} /* }}} */

/* {{{ apc_swizzle_bd */
static apc_bd_t* apc_swizzle_bd(apc_bd_t* bd, zend_llist *ll TSRMLS_DC) {
    unsigned int i;
    int count;
    PHP_MD5_CTX context;
    unsigned char digest[16];
    register php_uint32 crc;
    void ***ptr;
    void ***ptr_list;

    count = zend_llist_count(ll);
    ptr_list = emalloc(count * sizeof(void**));
    ptr = zend_llist_get_first(ll);
    for(i=0; i < count; i++) {
#if APC_BINDUMP_DEBUG
        printf("[%06d] ", i+1);
#endif
        SWIZZLE(bd, **ptr); /* swizzle ptr */
        if((ptrdiff_t)bd < (ptrdiff_t)*ptr && (size_t)*ptr < ((size_t)bd + bd->size)) {  /* exclude ptrs that aren't actually included in the ptr list */
#if APC_BINDUMP_DEBUG
            printf("[------] ");
#endif
            SWIZZLE(bd, *ptr);  /* swizzle ptr list */
            ptr_list[i] = *ptr;
        }
        ptr = zend_llist_get_next(ll);
    }
    SWIZZLE(bd, bd->entries);

    if(count > 0) {
        bd = erealloc(bd, bd->size + (count * sizeof(void**)));
        bd->num_swizzled_ptrs = count;
        bd->swizzled_ptrs = (void***)((unsigned char *)bd + bd->size -2);   /* extra -1 for null termination */
        bd->size += count * sizeof(void**);
        memcpy(bd->swizzled_ptrs, ptr_list, count * sizeof(void**));
        SWIZZLE(bd, bd->swizzled_ptrs);
    } else {
        bd->num_swizzled_ptrs = 0;
        bd->swizzled_ptrs = NULL;
    }
    ((unsigned char*)bd)[((bd->size >= 1) ? (bd->size-1) : 0)] = 0;  /* silence null termination for zval strings */
    efree(ptr_list);
    bd->swizzled = 1;

    /* Generate MD5/CRC32 checksum */
    memset(bd->md5, 0, 16);
    bd->crc=0;

    PHP_MD5Init(&context);
    PHP_MD5Update(&context, (const unsigned char*)bd, bd->size);
    PHP_MD5Final(digest, &context);
    crc = apc_crc32((unsigned char*)bd, bd->size);

    memmove(bd->md5, digest, 16);
    bd->crc = crc;

    return bd;
} /* }}} */

/* {{{ apc_unswizzle_bd */
static int apc_unswizzle_bd(apc_bd_t *bd, int flags TSRMLS_DC) {
    unsigned int i;
    unsigned char md5_orig[16];
    unsigned char digest[16];
    PHP_MD5_CTX context;
    register php_uint32 crc;
    php_uint32 crc_orig;

    /* Verify the md5 or crc32 before we unswizzle */
    memmove(md5_orig, bd->md5, 16);
    memset(bd->md5, 0, 16);
    crc_orig = bd->crc;
    bd->crc=0;

    if(flags & APC_BIN_VERIFY_MD5) {
        PHP_MD5Init(&context);
        PHP_MD5Update(&context, (const unsigned char*)bd, bd->size);
        PHP_MD5Final(digest, &context);
        if(memcmp(md5_orig, digest, 16)) {
            apc_error("MD5 checksum of binary dump failed." TSRMLS_CC);
            return -1;
        }
    }
    if(flags & APC_BIN_VERIFY_CRC32) {
        crc = apc_crc32((unsigned char*)bd, bd->size);
        if(crc_orig != crc) {
            apc_error("CRC32 checksum of binary dump failed." TSRMLS_CC);
            return -1;
        }
    }
    memcpy(bd->md5, md5_orig, 16); /* add back md5 checksum */
    bd->crc = crc_orig;

    UNSWIZZLE(bd, bd->entries);
    UNSWIZZLE(bd, bd->swizzled_ptrs);
    for(i=0; i < bd->num_swizzled_ptrs; i++) {
        if(bd->swizzled_ptrs[i]) {
            UNSWIZZLE(bd, bd->swizzled_ptrs[i]);
            if(*bd->swizzled_ptrs[i] && (*bd->swizzled_ptrs[i] < (void*)bd)) {
                UNSWIZZLE(bd, *bd->swizzled_ptrs[i]);
            }
        }
    }

    bd->swizzled=0;

    return 0;
} /* }}} */

/* {{{ apc_bin_checkfilter */
static int apc_bin_checkfilter(HashTable *filter, const char *key, uint key_len) {
    zval **zptr;

    if(filter == NULL) {
        return 1;
    }

    if(zend_hash_find(filter, (char*)key, key_len, (void**)&zptr) == SUCCESS) {
        if(Z_TYPE_PP(zptr) == IS_LONG && Z_LVAL_PP(zptr) == 0) {
            return 0;
        }
    } else {
        return 0;
    }


    return 1;
} /* }}} */

/* {{{ apc_bin_dump */
PHP_APCU_API apc_bd_t* apc_bin_dump(apc_cache_t* cache, HashTable *user_vars TSRMLS_DC) {
    apc_cache_slot_t *sp;
    apc_bd_entry_t *ep;
    int i, count=0;
    apc_bd_t *bd;
    zend_llist ll;
    size_t size=0;
    apc_context_t ctxt;
    void *pool_ptr;

    zend_llist_init(&ll, sizeof(void*), NULL, 0);
    zend_hash_init(&APCG(apc_bd_alloc_list), 0, NULL, NULL, 0);

    /* flip the hash for faster filter checking */
    user_vars = apc_flip_hash(user_vars);

    APC_RLOCK(cache->header);

    /* get size and entry counts */
    for(i=0; i < cache->nslots; i++) {
        sp = cache->slots[i];
        for(; sp != NULL; sp = sp->next) {
            if(apc_bin_checkfilter(user_vars, sp->key.str, sp->key.len)) {
                size += sizeof(apc_bd_entry_t*) + sizeof(apc_bd_entry_t);
                size += sp->value->mem_size - (sizeof(apc_cache_entry_t));
                count++;
            }
        }
    }

    size += sizeof(apc_bd_t) +1;  /* +1 for null termination */
    bd = emalloc(size);
    bd->size = (unsigned int)size;
    pool_ptr = emalloc(sizeof(apc_pool));
    apc_bd_alloc_ex(pool_ptr, sizeof(apc_pool) TSRMLS_CC);

	ctxt.serializer = cache->serializer;
    ctxt.pool = apc_pool_create(APC_UNPOOL, apc_bd_alloc, apc_bd_free, NULL, NULL TSRMLS_CC);  /* ideally the pool wouldn't be alloc'd as part of this */
    if (!ctxt.pool) { /* TODO need to cleanup */
        apc_warning("Unable to allocate memory for pool." TSRMLS_CC);
        return NULL;
    }

    ctxt.copy = APC_COPY_OTHER; /* avoid stupid ALLOC_ZVAL calls here, hack */
    apc_bd_alloc_ex((void*)((size_t)bd + sizeof(apc_bd_t)), bd->size - sizeof(apc_bd_t) -1 TSRMLS_CC);
    bd->num_entries = count;
    bd->entries = apc_bd_alloc_ex(NULL, sizeof(apc_bd_entry_t) * count TSRMLS_CC);

    /* User entries */
    zend_hash_init(&ctxt.copied, 0, NULL, NULL, 0);
    count = 0;
    for(i=0; i < cache->nslots; i++) {
        sp = cache->slots[i];
        for(; sp != NULL; sp = sp->next) {
            if(apc_bin_checkfilter(user_vars, sp->key.str, sp->key.len)) {
                ep = &bd->entries[count];
				
				/* copy key with current pool */
				ep->key.str = apc_pmemcpy(sp->key.str, sp->key.len, ctxt.pool TSRMLS_CC);
				ep->key.len = sp->key.len;

                if ((Z_TYPE_P(sp->value->val) == IS_ARRAY && cache->serializer)
                        || Z_TYPE_P(sp->value->val) == IS_OBJECT) {
                    /* avoiding hash copy, hack */
                    uint type = Z_TYPE_P(sp->value->val);
                    Z_TYPE_P(sp->value->val) = IS_STRING;
                    ep->val.val = apc_copy_zval(NULL, sp->value->val, &ctxt TSRMLS_CC);
					
                    Z_TYPE_P(ep->val.val) = IS_OBJECT;
                    sp->value->val->type = type;
                } else if (Z_TYPE_P(sp->value->val) == IS_ARRAY && !cache->serializer) {
                    /* this is a little complicated, we have to unserialize it first, then serialize it again */
                    zval *garbage;
                    ctxt.copy = APC_COPY_OUT;
                    garbage = apc_copy_zval(NULL, sp->value->val, &ctxt TSRMLS_CC);
                    ctxt.copy = APC_COPY_IN;
                    ep->val.val = apc_copy_zval(NULL, garbage, &ctxt TSRMLS_CC);
                    ep->val.val->type = IS_OBJECT;
                    /* a memleak can not be avoided: zval_ptr_dtor(&garbage); */
                    ctxt.copy = APC_COPY_OTHER;
                } else {
                    ep->val.val = apc_copy_zval(NULL, sp->value->val, &ctxt TSRMLS_CC);
                }
                ep->val.ttl = sp->value->ttl;

                /* swizzle pointers */
                zend_hash_clean(&ctxt.copied);
                if (ep->val.val->type == IS_OBJECT) {
                    apc_swizzle_ptr(bd, &ctxt, &ll, &bd->entries[count].val.val->value.str.val);
                } else {
                    apc_swizzle_zval(bd, &ctxt, &ll, bd->entries[count].val.val TSRMLS_CC);
                }
                apc_swizzle_ptr(bd, &ctxt, &ll, &bd->entries[count].val.val);
                apc_swizzle_ptr(bd, &ctxt, &ll, &bd->entries[count].key.str);

                count++;
            }
        }
    }
    zend_hash_destroy(&ctxt.copied);
    ctxt.copied.nTableSize=0;

    /* append swizzle pointer list to bd */
    bd = apc_swizzle_bd(bd, &ll TSRMLS_CC);
    zend_llist_destroy(&ll);
    zend_hash_destroy(&APCG(apc_bd_alloc_list));

    APC_RUNLOCK(cache->header);

    if(user_vars) {
        zend_hash_destroy(user_vars);
        efree(user_vars);
    }

    efree(pool_ptr);

    return bd;
} /* }}} */

/* {{{ apc_bin_load */
PHP_APCU_API int apc_bin_load(apc_cache_t* cache, apc_bd_t *bd, int flags TSRMLS_DC) {
    apc_bd_entry_t *ep;
    uint i;
    apc_context_t ctxt;

    if (bd->swizzled) {
        if(apc_unswizzle_bd(bd, flags TSRMLS_CC) < 0) {
            return -1;
        }
    }

    for(i = 0; i < bd->num_entries; i++) {
        ctxt.pool = apc_pool_create(APC_SMALL_POOL, (apc_malloc_t) apc_sma_malloc, (apc_free_t) apc_sma_free, apc_sma_protect, apc_sma_unprotect TSRMLS_CC);
        if (!ctxt.pool) { /* TODO need to cleanup previous pools */
            apc_warning("Unable to allocate memory for pool." TSRMLS_CC);
            goto failure;
        }
        ep = &bd->entries[i];
        {
            zval *data;
            uint use_copy = 0;
            switch (Z_TYPE_P(ep->val.val)) {
                case IS_OBJECT:
                    ctxt.copy = APC_COPY_OUT;
                    data = apc_copy_zval(NULL, ep->val.val, &ctxt TSRMLS_CC);
                    use_copy = 1;
                break;
                default:
                    data = ep->val.val;
                break;
            }
            ctxt.copy = APC_COPY_IN;

            apc_cache_store(
				cache, ep->key.str, ep->key.len, data, ep->val.ttl, 0 TSRMLS_CC);

            if (use_copy) {
                zval_ptr_dtor(&data);
            }
        }
    }

    return 0;

failure:
    apc_pool_destroy(ctxt.pool TSRMLS_CC);
    apc_warning("Unable to allocate memory for apc binary load/dump functionality." TSRMLS_CC);

    HANDLE_UNBLOCK_INTERRUPTIONS();
    return -1;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
