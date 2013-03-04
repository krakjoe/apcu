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

/* $Id: apc_cache.c 328743 2012-12-12 07:58:32Z ab $ */

#include "apc_cache.h"
#include "apc_sma.h"
#include "apc_globals.h"
#include "SAPI.h"
#include "TSRM.h"
#include "ext/standard/md5.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"

typedef void* (*ht_copy_fun_t)(void*, void*, apc_context_t* TSRMLS_DC);
typedef int (*ht_check_copy_fun_t)(Bucket*, va_list);

/* TODO: rehash when load factor exceeds threshold */

#define CHECK(p) { if ((p) == NULL) return NULL; }

static void apc_cache_expunge(apc_cache_t* cache, size_t size TSRMLS_DC);

static APC_HOTSPOT zval* my_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC);
static HashTable* my_copy_hashtable_ex(HashTable*, HashTable* TSRMLS_DC, ht_copy_fun_t, int, apc_context_t*, ht_check_copy_fun_t, ...);
#define my_copy_hashtable( dst, src, copy_fn, holds_ptr, ctxt) \
    my_copy_hashtable_ex(dst, src TSRMLS_CC, copy_fn, holds_ptr, ctxt, NULL)

/* {{{ string_nhash_8 */
#define string_nhash_8(s,len) (unsigned long)(zend_inline_hash_func((s), len))
/* }}} */

/* {{{ murmurhash */
#if 0
static inline unsigned long murmurhash(const char *skey, size_t keylen)
{
	const long m = 0x7fd652ad;
	const long r = 16;
	unsigned int h = 0xdeadbeef;

	while(keylen >= 4)
	{
		h += *(unsigned int*)skey;
		h *= m;
		h ^= h >> r;

		skey += 4;
		keylen -= 4;
	}

	switch(keylen)
	{
	case 3:
		h += skey[2] << 16;
	case 2:
		h += skey[1] << 8;
	case 1:
		h += skey[0];
		h *= m;
		h ^= h >> r;
	};

	h *= m;
	h ^= h >> 10;
	h *= m;
	h ^= h >> 17;

	return h;
}
#endif
/* }}} */

/* {{{ make_prime */
static int const primes[] = {
  257, /*   256 */
  521, /*   512 */
 1031, /*  1024 */
 2053, /*  2048 */
 3079, /*  3072 */
 4099, /*  4096 */
 5147, /*  5120 */
 6151, /*  6144 */
 7177, /*  7168 */
 8209, /*  8192 */
 9221, /*  9216 */
10243, /* 10240 */
11273, /* 11264 */
12289, /* 12288 */
13313, /* 13312 */
14341, /* 14336 */
15361, /* 15360 */
16411, /* 16384 */
17417, /* 17408 */
18433, /* 18432 */
19457, /* 19456 */
0      /* sentinel */
};

static int make_prime(int n)
{
    int *k = (int*)primes; 
    while(*k) {
        if((*k) > n) return *k;
        k++;
    }
    return *(k-1);
}
/* }}} */

/* {{{ make_slot */
slot_t* make_slot(apc_cache_key_t *key, apc_cache_entry_t* value, slot_t* next, time_t t TSRMLS_DC)
{
    slot_t* p = apc_pool_alloc(value->pool, sizeof(slot_t));
	char *identifier = NULL;
	
    if (!p) 
		return NULL;

	identifier = (char*) apc_pmemcpy(
		key->data.identifier, key->data.identifier_len, 
		value->pool TSRMLS_CC
	);
	
	if (!identifier)
		return NULL;
	
    key->data.identifier = identifier;

    p->key = key[0];
    p->value = value;
    p->next = next;
    p->num_hits = 0;
    p->creation_time = t;
    p->access_time = t;
    p->deletion_time = 0;
    return p;
}
/* }}} */

/* {{{ free_slot */
static void free_slot(slot_t* slot TSRMLS_DC)
{
    apc_pool_destroy(slot->value->pool TSRMLS_CC);
}
/* }}} */

/* {{{ remove_slot 
 Must never be called outside of a lock */
static void remove_slot(apc_cache_t* cache, slot_t** slot TSRMLS_DC)
{
    slot_t* dead = *slot;
    *slot = (*slot)->next;

    cache->header->mem_size -= dead->value->mem_size;
    cache->header->num_entries--;
	
    if (dead->value->ref_count <= 0) {
        free_slot(dead TSRMLS_CC);
    }
    else {
        dead->next = cache->header->deleted_list;
        dead->deletion_time = time(0);
        cache->header->deleted_list = dead;
    }
}
/* }}} */

/* {{{ process_pending_removals */
static void process_pending_removals(apc_cache_t* cache TSRMLS_DC)
{
    slot_t** slot;
    time_t now;

    /* This function scans the list of removed cache entries and deletes any
     * entry whose reference count is zero (indicating that it is no longer
     * being executed) or that has been on the pending list for more than
     * cache->gc_ttl seconds (we issue a warning in the latter case).
     */

    if (!cache->header->deleted_list)
        return;

    slot = &cache->header->deleted_list;
    now = time(0);

    while (*slot != NULL) {
        int gc_sec = cache->gc_ttl ? (now - (*slot)->deletion_time) : 0;

        if ((*slot)->value->ref_count <= 0 || gc_sec > cache->gc_ttl) {
            slot_t* dead = *slot;

            if (dead->value->ref_count > 0) {
                apc_debug(
					"GC cache entry '%s' was on gc-list for %d seconds" TSRMLS_CC, 
					dead->value->data.info, gc_sec
				);
            }
            *slot = dead->next;
            free_slot(dead TSRMLS_CC);
        }
        else {
            slot = &(*slot)->next;
        }
    }
}
/* }}} */


/* {{{ apc php serializers */
int APC_SERIALIZER_NAME(php) (APC_SERIALIZER_ARGS) 
{
    smart_str strbuf = {0};
    php_serialize_data_t var_hash;
    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&strbuf, (zval**)&value, &var_hash TSRMLS_CC);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    if(strbuf.c) {
        *buf = (unsigned char*)strbuf.c;
        *buf_len = strbuf.len;
        smart_str_0(&strbuf);
        return 1; 
    }
    return 0;
}

int APC_UNSERIALIZER_NAME(php) (APC_UNSERIALIZER_ARGS) 
{
    const unsigned char *tmp = buf;
    php_unserialize_data_t var_hash;
    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    if(!php_var_unserialize(value, &tmp, buf + buf_len, &var_hash TSRMLS_CC)) {
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        zval_dtor(*value);
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %ld of %ld bytes", (long)(tmp - buf), (long)buf_len);
        (*value)->type = IS_NULL;
        return 0;
    }
    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
    return 1;
}
/* }}} */


/* {{{ apc_cache_create */
apc_cache_t* apc_cache_create(int size_hint, int gc_ttl, int ttl TSRMLS_DC)
{
    apc_cache_t* cache;
    int cache_size;
    int num_slots;

    num_slots = make_prime(size_hint > 0 ? size_hint : 2000);

    cache = (apc_cache_t*) apc_emalloc(sizeof(apc_cache_t) TSRMLS_CC);
    cache_size = sizeof(cache_header_t) + num_slots*sizeof(slot_t*);

    cache->shmaddr = apc_sma_malloc(cache_size TSRMLS_CC);
    if(!cache->shmaddr) {
        apc_error("Unable to allocate shared memory for cache structures.  (Perhaps your shared memory size isn't large enough?). " TSRMLS_CC);
        return NULL;
    }
    memset(cache->shmaddr, 0, cache_size);

    cache->header = (cache_header_t*) cache->shmaddr;
    cache->header->num_hits = 0;
    cache->header->num_misses = 0;
    cache->header->deleted_list = NULL;
    cache->header->start_time = time(NULL);
    cache->header->expunges = 0;
    cache->header->busy = 0;

    cache->slots = (slot_t**) (((char*) cache->shmaddr) + sizeof(cache_header_t));
    cache->num_slots = num_slots;
    cache->gc_ttl = gc_ttl;
    cache->ttl = ttl;

    CREATE_LOCK(&cache->header->lock);

    memset(cache->slots, 0, sizeof(slot_t*)*num_slots);
    cache->expunge_cb = apc_cache_expunge;

    return cache;
}
/* }}} */

/* {{{ apc_cache_release */
void apc_cache_release(apc_cache_t* cache, apc_cache_entry_t* entry TSRMLS_DC)
{
    entry->ref_count--;
}
/* }}} */

/* {{{ apc_cache_destroy */
void apc_cache_destroy(apc_cache_t* cache TSRMLS_DC)
{
    DESTROY_LOCK(&cache->header->lock);

	/* XXX this is definitely a leak, but freeing this causes all the apache
		children to freeze. It might be because the segment is shared between
		several processes. To figure out is how to free this safely. */
    /*apc_sma_free(cache->shmaddr TSRMLS_CC);*/
    apc_efree(cache TSRMLS_CC);
}
/* }}} */

/* {{{ apc_cache_clear */
void apc_cache_clear(apc_cache_t* cache TSRMLS_DC)
{
    int i;

    if(!cache) return;

    CACHE_LOCK(cache);
    cache->header->busy = 1;
    cache->header->num_hits = 0;
    cache->header->num_misses = 0;
    cache->header->start_time = time(NULL);
    cache->header->expunges = 0;

    for (i = 0; i < cache->num_slots; i++) {
        slot_t* p = cache->slots[i];
        while (p) {
            remove_slot(cache, &p TSRMLS_CC);
        }
        cache->slots[i] = NULL;
    }

    memset(&cache->header->lastkey, 0, sizeof(apc_keyid_t));

    cache->header->busy = 0;
    CACHE_UNLOCK(cache);
}
/* }}} */

/* {{{ apc_cache_expunge */
static void apc_cache_expunge(apc_cache_t* cache, size_t size TSRMLS_DC)
{
    int i;
    time_t t;

    t = apc_time();

    if(!cache) return;

    if(!cache->ttl) {
        /*
         * If cache->ttl is not set, we wipe out the entire cache when
         * we run out of space.
         */
        WLOCK(&cache->header->lock);
        process_pending_removals(cache TSRMLS_CC);
        if (apc_sma_get_avail_mem() > (size_t)(APCG(shm_size)/2)) {
            /* probably a queued up expunge, we don't need to do this */
            WUNLOCK(&cache->header->lock);
            return;
        }
        cache->header->busy = 1;
        cache->header->expunges++;

clear_all:
        for (i = 0; i < cache->num_slots; i++) {
            slot_t* p = cache->slots[i];
            while (p) {
                remove_slot(cache, &p TSRMLS_CC);
            }
            cache->slots[i] = NULL;
        }
        memset(&cache->header->lastkey, 0, sizeof(apc_keyid_t));
        cache->header->busy = 0;
        WUNLOCK(&cache->header->lock);
    } else {
        slot_t **p;
        /*
         * If the ttl for the cache is set we walk through and delete stale 
         * entries.  For the user cache that is slightly confusing since
         * we have the individual entry ttl's we can look at, but that would be
         * too much work.  So if you want the user cache expunged, set a high
         * default apc.user_ttl and still provide a specific ttl for each entry
         * on insert
         */

        WLOCK(&cache->header->lock);
        process_pending_removals(cache TSRMLS_CC);
        if (apc_sma_get_avail_mem() > (size_t)(APCG(shm_size)/2)) {
            /* probably a queued up expunge, we don't need to do this */
            WUNLOCK(&cache->header->lock);
            return;
        }

        cache->header->busy = 1;
        cache->header->expunges++;
		
        for (i = 0; i < cache->num_slots; i++) {
            p = &cache->slots[i];
            while(*p) {
                /*
                 * For the user cache we look at the individual entry ttl values
                 * and if not set fall back to the default ttl for the user cache
                 */
                if((*p)->value->data.ttl) {
                    if((time_t) ((*p)->creation_time + (*p)->value->data.ttl) < t) {
                        remove_slot(cache, p TSRMLS_CC);
                        continue;
                    }
                } else if(cache->ttl) {
                    if((*p)->creation_time + cache->ttl < t) {
                        remove_slot(cache, p TSRMLS_CC);
                        continue;
                    }
                }
                p = &(*p)->next;
            }
        }

        if (!apc_sma_get_avail_size(size)) {
            /* TODO: re-do this to remove goto across locked sections */
            goto clear_all;
        }
        memset(&cache->header->lastkey, 0, sizeof(apc_keyid_t));
        cache->header->busy = 0;
        WUNLOCK(&cache->header->lock);
    }
}
/* }}} */

/* {{{ apc_cache_user_insert */
int apc_cache_user_insert(apc_cache_t* cache, apc_cache_key_t key, apc_cache_entry_t* value, apc_context_t* ctxt, time_t t, int exclusive TSRMLS_DC)
{
    slot_t** slot;
    unsigned int keylen = key.data.identifier_len;
    apc_keyid_t *lastkey = &cache->header->lastkey;
    
    if (!value) {
        return 0;
    }
    
    if(apc_cache_busy(cache)) {
        /* cache cleanup in progress, do not wait */ 
        return 0;
    }

    CACHE_LOCK(cache);

    memset(lastkey, 0, sizeof(apc_keyid_t));

    lastkey->h = key.h;
    lastkey->keylen = keylen;
    lastkey->mtime = t;
#ifdef ZTS
    lastkey->tid = tsrm_thread_id();
#else
    lastkey->pid = getpid();
#endif
    
    /* we do not reset lastkey after the insert. Whether it is inserted 
     * or not, another insert in the same second is always a bad idea. 
     */
    process_pending_removals(cache TSRMLS_CC);
    
    slot = &cache->slots[key.h % cache->num_slots];

    while (*slot) {
        if (((*slot)->key.h == key.h) && 
            (!memcmp((*slot)->key.data.identifier, key.data.identifier, keylen))) {
            /* 
             * At this point we have found the user cache entry.  If we are doing 
             * an exclusive insert (apc_add) we are going to bail right away if
             * the user entry already exists and it has no ttl, or
             * there is a ttl and the entry has not timed out yet.
             */
            if(exclusive && (  !(*slot)->value->data.ttl ||
                              ( (*slot)->value->data.ttl && (time_t) ((*slot)->creation_time + (*slot)->value->data.ttl) >= t ) 
                            ) ) {
                goto fail;
            }
            remove_slot(cache, slot TSRMLS_CC);
            break;
        } else 
        /* 
         * This is a bit nasty.  The idea here is to do runtime cleanup of the linked list of
         * slot entries so we don't always have to skip past a bunch of stale entries.  We check
         * for staleness here and get rid of them by first checking to see if the cache has a global
         * access ttl on it and removing entries that haven't been accessed for ttl seconds and secondly
         * we see if the entry has a hard ttl on it and remove it if it has been around longer than its ttl
         */
        if((cache->ttl && (*slot)->access_time < (t - cache->ttl)) || 
           ((*slot)->value->data.ttl && (time_t) ((*slot)->creation_time + (*slot)->value->data.ttl) < t)) {
            remove_slot(cache, slot TSRMLS_CC);
            continue;
        }
        slot = &(*slot)->next;
    }

    if ((*slot = make_slot(&key, value, *slot, t TSRMLS_CC)) == NULL) {
        goto fail;
    } 
    
    value->mem_size = ctxt->pool->size;
    cache->header->mem_size += ctxt->pool->size;

    cache->header->num_entries++;
    cache->header->num_inserts++;

    CACHE_UNLOCK(cache);

    return 1;

fail:
    CACHE_UNLOCK(cache);

    return 0;
}
/* }}} */



/* {{{ apc_cache_user_find */
apc_cache_entry_t* apc_cache_user_find(apc_cache_t* cache, char *strkey, int keylen, time_t t TSRMLS_DC)
{
    slot_t** slot;
    volatile apc_cache_entry_t* value = NULL;
    unsigned long h;

    if(apc_cache_busy(cache))
    {
        /* cache cleanup in progress */ 
        return NULL;
    }

    RLOCK(&cache->header->lock);

    h = string_nhash_8(strkey, keylen);

    slot = &cache->slots[h % cache->num_slots];

    while (*slot) {
        if ((h == (*slot)->key.h) &&
            !memcmp((*slot)->key.data.identifier, strkey, keylen)) {
            /* Check to make sure this entry isn't expired by a hard TTL */
            if((*slot)->value->data.ttl && (time_t) ((*slot)->creation_time + (*slot)->value->data.ttl) < t) {
                #if (USE_READ_LOCKS == 0) 
                /* this is merely a memory-friendly optimization, if we do have a write-lock
                 * might as well move this to the deleted_list right-away. Otherwise an insert
                 * of the same key wil do it (or an expunge, *eventually*).
                 */
                remove_slot(cache, slot TSRMLS_CC);
                #endif
                cache->header->num_misses++;

                RUNLOCK(&cache->header->lock);
                return NULL;
            }

            /* Otherwise we are fine, increase counters and return the cache entry */
            (*slot)->num_hits++;
            (*slot)->value->ref_count++;
            (*slot)->access_time = t;

            cache->header->num_hits++;

            value = (*slot)->value;
            RUNLOCK(&cache->header->lock);
            return (apc_cache_entry_t*)value;
        }
        slot = &(*slot)->next;
    }
 
    cache->header->num_misses++;
	
    RUNLOCK(&cache->header->lock);
    return NULL;
}
/* }}} */

/* {{{ apc_cache_user_exists */
apc_cache_entry_t* apc_cache_user_exists(apc_cache_t* cache, char *strkey, int keylen, time_t t TSRMLS_DC)
{
    slot_t** slot;
    volatile apc_cache_entry_t* value = NULL;
    unsigned long h;

    if(apc_cache_busy(cache))
    {
        /* cache cleanup in progress */ 
        return NULL;
    }

    RLOCK(&cache->header->lock);

    h = string_nhash_8(strkey, keylen);

    slot = &cache->slots[h % cache->num_slots];

    while (*slot) {
        if ((h == (*slot)->key.h) &&
            !memcmp((*slot)->key.data.identifier, strkey, keylen)) {
            /* Check to make sure this entry isn't expired by a hard TTL */
            if((*slot)->value->data.ttl && (time_t) ((*slot)->creation_time + (*slot)->value->data.ttl) < t) {
                CACHE_UNLOCK(cache);
                return NULL;
            }
            /* Return the cache entry ptr */
            value = (*slot)->value;
            RUNLOCK(&cache->header->lock);
            return (apc_cache_entry_t*)value;
        }
        slot = &(*slot)->next;
    }
    RUNLOCK(&cache->header->lock);
    return NULL;
}
/* }}} */

/* {{{ apc_cache_user_update */
int _apc_cache_user_update(apc_cache_t* cache, char *strkey, int keylen, apc_cache_updater_t updater, void* data TSRMLS_DC)
{
    slot_t** slot;
    int retval;
    unsigned long h;

    if(apc_cache_busy(cache))
    {
        /* cache cleanup in progress */ 
        return 0;
    }

    CACHE_LOCK(cache);

    h = string_nhash_8(strkey, keylen);
    slot = &cache->slots[h % cache->num_slots];

    while (*slot) {
        if ((h == (*slot)->key.h) &&
            !memcmp((*slot)->key.data.identifier, strkey, keylen)) {
            switch(Z_TYPE_P((*slot)->value->data.val) & ~IS_CONSTANT_INDEX) {
                case IS_ARRAY:
                case IS_CONSTANT_ARRAY:
                case IS_OBJECT:
                {
                    if(APCG(serializer)) {
                        retval = 0;
                        break;
                    } else {
                        /* fall through */
                    }
                }
                /* fall through */
                default:
                {
                    retval = updater(cache, (*slot)->value, data);
                    (*slot)->key.mtime = apc_time();
                }
                break;
            }
            CACHE_UNLOCK(cache);
            return retval;
        }
        slot = &(*slot)->next;
    }
    CACHE_UNLOCK(cache);
    return 0;
}
/* }}} */

/* {{{ apc_cache_user_delete */
int apc_cache_user_delete(apc_cache_t* cache, char *strkey, int keylen TSRMLS_DC)
{
    slot_t** slot;
    unsigned long h;

    CACHE_LOCK(cache);

    h = string_nhash_8(strkey, keylen);

    slot = &cache->slots[h % cache->num_slots];

    while (*slot) {
        if ((h == (*slot)->key.h) && 
            !memcmp((*slot)->key.data.identifier, strkey, keylen)) {
            remove_slot(cache, slot TSRMLS_CC);
            CACHE_UNLOCK(cache);
            return 1;
        }
        slot = &(*slot)->next;
    }

    CACHE_UNLOCK(cache);
    return 0;
}
/* }}} */

/* {{{ apc_cache_make_user_key */
int apc_cache_make_user_key(apc_cache_key_t* key, char* identifier, int identifier_len, const time_t t)
{
    assert(key != NULL);

    if (!identifier)
        return 0;

    key->data.identifier = identifier;
    key->data.identifier_len = identifier_len;
    key->h = string_nhash_8(
		(char *)key->data.identifier, key->data.identifier_len);
    key->mtime = t;

    return 1;
}
/* }}} */

/* {{{ my_serialize_object */
static zval* my_serialize_object(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    smart_str buf = {0};
    apc_pool* pool = ctxt->pool;
    apc_serialize_t serialize = APC_SERIALIZER_NAME(php);
    void *config = NULL;

    if(APCG(serializer)) { /* TODO: move to ctxt */
        serialize = APCG(serializer)->serialize;
        config = APCG(serializer)->config;
    }

    if(serialize((unsigned char**)&buf.c, &buf.len, src, config TSRMLS_CC)) {
        dst->type = src->type & ~IS_CONSTANT_INDEX; 
        dst->value.str.len = buf.len;
        CHECK(dst->value.str.val = apc_pmemcpy(buf.c, (buf.len + 1), pool TSRMLS_CC));
    }

    if(buf.c) smart_str_free(&buf);

    return dst;
}
/* }}} */

/* {{{ my_unserialize_object */
static zval* my_unserialize_object(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    apc_unserialize_t unserialize = APC_UNSERIALIZER_NAME(php);
    unsigned char *p = (unsigned char*)Z_STRVAL_P(src);
    void *config = NULL;

    if(APCG(serializer)) { /* TODO: move to ctxt */
        unserialize = APCG(serializer)->unserialize;
        config = APCG(serializer)->config;
    }

    if(unserialize(&dst, p, Z_STRLEN_P(src), config TSRMLS_CC)) {
        return dst;
    } else {
        zval_dtor(dst);
        dst->type = IS_NULL;
    }
    return dst;
}
/* }}} */


/* {{{ my_copy_hashtable_ex */
static APC_HOTSPOT HashTable* my_copy_hashtable_ex(HashTable* dst,
                                    HashTable* src TSRMLS_DC,
                                    ht_copy_fun_t copy_fn,
                                    int holds_ptrs,
                                    apc_context_t* ctxt,
                                    ht_check_copy_fun_t check_fn,
                                    ...)
{
    Bucket* curr = NULL;
    Bucket* prev = NULL;
    Bucket* newp = NULL;
    int first = 1;
    apc_pool* pool = ctxt->pool;

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (HashTable*) apc_pool_alloc(pool, sizeof(src[0])));
    }

    memcpy(dst, src, sizeof(src[0]));

    /* allocate buckets for the new hashtable */
    CHECK((dst->arBuckets = apc_pool_alloc(pool, (dst->nTableSize * sizeof(Bucket*)))));

    memset(dst->arBuckets, 0, dst->nTableSize * sizeof(Bucket*));
    dst->pInternalPointer = NULL;
    dst->pListHead = NULL;

    for (curr = src->pListHead; curr != NULL; curr = curr->pListNext) {
        int n = curr->h % dst->nTableSize;

        if(check_fn) {
            va_list args;
            va_start(args, check_fn);

            /* Call the check_fn to see if the current bucket
             * needs to be copied out
             */
            if(!check_fn(curr, args)) {
                dst->nNumOfElements--;
                va_end(args);
                continue;
            }

            va_end(args);
        }

        /* create a copy of the bucket 'curr' */
#ifdef ZEND_ENGINE_2_4
        if (!curr->nKeyLength) {
            CHECK((newp = (Bucket*) apc_pmemcpy(curr, sizeof(Bucket), pool TSRMLS_CC)));
        } else if (IS_INTERNED(curr->arKey)) {
            CHECK((newp = (Bucket*) apc_pmemcpy(curr, sizeof(Bucket), pool TSRMLS_CC)));
#ifndef ZTS
        } else if (pool->type != APC_UNPOOL) {
            char *arKey;

            arKey = (char *) zend_new_interned_string(curr->arKey, curr->nKeyLength, 0 TSRMLS_CC);
            if (!arKey) {
                /* this is ugly, but the old arkey[1] is gone, so we allocate all of the bytes as a tail-fragment (see IS_TAILED) */
                CHECK((newp = (Bucket*) apc_pmemcpy(curr, sizeof(Bucket) + curr->nKeyLength, pool TSRMLS_CC)));
                newp->arKey = (const char*)(newp+1); 
            } else {
                CHECK((newp = (Bucket*) apc_pmemcpy(curr, sizeof(Bucket), pool TSRMLS_CC)));
                newp->arKey = arKey;
            }
#endif
        } else {
            /* I repeat, this is ugly */
            CHECK((newp = (Bucket*) apc_pmemcpy(curr, sizeof(Bucket) + curr->nKeyLength, pool TSRMLS_CC)));
            newp->arKey = (const char*)(newp+1);
        }
#else
        CHECK((newp = (Bucket*) apc_pmemcpy(curr,
                                  (sizeof(Bucket) + curr->nKeyLength - 1),
                                  pool TSRMLS_CC)));
#endif

        /* insert 'newp' into the linked list at its hashed index */
        if (dst->arBuckets[n]) {
            newp->pNext = dst->arBuckets[n];
            newp->pLast = NULL;
            newp->pNext->pLast = newp;
        }
        else {
            newp->pNext = newp->pLast = NULL;
        }

        dst->arBuckets[n] = newp;

        /* copy the bucket data using our 'copy_fn' callback function */
        CHECK((newp->pData = copy_fn(NULL, curr->pData, ctxt TSRMLS_CC)));

        if (holds_ptrs) {
            memcpy(&newp->pDataPtr, newp->pData, sizeof(void*));
        }
        else {
            newp->pDataPtr = NULL;
        }

        /* insert 'newp' into the table-thread linked list */
        newp->pListLast = prev;
        newp->pListNext = NULL;

        if (prev) {
            prev->pListNext = newp;
        }

        if (first) {
            dst->pListHead = newp;
            first = 0;
        }

        prev = newp;
    }

    dst->pListTail = newp;

    zend_hash_internal_pointer_reset(dst);

    return dst;
}
/* }}} */


/* {{{ my_copy_zval_ptr */
static zval** my_copy_zval_ptr(zval** dst, const zval** src, apc_context_t* ctxt TSRMLS_DC)
{
    zval* dst_new;
    apc_pool* pool = ctxt->pool;
    int usegc = (ctxt->copy == APC_COPY_OUT_USER);

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zval**) apc_pool_alloc(pool, sizeof(zval*)));
    }

    if(usegc) {
        ALLOC_ZVAL(dst[0]);
        CHECK(dst[0]);
    } else {
        CHECK((dst[0] = (zval*) apc_pool_alloc(pool, sizeof(zval))));
    }

    CHECK((dst_new = my_copy_zval(*dst, *src, ctxt TSRMLS_CC)));

    if(dst_new != *dst) {
        if(usegc) {
            FREE_ZVAL(dst[0]);
        }
        *dst = dst_new;
    }

    return dst;
}
/* }}} */


/* {{{ my_copy_zval */
static APC_HOTSPOT zval* my_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    zval **tmp;
    apc_pool* pool = ctxt->pool;

    assert(dst != NULL);
    assert(src != NULL);

    memcpy(dst, src, sizeof(src[0]));

    if(APCG(copied_zvals).nTableSize) {
        if(zend_hash_index_find(&APCG(copied_zvals), (ulong)src, (void**)&tmp) == SUCCESS) {
            if(Z_ISREF_P((zval*)src)) {
                Z_SET_ISREF_PP(tmp);
            }
            Z_ADDREF_PP(tmp);
            return *tmp;
        }

        zend_hash_index_update(&APCG(copied_zvals), (ulong)src, (void**)&dst, sizeof(zval*), NULL);
    }

    if(ctxt->copy == APC_COPY_OUT_USER || ctxt->copy == APC_COPY_IN_USER) {
        /* deep copies are refcount(1), but moved up for recursive 
         * arrays,  which end up being add_ref'd during its copy. */
        Z_SET_REFCOUNT_P(dst, 1);
        Z_UNSET_ISREF_P(dst);
    } else {
        /* code uses refcount=2 for consts */
        Z_SET_REFCOUNT_P(dst, Z_REFCOUNT_P((zval*)src));
        Z_SET_ISREF_TO_P(dst, Z_ISREF_P((zval*)src));
    }

    switch (src->type & IS_CONSTANT_TYPE_MASK) {
    case IS_RESOURCE:
    case IS_BOOL:
    case IS_LONG:
    case IS_DOUBLE:
    case IS_NULL:
        break;

    case IS_CONSTANT:
    case IS_STRING:
        if (src->value.str.val) {
            CHECK(dst->value.str.val = apc_pmemcpy(src->value.str.val,
                                                   src->value.str.len+1,
                                                   pool TSRMLS_CC));
        }
        break;

    case IS_ARRAY:
    case IS_CONSTANT_ARRAY:
        if(APCG(serializer) == NULL) {

            CHECK(dst->value.ht =
                my_copy_hashtable(NULL,
                                  src->value.ht,
                                  (ht_copy_fun_t) my_copy_zval_ptr,
                                  1,
                                  ctxt));
            break;
        } else {
            /* fall through to object case */
        }

    case IS_OBJECT:
    
        dst->type = IS_NULL;
        if(ctxt->copy == APC_COPY_IN_USER) {
            dst = my_serialize_object(dst, src, ctxt TSRMLS_CC);
        } else if(ctxt->copy == APC_COPY_OUT_USER) {
            dst = my_unserialize_object(dst, src, ctxt TSRMLS_CC);
        }
        break;
#ifdef ZEND_ENGINE_2_4
    case IS_CALLABLE:
        /* XXX implement this */
        assert(0);
        break;
#endif

    default:
        assert(0);
    }

    return dst;
}
/* }}} */

/* {{{ apc_copy_zval */
zval* apc_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    apc_pool* pool = ctxt->pool;
    
    assert(src != NULL);

    if (!dst) {
        if(ctxt->copy == APC_COPY_OUT_USER) {
            ALLOC_ZVAL(dst);
            CHECK(dst);
        } else {
            CHECK(dst = (zval*) apc_pool_alloc(pool, sizeof(zval)));
        }
    }

    CHECK(dst = my_copy_zval(dst, src, ctxt TSRMLS_CC));
    return dst;
}
/* }}} */

/* {{{ apc_cache_store_zval */
zval* apc_cache_store_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    if (Z_TYPE_P(src) == IS_ARRAY) {
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        zend_hash_init(&APCG(copied_zvals), 0, NULL, NULL, 0);
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
        zend_hash_destroy(&APCG(copied_zvals));
        APCG(copied_zvals).nTableSize=0;
    } else {
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
    }


    return dst;
}
/* }}} */

/* {{{ apc_cache_fetch_zval */
zval* apc_cache_fetch_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    if (Z_TYPE_P(src) == IS_ARRAY) {
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        zend_hash_init(&APCG(copied_zvals), 0, NULL, NULL, 0);
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
        zend_hash_destroy(&APCG(copied_zvals));
        APCG(copied_zvals).nTableSize=0;
    } else {
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
    }


    return dst;
}
/* }}} */

/* {{{ apc_cache_make_user_entry */
apc_cache_entry_t* apc_cache_make_user_entry(const char* info, int info_len, const zval* val, apc_context_t* ctxt, const unsigned int ttl TSRMLS_DC)
{
    apc_cache_entry_t* entry;
    apc_pool* pool = ctxt->pool;

    entry = (apc_cache_entry_t*) apc_pool_alloc(pool, sizeof(apc_cache_entry_t));
    if (!entry) return NULL;

    entry->data.info = apc_pmemcpy(info, info_len, pool TSRMLS_CC);
    entry->data.info_len = info_len;
    if(!entry->data.info) {
        return NULL;
    }
    entry->data.val = apc_cache_store_zval(NULL, val, ctxt TSRMLS_CC);
    if(!entry->data.val) {
        return NULL;
    }
    INIT_PZVAL(entry->data.val);
    entry->data.ttl = ttl;
    entry->ref_count = 0;
    entry->mem_size = 0;
    entry->pool = pool;
    return entry;
}
/* }}} */

#if !defined(_WIN32) && !defined(ZTS)
apc_async_insert_t* apc_cache_make_async_insert(char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC) 
{
	apc_async_insert_t *insert = apc_sma_malloc(sizeof(apc_async_insert_t) TSRMLS_CC);
		
	if (!insert)
		return NULL;

	insert->cache = apc_user_cache;
	insert->ctime = apc_time();
	insert->ttl   = ttl;
	insert->exclusive = exclusive;	

	if (!apc_cache_make_user_key(&insert->key, strkey, strkey_len, insert->ctime) ||
		apc_cache_is_last_key(insert->cache, &insert->key, insert->ctime TSRMLS_CC)) {
		apc_sma_free(insert TSRMLS_CC);
		
		return NULL;
	}
	
	insert->ctx.pool = apc_pool_create(APC_SMALL_POOL, apc_sma_malloc, apc_sma_free, apc_sma_protect, apc_sma_unprotect TSRMLS_CC);

	if (!insert->ctx.pool) {
		apc_efree(insert TSRMLS_CC);
		
		return NULL;
	}

	insert->ctx.copy = APC_COPY_IN_USER;
	insert->ctx.force_update = 0;
	
	insert->entry = apc_cache_make_user_entry(strkey, strkey_len, val, &insert->ctx, insert->ttl TSRMLS_CC);	

	if (!insert->entry) {
		apc_pool_destroy(insert->ctx.pool TSRMLS_CC);
		apc_sma_free(insert TSRMLS_CC);
		
		return NULL;
	}

	return insert;
}
#endif

/* {{{ apc_cache_link_info */
static zval* apc_cache_link_info(apc_cache_t *cache, slot_t* p TSRMLS_DC)
{
    zval *link = NULL;

    ALLOC_INIT_ZVAL(link);

    if(!link) {
        return NULL;
    }

    array_init(link);

    add_assoc_stringl(link, "info", p->value->data.info, p->value->data.info_len-1, 1);
    add_assoc_long(link, "ttl", (long)p->value->data.ttl);

    add_assoc_double(link, "num_hits", (double)p->num_hits);
    add_assoc_long(link, "mtime", p->key.mtime);
    add_assoc_long(link, "creation_time", p->creation_time);
    add_assoc_long(link, "deletion_time", p->deletion_time);
    add_assoc_long(link, "access_time", p->access_time);
    add_assoc_long(link, "ref_count", p->value->ref_count);
    add_assoc_long(link, "mem_size", p->value->mem_size);

    return link;
}
/* }}} */

/* {{{ apc_cache_info */
zval* apc_cache_info(apc_cache_t* cache, zend_bool limited TSRMLS_DC)
{
    zval *info = NULL;
    zval *list = NULL;
    zval *deleted_list = NULL;
    zval *slots = NULL;
    slot_t* p;
    int i, j;

    if(!cache) return NULL;

    RLOCK(&cache->header->lock);

    ALLOC_INIT_ZVAL(info);

    if(!info) {
        RUNLOCK(&cache->header->lock);
        return NULL;
    }

    array_init(info);
    add_assoc_long(info, "num_slots", cache->num_slots);
    add_assoc_long(info, "ttl", cache->ttl);

    add_assoc_double(info, "num_hits", (double)cache->header->num_hits);
    add_assoc_double(info, "num_misses", (double)cache->header->num_misses);
    add_assoc_double(info, "num_inserts", (double)cache->header->num_inserts);
    add_assoc_double(info, "expunges", (double)cache->header->expunges);
    
    add_assoc_long(info, "start_time", cache->header->start_time);
    add_assoc_double(info, "mem_size", (double)cache->header->mem_size);
    add_assoc_long(info, "num_entries", cache->header->num_entries);
#ifdef MULTIPART_EVENT_FORMDATA
    add_assoc_long(info, "file_upload_progress", 1);
#else
    add_assoc_long(info, "file_upload_progress", 0);
#endif
#if APC_MMAP
    add_assoc_stringl(info, "memory_type", "mmap", sizeof("mmap")-1, 1);
#else
    add_assoc_stringl(info, "memory_type", "IPC shared", sizeof("IPC shared")-1, 1);
#endif

    if(!limited) {
        /* For each hashtable slot */
        ALLOC_INIT_ZVAL(list);
        array_init(list);

        ALLOC_INIT_ZVAL(slots);
        array_init(slots);

        for (i = 0; i < cache->num_slots; i++) {
            p = cache->slots[i];
            j = 0;
            for (; p != NULL; p = p->next) {
                zval *link = apc_cache_link_info(cache, p TSRMLS_CC);
                add_next_index_zval(list, link);
                j++;
            }
            if(j != 0) {
                add_index_long(slots, (ulong)i, j);
            }
        }

        /* For each slot pending deletion */
        ALLOC_INIT_ZVAL(deleted_list);
        array_init(deleted_list);

        for (p = cache->header->deleted_list; p != NULL; p = p->next) {
            zval *link = apc_cache_link_info(cache, p TSRMLS_CC);
            add_next_index_zval(deleted_list, link);
        }
        
        add_assoc_zval(info, "cache_list", list);
        add_assoc_zval(info, "deleted_list", deleted_list);
        add_assoc_zval(info, "slot_distribution", slots);
    }

    RUNLOCK(&cache->header->lock);
    return info;
}
/* }}} */

/* {{{ apc_cache_busy */
zend_bool apc_cache_busy(apc_cache_t* cache)
{
    return cache->header->busy;
}
/* }}} */

/* {{{ apc_cache_is_last_key */
zend_bool apc_cache_is_last_key(apc_cache_t* cache, apc_cache_key_t* key, time_t t TSRMLS_DC)
{
    apc_keyid_t *lastkey = &cache->header->lastkey;
    unsigned int keylen = key->data.identifier_len;
#ifdef ZTS
    THREAD_T tid = tsrm_thread_id();
    #define FROM_DIFFERENT_THREAD(k) (memcmp(&((k)->tid), &tid, sizeof(THREAD_T))!=0)
#else
    pid_t pid = getpid();
    #define FROM_DIFFERENT_THREAD(k) (pid != (k)->pid) 
#endif

    /* unlocked reads, but we're not shooting for 100% success with this */
    if(lastkey->h == key->h && keylen == lastkey->keylen) {
        if(lastkey->mtime == t && FROM_DIFFERENT_THREAD(lastkey)) {
            /* potential cache slam */
            if(APCG(slam_defense)) {
                apc_debug("Potential cache slam averted for key '%s'" TSRMLS_CC, key->data.identifier);
                return 1;
            }
        }
    }

    return 0;
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
