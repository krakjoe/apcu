/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http:www.php.net/license/3_01.txt                                    |
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
#include "php_scandir.h"
#include "SAPI.h"
#include "TSRM.h"
#include "php_main.h"
#include "ext/standard/md5.h"
#include "ext/standard/php_var.h"
#include "zend_smart_str.h"

typedef void* (*ht_copy_fun_t)(void*, void*, apc_context_t*);
typedef int (*ht_check_copy_fun_t)(Bucket*, va_list);

#define CHECK(p) { if ((p) == NULL) return NULL; }

static APC_HOTSPOT zval* my_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt);

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
20483, /* 20480 */
30727, /* 30720 */
40961, /* 40960 */
61441, /* 61440 */
81929, /* 81920 */
122887,/* 122880 */
163841,/* 163840 */
245771,/* 245760 */
327689,/* 327680 */
491527,/* 491520 */
655373,/* 655360 */
983063,/* 983040 */
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
apc_cache_slot_t* make_slot(apc_cache_t* cache, apc_cache_key_t *key, apc_cache_entry_t* value, apc_cache_slot_t* next, time_t t)
{
	apc_cache_slot_t* p = NULL;
	
	/* allocate slot */
	if ((p = value->pool->palloc(value->pool, sizeof(apc_cache_slot_t)))) {	
		
		/* copy identifier */
		zend_string *copiedKey = apc_pstrcpy(key->str, value->pool);

		if (copiedKey == NULL) {
            value->pool->pfree(value->pool, p);
            return NULL;
        }
			
		/* set slot data */
		p->key = key[0];
		p->key.str = copiedKey;
		p->value = value;

		/* set slot relation */
		p->next = next;
			
		/* set slot defaults */
		p->nhits = 0;
		p->ctime = t;
		p->atime = t;
		p->dtime = 0;
	}

    return p;
}
/* }}} */

/* {{{ free_slot */
static void free_slot(apc_cache_slot_t* slot)
{
	/* destroy slot pool */
    apc_pool_destroy(
		slot->value->pool);
}
/* }}} */

/* {{{ apc_cache_hash_slot
 Note: These calculations can and should be done outside of a lock */
static void apc_cache_hash_slot(apc_cache_t* cache, 
                                zend_string *key,
                                zend_ulong* hash, 
                                zend_ulong* slot) {
	(*hash) = ZSTR_HASH(key);
	(*slot) = (*hash) % (cache->nslots);
} /* }}} */

/* {{{ apc_cache_remove_slot  */
PHP_APCU_API void apc_cache_remove_slot(apc_cache_t* cache, apc_cache_slot_t** slot)
{
    apc_cache_slot_t* dead = *slot;
    
    /* think here is safer */
	*slot = (*slot)->next;

	/* adjust header info */
	if (cache->header->mem_size)
		cache->header->mem_size -= dead->value->mem_size;

    if (cache->header->nentries)
		cache->header->nentries--;
	
	/* remove if there are no references */
    if (dead->value->ref_count <= 0) {
        free_slot(dead);
    } else {
		/* add to gc if there are still refs */
        dead->next = cache->header->gc;
        dead->dtime = time(0);
        cache->header->gc = dead;
    }
}
/* }}} */

/* {{{ apc_cache_gc */
PHP_APCU_API void apc_cache_gc(apc_cache_t* cache)
{
    /* This function scans the list of removed cache entries and deletes any
     * entry whose reference count is zero  or that has been on the gc 
	 * list for more than cache->gc_ttl seconds 
	 *   (we issue a warning in the latter case).
     */
	if (!cache || !cache->header->gc) {
		return;
	}

    {
		apc_cache_slot_t** slot = &cache->header->gc;

		while (*slot != NULL) {
			time_t now = time(0);
			time_t gc_sec = cache->gc_ttl ? (now - (*slot)->dtime) : 0;

			if (!(*slot)->value->ref_count || gc_sec > (time_t)cache->gc_ttl) {
                apc_cache_slot_t* dead = *slot;

				/* good ol' whining */
				if (dead->value->ref_count > 0) {
					apc_debug(
						"GC cache entry '%s' was on gc-list for %d seconds", 
						dead->key.str, gc_sec
					);
				}

				/* set next slot */
				*slot = dead->next;
			
				/* free slot */
				free_slot(
					dead);
			
				/* next */
				continue;

			} else {
				slot = &(*slot)->next;
			}
		}
	}
}
/* }}} */

/* {{{ php serializer */
PHP_APCU_API int APC_SERIALIZER_NAME(php) (APC_SERIALIZER_ARGS) 
{
    smart_str strbuf = {0};
    php_serialize_data_t var_hash;
    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&strbuf, (zval*) value, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);

    if (strbuf.s != NULL) {
        *buf = (unsigned char *)estrndup(ZSTR_VAL(strbuf.s), ZSTR_LEN(strbuf.s));
        if (*buf == NULL)
            return 0;

        *buf_len = ZSTR_LEN(strbuf.s);
		smart_str_free(&strbuf);
        return 1;
    }
    return 0;
} /* }}} */

/* {{{ php unserializer */
PHP_APCU_API int APC_UNSERIALIZER_NAME(php) (APC_UNSERIALIZER_ARGS) 
{
    const unsigned char *tmp = buf;
    php_unserialize_data_t var_hash;
    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    if(!php_var_unserialize(value, &tmp, buf + buf_len, &var_hash)) {
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        php_error_docref(NULL, E_NOTICE, "Error at offset %ld of %ld bytes", (zend_long)(tmp - buf), (zend_long)buf_len);
        ZVAL_NULL(value);
        return 0;
    }
    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
    return 1;
} /* }}} */

/* {{{ apc_cache_create */
PHP_APCU_API apc_cache_t* apc_cache_create(apc_sma_t* sma, apc_serializer_t* serializer, zend_long size_hint, zend_long gc_ttl, zend_long ttl, zend_long smart, zend_bool defend) {
	apc_cache_t* cache;
    zend_long cache_size;
    zend_long nslots;

	/* calculate number of slots */
    nslots = make_prime(size_hint > 0 ? size_hint : 2000);

	/* allocate pointer by normal means */
    cache = (apc_cache_t*) apc_emalloc(sizeof(apc_cache_t));

    if (!cache) {
        apc_error("Unable to allocate memory for cache structures. (Perhaps your memory_limit isn't large enough?). ");
        return NULL;
    }
	
	/* calculate cache size for shm allocation */
    cache_size = sizeof(apc_cache_header_t) + nslots*sizeof(apc_cache_slot_t*);

	/* allocate shm */
    cache->shmaddr = sma->smalloc(cache_size);

    if(!cache->shmaddr) {
        apc_error("Unable to allocate shared memory for cache structures.  (Perhaps your shared memory size isn't large enough?). ");
        return NULL;
    }
	
	/* zero shm */
    memset(cache->shmaddr, 0, cache_size);

	/* set default header */
    cache->header = (apc_cache_header_t*) cache->shmaddr;
	
    cache->header->nhits = 0;
    cache->header->nmisses = 0;
	cache->header->nentries = 0;
    cache->header->nexpunges = 0;
    cache->header->gc = NULL;
    cache->header->stime = time(NULL);
	cache->header->state |= APC_CACHE_ST_NONE;
	
	/* set cache options */
    cache->slots = (apc_cache_slot_t**) (((char*) cache->shmaddr) + sizeof(apc_cache_header_t));
    cache->sma = sma;
	cache->serializer = serializer;
	cache->nslots = nslots;
    cache->gc_ttl = gc_ttl;
    cache->ttl = ttl;
	cache->smart = smart;
	cache->defend = defend;
	
	/* header lock */
	CREATE_LOCK(&cache->header->lock);

	/* zero slots */
    memset(cache->slots, 0, sizeof(apc_cache_slot_t*)*nslots);

    return cache;
} /* }}} */

static inline zend_bool apc_cache_insert_internal(apc_cache_t* cache, 
                                        apc_cache_key_t *key, 
                                        apc_cache_entry_t* value, 
                                        apc_context_t* ctxt, 
                                        time_t t, 
                                        zend_bool exclusive) {
	/* at least */
	if (!value) {
		return 0;
	}
	
	/* check we are able to deal with this request */
	if (!cache || apc_cache_busy(cache)) {
		return 0;
	}
	
	/* process deleted list  */
    apc_cache_gc(cache);

	/* make the insertion */	
	{
		apc_cache_slot_t** slot;

		/*
		* select appropriate slot ...
		*/
		slot = &cache->slots[ZSTR_HASH(key->str) % cache->nslots];

		while (*slot) {
			
			/* check for a match by hash and string */
			if ((ZSTR_HASH((*slot)->key.str) == ZSTR_HASH(key->str)) && 
				memcmp(ZSTR_VAL((*slot)->key.str), ZSTR_VAL(key->str), ZSTR_LEN(key->str)) == SUCCESS) {

				/* 
				 * At this point we have found the user cache entry.  If we are doing 
				 * an exclusive insert (apc_add) we are going to bail right away if
				 * the user entry already exists and it has no ttl, or
				 * there is a ttl and the entry has not timed out yet.
				 */
				if(exclusive) {
                    if (!(*slot)->value->ttl || (time_t) ((*slot)->ctime + (*slot)->value->ttl) >= t) {
                        return 0;
                    }
				}
                apc_cache_remove_slot(cache, slot);
				break;
			} else

			/* 
			 * This is a bit nasty.  The idea here is to do runtime cleanup of the linked list of
			 * slot entries so we don't always have to skip past a bunch of stale entries.  We check
			 * for staleness here and get rid of them by first checking to see if the cache has a global
			 * access ttl on it and removing entries that haven't been accessed for ttl seconds and secondly
			 * we see if the entry has a hard ttl on it and remove it if it has been around longer than its ttl
			 */
			if((cache->ttl && (time_t)(*slot)->atime < (t - (time_t)cache->ttl)) || 
			   ((*slot)->value->ttl && (time_t) ((*slot)->ctime + (*slot)->value->ttl) < t)) {
                apc_cache_remove_slot(cache, slot);
				continue;
			}
		
			/* set next slot */
            slot = &(*slot)->next;      
		}

		if ((*slot = make_slot(cache, key, value, *slot, t)) != NULL) {
            /* set value size from pool size */
            value->mem_size = ctxt->pool->size;

            cache->header->mem_size += ctxt->pool->size;
            cache->header->nentries++;
            cache->header->ninserts++;
		} else {
			return 0;
		}
	}

    return 1;
}

static inline zend_bool apc_cache_store_internal(apc_cache_t *cache, zend_string *strkey, const zval *val, const int32_t ttl, const zend_bool exclusive) {
	apc_cache_entry_t *entry;
    apc_cache_key_t key;
    time_t t;
    apc_context_t ctxt={0,};
    zend_bool ret = 0;

    t = apc_time();

	/* initialize a context suitable for making an insert */
    if (apc_cache_make_context(cache, &ctxt, APC_CONTEXT_SHARE, APC_SMALL_POOL, APC_COPY_IN, 0)) {

        /* initialize the key for insertion */
        if (apc_cache_make_key(&key, strkey)) {

            /* run cache defense */
            if (!apc_cache_defense(cache, &key)) {
                
                /* initialize the entry for insertion */
                if ((entry = apc_cache_make_entry(&ctxt, &key, val, ttl))) {
                
                    /* execute an insertion */
                    if (apc_cache_insert_internal(cache, &key, entry, &ctxt, t, exclusive)) {
                        ret = 1;
                    }
                }
            }
        }

        /* in any case of failure the context should be destroyed */
        if (!ret) {
            apc_cache_destroy_context(&ctxt);
        }
    }
    return ret;
}

static inline apc_cache_entry_t* apc_cache_find_internal(apc_cache_t *cache, zend_string *key, time_t t, zend_bool lock) {
	apc_cache_slot_t** slot;
	zend_ulong h, s;

    volatile apc_cache_entry_t* value = NULL;

	if (lock)
		APC_RLOCK(cache->header);
    
	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	/* find head */
	slot = &cache->slots[s];

	while (*slot) {
		/* check for a matching key by has and identifier */
		if ((h == ZSTR_HASH((*slot)->key.str)) && 
			memcmp(ZSTR_VAL((*slot)->key.str), ZSTR_VAL(key), ZSTR_LEN(key)) == SUCCESS) {

			/* Check to make sure this entry isn't expired by a hard TTL */
			if((*slot)->value->ttl && (time_t) ((*slot)->ctime + (*slot)->value->ttl) < t) {
				/* increment misses on cache */
				ATOMIC_INC(cache, cache->header->nmisses);

				if (lock)
					APC_RUNLOCK(cache->header);
				return NULL;
			}
			
			/* set cache num hits */
			ATOMIC_INC(cache, cache->header->nhits);

			/* grab value */
			value = (*slot)->value;

			(*slot)->atime = t;

			/* Otherwise we are fine, increase counters and return the cache entry */
			ATOMIC_INC(cache, (*slot)->nhits);
			ATOMIC_INC(cache, (*slot)->value->ref_count);

			if (lock)
				APC_RUNLOCK(cache->header);

			return (apc_cache_entry_t*)value;
		}

		/* next */
		slot = &(*slot)->next;		
	}

	/* not found, so increment misses */
	ATOMIC_INC(cache, cache->header->nmisses);

	if (lock)
		APC_RUNLOCK(cache->header);

	return NULL;
}

static inline zend_bool apc_cache_fetch_internal(apc_cache_t* cache, zend_string *key, apc_cache_entry_t *entry, time_t t, zval **dst) {
	/* context for copying out */
	apc_context_t ctxt = {0, };
    zval *rv;

	/* create unpool context */
	if (apc_cache_make_context(cache, &ctxt, APC_CONTEXT_NOSHARE, APC_UNPOOL, APC_COPY_OUT, 0)) {

		/* copy to destination */
		rv = apc_cache_fetch_zval(&ctxt, *dst, &entry->val);

		/* release entry */
		apc_cache_release(cache, entry);

		/* destroy context */
		apc_cache_destroy_context(&ctxt );

		return (rv != NULL) ? 1 : 0;
	}

	return 0;
}

/* {{{ apc_cache_store */
PHP_APCU_API zend_bool apc_cache_store(apc_cache_t* cache, zend_string *strkey, const zval *val, const int32_t ttl, const zend_bool exclusive) {
    apc_cache_entry_t *entry;
    apc_cache_key_t key;
    time_t t;
    apc_context_t ctxt={0,};
    zend_bool ret = 0;

    t = apc_time();

	/* initialize a context suitable for making an insert */
    if (apc_cache_make_context(cache, &ctxt, APC_CONTEXT_SHARE, APC_SMALL_POOL, APC_COPY_IN, 0)) {

        /* initialize the key for insertion */
        if (apc_cache_make_key(&key, strkey)) {

            /* run cache defense */
            if (!apc_cache_defense(cache, &key)) {
                
                /* initialize the entry for insertion */
                if ((entry = apc_cache_make_entry(&ctxt, &key, val, ttl))) {
                
                    /* execute an insertion */
                    if (apc_cache_insert(cache, &key, entry, &ctxt, t, exclusive)) {
                        ret = 1;
                    }
                }
            }
        }

        /* in any case of failure the context should be destroyed */
        if (!ret) {
            apc_cache_destroy_context(&ctxt);
        }
    }

    return ret;
} /* }}} */

#ifndef ZTS
/* {{{ data_unserialize */
static zval data_unserialize(const char *filename)
{
    zval retval;
    zend_long len = 0;
    zend_stat_t sb;
    char *contents, *tmp;
    FILE *fp;
    php_unserialize_data_t var_hash = {0,};

    if(VCWD_STAT(filename, &sb) == -1) {
        return EG(uninitialized_zval);
    }

    fp = fopen(filename, "rb");

    len = sizeof(char)*sb.st_size;

    tmp = contents = malloc(len);

    if(!contents) {
        fclose(fp);
        return EG(uninitialized_zval);
    }

    if(fread(contents, 1, len, fp) < 1) {	
        fclose(fp);
        free(contents);
        return EG(uninitialized_zval);
    }

    ZVAL_UNDEF(&retval);

    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    
    /* I wish I could use json */
    if(!php_var_unserialize(&retval, (const unsigned char**)&tmp, (const unsigned char*)(contents+len), &var_hash)) {
        fclose(fp);
        free(contents);
        return EG(uninitialized_zval);
    }

    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

    free(contents);
    fclose(fp);

    return retval;
}

static int apc_load_data(apc_cache_t* cache, const char *data_file)
{
    char *p;
    char key[MAXPATHLEN] = {0,};
    unsigned int key_len;
    zval data;

    p = strrchr(data_file, DEFAULT_SLASH);

    if(p && p[1]) {
        strlcpy(key, p+1, sizeof(key));
        p = strrchr(key, '.');

        if(p) {
            p[0] = '\0';
            key_len = strlen(key);

            data = data_unserialize(data_file);
            if(Z_TYPE(data) != IS_UNDEF) {
				zend_string *name = zend_string_init(key, key_len, 0);
                apc_cache_store(
					cache, name, &data, 0, 1);
				zend_string_release(name);
				zval_dtor(&data);
            }
            return 1;
        }
    }

    return 0;
}
#endif

/* {{{ apc_cache_preload shall load the prepared data files in path into the specified cache */
PHP_APCU_API zend_bool apc_cache_preload(apc_cache_t* cache, const char *path)
{
#ifndef ZTS	
	zend_bool result = 0;
	char file[MAXPATHLEN]={0,};
	int ndir, i;
	char *p = NULL;
	struct dirent **namelist = NULL;

	if ((ndir = php_scandir(path, &namelist, 0, php_alphasort)) > 0) {
		for (i = 0; i < ndir; i++) {
			/* check for extension */
			if (!(p = strrchr(namelist[i]->d_name, '.'))
					|| (p && strcmp(p, ".data"))) {
				free(namelist[i]);
				continue;
			}

			snprintf(file, MAXPATHLEN, "%s%c%s",
					path, DEFAULT_SLASH, namelist[i]->d_name);

			if(apc_load_data(cache, file)) {
				result = 1;
			}
			free(namelist[i]);
		}
		free(namelist);
	}
	return result;
#else 
	apc_error("Cannot load data from apc.preload_path=%s in thread-safe mode", path);
	return 0;
#endif	
} /* }}} */

/* {{{ apc_cache_release */
PHP_APCU_API void apc_cache_release(apc_cache_t* cache, apc_cache_entry_t* entry)
{
    ATOMIC_DEC(cache, entry->ref_count);
}
/* }}} */

/* {{{ apc_cache_destroy */
PHP_APCU_API void apc_cache_destroy(apc_cache_t* cache)
{
	if (!cache) {
		return;
	}

	/* destroy lock */
	DESTROY_LOCK(&cache->header->lock);

	/* XXX this is definitely a leak, but freeing this causes all the apache
		children to freeze. It might be because the segment is shared between
		several processes. To figure out is how to free this safely. */
    /*apc_sma_free(cache->shmaddr);*/
    apc_efree(cache);
}
/* }}} */

/* {{{ apc_cache_real_expunge */
PHP_APCU_API void apc_cache_real_expunge(apc_cache_t* cache) {
	/* increment counter */	
	cache->header->nexpunges++;
    
	/* expunge */
    {
		zend_ulong i;

		for (i = 0; i < cache->nslots; i++) {
			apc_cache_slot_t* p = cache->slots[i];
			while (p) {
				apc_cache_remove_slot(cache, &p);
			}
			cache->slots[i] = NULL;
		}
	}
	
	/* set new time so counters make sense */
	cache->header->stime = apc_time();

	/* reset counters */
	cache->header->ninserts = 0;
	cache->header->nentries = 0;
    cache->header->nhits = 0;
    cache->header->nmisses = 0;
	
	/* resets lastkey */
	memset(&cache->header->lastkey, 0, sizeof(apc_cache_key_t));
} /* }}} */

/* {{{ apc_cache_clear */
PHP_APCU_API void apc_cache_clear(apc_cache_t* cache)
{
	/* check there is a cache and it is not busy */
    if(!cache || apc_cache_busy(cache)) {
		return;
	}
	
	/* lock header */
	APC_LOCK(cache->header);

	/* set busy */
	cache->header->state |= APC_CACHE_ST_BUSY;

	/* expunge cache */
	apc_cache_real_expunge(cache);

	/* set info */
	cache->header->stime = apc_time();
	cache->header->nexpunges = 0;

	/* unset busy */
	cache->header->state &= ~APC_CACHE_ST_BUSY;

	/* unlock header */
	APC_UNLOCK(cache->header);
}
/* }}} */

/* {{{ apc_cache_default_expunge */
PHP_APCU_API void apc_cache_default_expunge(apc_cache_t* cache, size_t size)
{
    time_t t;
	size_t suitable = 0L;
    size_t available = 0L;

    t = apc_time();

	/* check there is a cache, and it is not busy */
    if(!cache || apc_cache_busy(cache)) {
		return;
	}
	
	/* get the lock for header */
	APC_LOCK(cache->header);

	/* update state in header */
	cache->header->state |= APC_CACHE_ST_BUSY;

	/* make suitable selection */
	suitable = (cache->smart > 0L) ? (size_t) (cache->smart * size) : (size_t) (cache->sma->size/2);

	/* gc */
	apc_cache_gc(cache);

	/* get available */
	available = cache->sma->get_avail_mem();

	/* perform expunge processing */
	if(!cache->ttl) {

		/* check it is necessary to expunge */
		if (available < suitable) {
			apc_cache_real_expunge(cache);
		}
	} else {
		apc_cache_slot_t **slot;

		/* check that expunge is necessary */
	    if (available < suitable) {
			zend_ulong i;

			/* look for junk */
			for (i = 0; i < cache->nslots; i++) {
				slot = &cache->slots[i];
				while (*slot) {
					/*
					 * Entry TTL has precedence over cache TTL
					 */
					if((*slot)->value->ttl) {
						if((time_t) ((*slot)->ctime + (*slot)->value->ttl) < t) {
							apc_cache_remove_slot(cache, slot);
							continue;
						}
					} else if(cache->ttl) {
						if((time_t) ((*slot)->ctime + cache->ttl) < t) {
							apc_cache_remove_slot(cache, slot);
							continue;
						}
					}
				
					/* grab next slot */
					slot = &(*slot)->next;				
				}
			}

			/* if the cache now has space, then reset last key */
			if (cache->sma->get_avail_size(size)) {
				/* wipe lastkey */
				memset(&cache->header->lastkey, 0, sizeof(apc_cache_key_t));
			} else {
				/* with not enough space left in cache, we are forced to expunge */
				apc_cache_real_expunge(cache);
			}
	    }
	}

	/* we are done */
	cache->header->state &= ~APC_CACHE_ST_BUSY;

	/* unlock header */
	APC_UNLOCK(cache->header);
}
/* }}} */

/* {{{ apc_cache_make_context */
PHP_APCU_API zend_bool apc_cache_make_context(apc_cache_t* cache,
                                              apc_context_t* context,
                                              apc_context_type context_type,
                                              apc_pool_type pool_type,
                                              apc_copy_type copy_type,
                                              uint force_update) {
	switch (context_type) {
		case APC_CONTEXT_SHARE: {
			return apc_cache_make_context_ex(
				context,
                cache->serializer,
				(apc_malloc_t) cache->sma->smalloc,
                cache->sma->sfree,
                cache->sma->protect,
                cache->sma->unprotect,
				pool_type, copy_type, force_update
			);
		} break;
		
		case APC_CONTEXT_NOSHARE: {
			return apc_cache_make_context_ex(
				context,
                cache->serializer,
				apc_php_malloc, apc_php_free, NULL, NULL,
				pool_type, copy_type, force_update
			);
		} break;

		case APC_CONTEXT_NONE:
			/* never used, just to make gcc warning free */
			break;
	}

	return 0;
} /* }}} */

/* {{{ apc_cache_make_context_ex */
PHP_APCU_API zend_bool apc_cache_make_context_ex(apc_context_t* context,
                                                 apc_serializer_t* serializer,
                                                 apc_malloc_t _malloc, 
                                                 apc_free_t _free, 
                                                 apc_protect_t _protect, 
                                                 apc_unprotect_t _unprotect, 
                                                 apc_pool_type pool_type, 
                                                 apc_copy_type copy_type, 
                                                 uint force_update) {
	/* attempt to create the pool */
	context->pool = apc_pool_create(
		pool_type, _malloc, _free, _protect, _unprotect
	);

	if (!context->pool) {
		apc_warning("Unable to allocate memory for pool.");
		return 0;
	}

	/* set context information */
	context->serializer = serializer;
	context->copy = copy_type;
	context->force_update = force_update;

	/* set this to avoid memory errors */
	memset(&context->copied, 0, sizeof(HashTable));

	return 1;
} /* }}} */

/* {{{ apc_context_destroy */
PHP_APCU_API zend_bool apc_cache_destroy_context(apc_context_t* context) {
    if (!context->pool) {
        return 0;
    }

    apc_pool_destroy(context->pool);

    return 1;
} /* }}} */

/* {{{ apc_cache_insert */
PHP_APCU_API zend_bool apc_cache_insert(apc_cache_t* cache, 
                                        apc_cache_key_t *key, 
                                        apc_cache_entry_t* value, 
                                        apc_context_t* ctxt, 
                                        time_t t, 
                                        zend_bool exclusive)
{
	zend_bool result = 0;

	php_apc_try(APC_LOCK(cache->header), {
		result = apc_cache_insert_internal(
			cache, key, value, ctxt, t, exclusive);
	}, APC_UNLOCK(cache->header));

	return result;	
}
/* }}} */

/* {{{ apc_cache_find */
PHP_APCU_API apc_cache_entry_t* apc_cache_find(apc_cache_t* cache, zend_string *key, time_t t)
{
	apc_cache_entry_t *entry = NULL;

	/* check we are able to deal with the request */
    if(!cache || apc_cache_busy(cache)) {
        return entry;
    }

	entry = apc_cache_find_internal(cache, key, t, 1);

	return entry;
}
/* }}} */

/* {{{ apc_cache_fetch */
PHP_APCU_API zend_bool apc_cache_fetch(apc_cache_t* cache, zend_string *key, time_t t, zval **dst) 
{
	apc_cache_entry_t *entry = NULL;
	zend_bool ret = 0;

	/* check we are able to deal with the request */
    if(!cache || apc_cache_busy(cache)) {
        return 0;
    }

	entry = apc_cache_find_internal(cache, key, t, 1);

	if (entry) {
		ret = apc_cache_fetch_internal(cache, key, entry, t, dst);
	}
	
	return ret;
} /* }}} */

/* {{{ apc_cache_exists */
PHP_APCU_API apc_cache_entry_t* apc_cache_exists(apc_cache_t* cache, zend_string *key, time_t t)
{
    if(apc_cache_busy(cache))
    {
        /* cache cleanup in progress */ 
        return NULL;
    }

	/* we only declare volatiles we need */
	{
		apc_cache_slot_t** slot;
	
		volatile apc_cache_entry_t* value = NULL;
		zend_ulong h, s;

        /* get hash and slot */
		apc_cache_hash_slot(cache, key, &h, &s);

        /* read lock header */
		APC_RLOCK(cache->header);	

		/* find head */
		slot = &cache->slots[s];

		while (*slot) {
			/* check for match by hash and identifier */
			if ((h == ZSTR_HASH((*slot)->key.str)) &&
				memcmp(ZSTR_VAL((*slot)->key.str), ZSTR_VAL(key), ZSTR_LEN(key)) == SUCCESS) {

				/* Check to make sure this entry isn't expired by a hard TTL */
				if((*slot)->value->ttl && (time_t) ((*slot)->ctime + (*slot)->value->ttl) < t) {
                    /* marked as a miss */
                    ATOMIC_INC(cache, cache->header->nmisses);

					/* unlock header */
					APC_RUNLOCK(cache->header);

					return NULL;
				}

				/* Return the cache entry ptr */
				value = (*slot)->value;
			
				/* unlock header */
				APC_RUNLOCK(cache->header);
					
				return (apc_cache_entry_t*)value;
			}

			slot = &(*slot)->next;  
		}

		/* unlock header */
		APC_RUNLOCK(cache->header);
	}

    return NULL;
}
/* }}} */

/* {{{ apc_cache_update */
PHP_APCU_API zend_bool apc_cache_update(apc_cache_t* cache, zend_string *key, apc_cache_updater_t updater, void* data)
{
    apc_cache_slot_t** slot;
    apc_cache_entry_t tmp_entry;
	
    zend_bool retval = 0;
    zend_ulong h, s;

    if(apc_cache_busy(cache))
    {
        /* cannot service request right now */ 
        return 0;
    }

    /* calculate hash */
    apc_cache_hash_slot(cache, key, &h, &s);
	
	php_apc_try(APC_LOCK(cache->header), {

		/* find head */
		slot = &cache->slots[s];

		while (*slot) {
			/* check for a match by hash and identifier */
		    if ((h == ZSTR_HASH((*slot)->key.str)) &&
		        memcmp(ZSTR_VAL((*slot)->key.str), ZSTR_VAL(key), ZSTR_LEN(key)) == SUCCESS) {
				/* attempt to perform update */
		        switch(Z_TYPE((*slot)->value->val)) {
		            case IS_ARRAY:
		            case IS_OBJECT:
		            {
		                if(cache->serializer) {
		                    retval = 0;
		                    break;
		                }
		            }

		            /* break intentionally omitted */

		            default:
		            {
						/* executing update */
		                retval = updater(cache, (*slot)->value, data);
						/* set modified time */
		                (*slot)->key.mtime = apc_time();
		            }
		            break;
		        }

				php_apc_try_end({
					APC_UNLOCK(cache->header);
					return retval;
				});
		    }

			/* set next slot */
		    slot = &(*slot)->next;
		}
	}, APC_UNLOCK(cache->header));

	/* failed to find matching entry, create it */
	ZVAL_LONG(&tmp_entry.val, 0);
	updater(cache, &tmp_entry, data);

	if(apc_cache_store(cache, key, &tmp_entry.val, 0, 0)) {
		return 1;
	}

    return 0;
}
/* }}} */

/* {{{ apc_cache_delete */
PHP_APCU_API zend_bool apc_cache_delete(apc_cache_t* cache, zend_string *key)
{
    apc_cache_slot_t** slot;
	
    zend_ulong h, s;

	if (!cache) {
		return 1;
	}

    /* calculate hash and slot */
    apc_cache_hash_slot(cache, key, &h, &s);

	/* lock cache */
	APC_LOCK(cache->header);
	
	/* find head */
    slot = &cache->slots[s];

    while (*slot) {
		/* check for a match by hash and identifier */
        if ((h == ZSTR_HASH((*slot)->key.str)) && 
            memcmp(ZSTR_VAL((*slot)->key.str), ZSTR_VAL(key), ZSTR_LEN(key)) == SUCCESS) {
			/* executing removal */
            apc_cache_remove_slot(
				cache, slot);
			goto deleted;
        }
		
		/* continue locking */
		slot = &(*slot)->next;      
    }
	
	/* unlock header */
	APC_UNLOCK(cache->header);
	
	return 0;

deleted:
	/* unlock deleted */
	APC_UNLOCK(cache->header);

	return 1;
}
/* }}} */

/* {{{ apc_cache_make_key */
PHP_APCU_API zend_bool apc_cache_make_key(apc_cache_key_t* key, zend_string *str)
{
    assert(key != NULL);

    if (!str) {
		return 0;
    }
	
    key->str = str;
    key->mtime = apc_time();

    return 1;
}
/* }}} */

/* {{{ my_serialize_object */
static zval* my_serialize_object(zval* dst, const zval* src, apc_context_t* ctxt)
{
    unsigned char *buf = NULL;
	size_t buf_len = 0;
	
    apc_pool* pool = ctxt->pool;
    apc_serialize_t serialize = APC_SERIALIZER_NAME(php);
    void *config = NULL;
	zend_string *serial = NULL;

    if(ctxt->serializer) {
        serialize = ctxt->serializer->serialize;
        config = 
			(ctxt->serializer->config != NULL) ? 
			ctxt->serializer->config : ctxt;
    }

	ZVAL_NULL(dst);

    if(serialize((unsigned char**)&buf, &buf_len, src, config)) {
		if (!(serial = apc_pstrnew(buf, buf_len, pool))) {	
			efree(buf);

			return dst;
		}

		ZVAL_STR(dst, serial);
		/* Give this the type of an object/array, and the same type flags as a string. */
		/* The only necessary one is probably IS_TYPE_REFCOUNTED - (We check Z_REFCOUNTED_P when de-duplicating serialized values. */
		/* Probably safe - When copying strings into shared memory, the code gives it type IS_STRING_EX, */
		Z_TYPE_INFO_P(dst) = Z_TYPE_P(src) | ((IS_TYPE_REFCOUNTED | IS_TYPE_COPYABLE) << Z_TYPE_FLAGS_SHIFT);
		efree(buf);
    }

    return dst;
}
/* }}} */

/* {{{ my_unserialize_object */
static zval* my_unserialize_object(zval* dst, const zval* src, apc_context_t* ctxt)
{
    apc_unserialize_t unserialize = APC_UNSERIALIZER_NAME(php);
    void *config = NULL;

    if(ctxt->serializer) {
        unserialize = ctxt->serializer->unserialize;
        config = (ctxt->serializer->config != NULL) ? ctxt->serializer->config : ctxt;
    }

    if(unserialize(dst, (unsigned char *)Z_STRVAL_P(src), Z_STRLEN_P(src), config)) {
        return dst;
    } else {
        zval_dtor(dst);
        ZVAL_NULL(dst);
    }
    return dst;
}
/* }}} */

static const uint32_t uninitialized_bucket[-HT_MIN_MASK] = {HT_INVALID_IDX, HT_INVALID_IDX};

static zend_always_inline int apc_array_dup_element(apc_context_t *ctxt, HashTable *source, HashTable *target, uint32_t idx, Bucket *p, Bucket *q, int packed, int static_keys, int with_holes)
{
	zval *data = &p->val;

	if (with_holes) {
		if (!packed && Z_TYPE_INFO_P(data) == IS_INDIRECT) {
			data = Z_INDIRECT_P(data);
		}
		if (UNEXPECTED(Z_TYPE_INFO_P(data) == IS_UNDEF)) {
			return 0;
		}
	} else if (!packed) {
		/* INDIRECT element may point to UNDEF-ined slots */
		if (Z_TYPE_INFO_P(data) == IS_INDIRECT) {
			data = Z_INDIRECT_P(data);
			if (UNEXPECTED(Z_TYPE_INFO_P(data) == IS_UNDEF)) {
				return 0;
			}
		}
	}

	do {
		if (Z_OPT_REFCOUNTED_P(data)) {
			if (Z_ISREF_P(data) && Z_REFCOUNT_P(data) == 1 &&
				(Z_TYPE_P(Z_REFVAL_P(data)) != IS_ARRAY ||
				  Z_ARRVAL_P(Z_REFVAL_P(data)) != source)) {
				data = Z_REFVAL_P(data);
				if (!Z_OPT_REFCOUNTED_P(data)) {
					break;
				}
			}
		}
	} while (0);

	if (my_copy_zval(&q->val, data, ctxt) == NULL)
        return 0;

	q->h = p->h;
	if (packed) {
		q->key = NULL;
	} else {
		uint32_t nIndex;

		q->key = p->key;
		if (!static_keys && q->key) {
			if (ctxt->copy == APC_COPY_IN) {
				q->key = apc_pstrcpy(q->key, ctxt->pool);
			} else q->key = p->key;
		}

		nIndex = q->h | target->nTableMask;
		Z_NEXT(q->val) = HT_HASH(target, nIndex);
		HT_HASH(target, nIndex) = HT_IDX_TO_HASH(idx);
	}
	return 1;
}

static zend_always_inline void apc_array_dup_packed_elements(apc_context_t *ctxt, HashTable *source, HashTable *target, int with_holes)
{
	Bucket *p = source->arData;
	Bucket *q = target->arData;
	Bucket *end = p + source->nNumUsed;

	do {
		if (!apc_array_dup_element(ctxt, source, target, 0, p, q, 1, 1, with_holes)) {
			if (with_holes) {
				ZVAL_UNDEF(&q->val);
			}
		}
		p++; q++;
	} while (p != end);
}

static zend_always_inline uint32_t apc_array_dup_elements(apc_context_t *ctxt, HashTable *source, HashTable *target, int static_keys, int with_holes)
{
    uint32_t idx = 0;
	Bucket *p = source->arData;
	Bucket *q = target->arData;
	Bucket *end = p + source->nNumUsed;

	do {
		if (!apc_array_dup_element(ctxt, source, target, idx, p, q, 0, static_keys, with_holes)) {
			uint32_t target_idx = idx;

			idx++; p++;
			while (p != end) {
				if (apc_array_dup_element(ctxt, source, target, target_idx, p, q, 0, static_keys, with_holes)) {
					if (source->nInternalPointer == idx) {
						target->nInternalPointer = target_idx;
					}
					target_idx++; q++;
				}
				idx++; p++;
			}
			return target_idx;
		}
		idx++; p++; q++;
	} while (p != end);
	return idx;
}

static APC_HOTSPOT HashTable* my_copy_hashtable(HashTable *source, apc_context_t *ctxt) {
	uint32_t idx;
	HashTable *target;
	apc_pool *pool = ctxt->pool;

	if (ctxt->copy == APC_COPY_IN) {
		target = (HashTable*) pool->palloc(pool, sizeof(HashTable));
	} else
        ALLOC_HASHTABLE(target);

    if (target == NULL)
        goto bad;

#if PHP_VERSION_ID >= 70300
	GC_SET_REFCOUNT(target, 1);
#else
	GC_REFCOUNT(target) = 1;
#endif
	GC_TYPE_INFO(target) = IS_ARRAY;
    zend_hash_index_update_ptr(&ctxt->copied, (uintptr_t) source, target);

	target->nTableSize = source->nTableSize;
	target->pDestructor = source->pDestructor;

	if (source->nNumUsed == 0) {
#if PHP_VERSION_ID >= 70300
		target->u.flags = (source->u.flags & ~(HASH_FLAG_INITIALIZED|HASH_FLAG_PACKED)) | HASH_FLAG_STATIC_KEYS;
#else
		target->u.flags = (source->u.flags & ~(HASH_FLAG_INITIALIZED|HASH_FLAG_PACKED|HASH_FLAG_PERSISTENT)) | HASH_FLAG_APPLY_PROTECTION | HASH_FLAG_STATIC_KEYS;
#endif
		target->nTableMask = HT_MIN_MASK;
		target->nNumUsed = 0;
		target->nNumOfElements = 0;
		target->nNextFreeElement = 0;
		target->nInternalPointer = HT_INVALID_IDX;
		HT_SET_DATA_ADDR(target, &uninitialized_bucket);
	} else if (GC_FLAGS(source) & IS_ARRAY_IMMUTABLE) {
#if PHP_VERSION_ID < 70300
		target->u.flags = (source->u.flags & ~HASH_FLAG_PERSISTENT) | HASH_FLAG_APPLY_PROTECTION;
#endif
		target->nTableMask = source->nTableMask;
		target->nNumUsed = source->nNumUsed;
		target->nNumOfElements = source->nNumOfElements;
		target->nNextFreeElement = source->nNextFreeElement;
		if (ctxt->copy == APC_COPY_IN) {
			HT_SET_DATA_ADDR(target, pool->palloc(pool, HT_SIZE(target)));
		} else
            HT_SET_DATA_ADDR(target, emalloc(HT_SIZE(target)));

        if (HT_GET_DATA_ADDR(target) == NULL)
            goto bad;

		target->nInternalPointer = source->nInternalPointer;
		memcpy(HT_GET_DATA_ADDR(target), HT_GET_DATA_ADDR(source), HT_USED_SIZE(source));
		if (target->nNumOfElements > 0 &&
			target->nInternalPointer == HT_INVALID_IDX) {
			idx = 0;
			while (Z_TYPE(target->arData[idx].val) == IS_UNDEF) {
				idx++;
			}
			target->nInternalPointer = idx;
		}
	} else if (source->u.flags & HASH_FLAG_PACKED) {
#if PHP_VERSION_ID < 70300
		target->u.flags = (source->u.flags & ~HASH_FLAG_PERSISTENT) | HASH_FLAG_APPLY_PROTECTION;
#endif
		target->nTableMask = source->nTableMask;
		target->nNumUsed = source->nNumUsed;
		target->nNumOfElements = source->nNumOfElements;
		target->nNextFreeElement = source->nNextFreeElement;
		if (ctxt->copy == APC_COPY_IN) {
			HT_SET_DATA_ADDR(target, pool->palloc(pool, HT_SIZE(target)));
		} else
            HT_SET_DATA_ADDR(target, emalloc(HT_SIZE(target)));

        if (HT_GET_DATA_ADDR(target) == NULL)
            goto bad;

		target->nInternalPointer = source->nInternalPointer;
		HT_HASH_RESET_PACKED(target);

		if (target->nNumUsed == target->nNumOfElements) {
			apc_array_dup_packed_elements(ctxt, source, target, 0);
		} else {
			apc_array_dup_packed_elements(ctxt, source, target, 1);
		}
		if (target->nNumOfElements > 0 &&
			target->nInternalPointer == HT_INVALID_IDX) {
			idx = 0;
			while (Z_TYPE(target->arData[idx].val) == IS_UNDEF) {
				idx++;
			}
			target->nInternalPointer = idx;
		}
	} else {
#if PHP_VERSION_ID < 70300
		target->u.flags = (source->u.flags & ~HASH_FLAG_PERSISTENT) | HASH_FLAG_APPLY_PROTECTION;
#endif
		target->nTableMask = source->nTableMask;
		target->nNextFreeElement = source->nNextFreeElement;
		target->nInternalPointer = HT_INVALID_IDX;
		if (ctxt->copy == APC_COPY_IN) {
			HT_SET_DATA_ADDR(target, pool->palloc(pool, HT_SIZE(target)));
		} else
            HT_SET_DATA_ADDR(target, emalloc(HT_SIZE(target)));

        if (HT_GET_DATA_ADDR(target) == NULL)
            goto bad;

		HT_HASH_RESET(target);

		if (target->u.flags & HASH_FLAG_STATIC_KEYS) {
			if (source->nNumUsed == source->nNumOfElements) {
				idx = apc_array_dup_elements(ctxt, source, target, 1, 0);
			} else {
				idx = apc_array_dup_elements(ctxt, source, target, 1, 1);
			}
		} else {
			if (source->nNumUsed == source->nNumOfElements) {
				idx = apc_array_dup_elements(ctxt, source, target, 0, 0);
			} else {
				idx = apc_array_dup_elements(ctxt, source, target, 0, 1);
			}
		}
		target->nNumUsed = idx;
		target->nNumOfElements = idx;
		if (idx > 0 && target->nInternalPointer == HT_INVALID_IDX) {
			target->nInternalPointer = 0;
		}
	}
	return target;

  bad:
    /* some kind of memory allocation failure */
    if (target) {
	    if (ctxt->copy == APC_COPY_IN) {
            pool->pfree(pool, target);
	    } else {
            FREE_HASHTABLE(target);
        }
    }

    return NULL;
}

static APC_HOTSPOT zend_reference* my_copy_reference(const zend_reference* src, apc_context_t *ctxt) {
	apc_pool* pool = ctxt->pool;
	zend_reference *dst;

	assert(src != NULL);

	if (ctxt->copied.nTableSize) {
		zend_reference *rc = zend_hash_index_find_ptr(&ctxt->copied, (uintptr_t) src);
		if (rc) {
#if PHP_VERSION_ID >= 70300
			GC_ADDREF(rc);
#else
			GC_REFCOUNT(rc)++;
#endif
			return rc;
		}
	}

    if (ctxt->copy == APC_COPY_IN) {
        dst = pool->palloc(pool, sizeof(zend_reference));
    } else {
        dst = emalloc(sizeof(zend_reference));
    }

    if (dst == NULL)
        return NULL;

#if PHP_VERSION_ID >= 70300
    GC_SET_REFCOUNT(dst, 1);
#else
    GC_REFCOUNT(dst) = 1;
#endif
    GC_TYPE_INFO(dst) = IS_REFERENCE;

    if (my_copy_zval(&dst->val, &src->val, ctxt) == NULL) {
        return NULL;
    }

	if (ctxt->copied.nTableSize) {
		zend_hash_index_update_ptr(&ctxt->copied, (uintptr_t) src, dst);
	}

    return dst;
}

/* {{{ my_copy_zval */
/* This function initializes *dst (temporary zval with request lifetime) with the (possibly serialized) contents of the serialized value in *src */
static APC_HOTSPOT zval* my_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt)
{
    apc_pool* pool = ctxt->pool;

    assert(dst != NULL);
    assert(src != NULL);

	/* If src was already unserialized, then make dst a copy of the unserialization of src */
	if (Z_REFCOUNTED_P(src)) {
		if (zend_hash_num_elements(&ctxt->copied)) {
            zval *rc = zend_hash_index_find(
                    &ctxt->copied, (uintptr_t) Z_COUNTED_P(src));
            if (rc) {
                /* this does not allocate memory, so always succeeds */
				ZVAL_COPY(dst, rc);
				return dst;
            }
		}
	}

	/* Copy the raw bytes of the type, value, and type flags */
	memcpy(dst, src, sizeof(zval));

    switch (Z_TYPE_P(src)) {
    case IS_RESOURCE:
    case IS_TRUE:
    case IS_FALSE:
    case IS_LONG:
    case IS_DOUBLE:
    case IS_NULL:
        /* No additional work for scalars, they aren't ref counted */
        break;

	case IS_REFERENCE:
		if ((Z_REF_P(dst) = my_copy_reference(Z_REF_P(src), ctxt)) == NULL)
            return NULL;
	    break;

	case IS_INDIRECT:
		if (my_copy_zval(dst, Z_INDIRECT_P(src), ctxt) == NULL)
            return NULL;
	    break;

#ifdef IS_CONSTANT
    case IS_CONSTANT:
#endif
    case IS_STRING:	
		if (ctxt->copy == APC_COPY_OUT) {
			ZVAL_DUP(dst, src);
		} else {
			Z_TYPE_INFO_P(dst) = IS_STRING_EX;
			Z_STR_P(dst) = apc_pstrcpy(Z_STR_P(src), pool);
		}
        if (Z_STR_P(dst) == NULL)
            return NULL;
        break;

    case IS_ARRAY:
        if(ctxt->serializer == NULL) {
			if ((Z_ARRVAL_P(dst) = my_copy_hashtable(Z_ARRVAL_P(src), ctxt)) == NULL)
                return NULL;
            break;
        }

		/* break intentionally omitted */

    case IS_OBJECT:
        if (ctxt->copy == APC_COPY_IN) {
            /* For objects and arrays, their pointer points to a serialized string instead of a zend_array or zend_object. */
            /* Unserialize that, and on success, give dst the default type flags for an object/array (We check Z_REFCOUNTED_P below). */
            dst = my_serialize_object(dst, src, ctxt);
        } else
            dst = my_unserialize_object(dst, src, ctxt);
        if (dst == NULL)
            return NULL;
        break;

    default:
        assert(0);
    }

	if (Z_REFCOUNTED_P(dst) && ctxt->copied.nTableSize) {
		zend_hash_index_update(&ctxt->copied, (uintptr_t) Z_COUNTED_P(src), dst);
	}

    return dst;
}
/* }}} */

/* {{{ apc_copy_zval */
PHP_APCU_API zval* apc_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt)
{
    return my_copy_zval(dst, src, ctxt);
}
/* }}} */

/* {{{ apc_cache_store_zval */
PHP_APCU_API zval* apc_cache_store_zval(zval* dst, const zval* src, apc_context_t* ctxt)
{
    if (Z_TYPE_P(src) == IS_ARRAY) {
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        zend_hash_init(&ctxt->copied, 16, NULL, NULL, 0);
        dst = apc_copy_zval(dst, src, ctxt);
        /* remove from copied regardless if allocation failure */
        zend_hash_destroy(&ctxt->copied);
        ctxt->copied.nTableSize=0;
    } else {
        dst = apc_copy_zval(dst, src, ctxt);
    }

	if (dst == NULL || EG(exception)) {
		return NULL;
	}

    return dst;
}
/* }}} */

/* {{{ apc_cache_fetch_zval */
PHP_APCU_API zval* apc_cache_fetch_zval(apc_context_t* ctxt, zval* dst, const zval* src)
{
    if (Z_TYPE_P(src) == IS_ARRAY) {
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        zend_hash_init(&ctxt->copied, 16, NULL, NULL, 0);
        dst = apc_copy_zval(dst, src, ctxt);
        /* remove from copied regardless if allocation failure */
        zend_hash_destroy(&ctxt->copied);
        ctxt->copied.nTableSize=0;
    } else {
        dst = apc_copy_zval(dst, src, ctxt);
    }

    return dst;
}
/* }}} */

/* {{{ apc_cache_make_entry */
PHP_APCU_API apc_cache_entry_t* apc_cache_make_entry(apc_context_t* ctxt, apc_cache_key_t *key, const zval* val, const int32_t ttl)
{
    apc_cache_entry_t* entry;
    apc_pool* pool = ctxt->pool;

    entry = (apc_cache_entry_t*) pool->palloc(pool, sizeof(apc_cache_entry_t));
    if (!entry) {
		return NULL;
	}
	
	/* set key for serializer */
	ctxt->key = key;
	
    if (!apc_cache_store_zval(&entry->val, val, ctxt)) {
        pool->pfree(pool, entry);
        return NULL;
    }

    entry->ttl = ttl;
    entry->ref_count = 0;
    entry->mem_size = 0;
    entry->pool = pool;
    return entry;
}
/* }}} */

/* {{{ apc_cache_link_info */
static zval apc_cache_link_info(apc_cache_t *cache, apc_cache_slot_t* p)
{
    zval link;

    array_init(&link);

    add_assoc_str(&link, "info", zend_string_dup(p->key.str, 0));
    add_assoc_long(&link, "ttl", p->value->ttl);

    add_assoc_double(&link, "num_hits", (double)p->nhits);
    add_assoc_long(&link, "mtime", p->key.mtime);
    add_assoc_long(&link, "creation_time", p->ctime);
    add_assoc_long(&link, "deletion_time", p->dtime);
    add_assoc_long(&link, "access_time", p->atime);
    add_assoc_long(&link, "ref_count", p->value->ref_count);
    add_assoc_long(&link, "mem_size", p->value->mem_size);

    return link;
}
/* }}} */

#if APC_MMAP
#define apc_cache_info_set_memory_type() add_assoc_stringl(&info, "memory_type", "mmap", sizeof("mmap")-1)
#else
#define apc_cache_info_set_memory_type() add_assoc_stringl(&info, "memory_type", "IPC shared", sizeof("IPC shared")-1)
#endif

/* {{{ apc_cache_info */
PHP_APCU_API zval apc_cache_info(apc_cache_t* cache, zend_bool limited)
{
    zval info;
    zval list;
    zval gc;
    zval slots;
    apc_cache_slot_t* p;
    zend_ulong i, j;

    if (!cache) {
		ZVAL_NULL(&info);

        return info;
    }

	php_apc_try(APC_RLOCK(cache->header), {

		array_init(&info);
		add_assoc_long(&info, "num_slots", cache->nslots);
		add_assoc_long(&info, "ttl", cache->ttl);
		add_assoc_double(&info, "num_hits", (double)cache->header->nhits);
		add_assoc_double(&info, "num_misses", (double)cache->header->nmisses);
		add_assoc_double(&info, "num_inserts", (double)cache->header->ninserts);
		add_assoc_long(&info,   "num_entries", cache->header->nentries);
		add_assoc_double(&info, "expunges", (double)cache->header->nexpunges);
		add_assoc_long(&info, "start_time", cache->header->stime);
		add_assoc_double(&info, "mem_size", (double)cache->header->mem_size);

		apc_cache_info_set_memory_type();

		if (!limited) {
		    /* For each hashtable slot */
		    array_init(&list);
		    array_init(&slots);

		    for (i = 0; i < cache->nslots; i++) {
		        p = cache->slots[i];
		        j = 0;
		        for (; p != NULL; p = p->next) {
		            zval link = apc_cache_link_info(cache, p);
		            add_next_index_zval(&list, &link);
		            j++;
		        }
		        if(j != 0) {
		            add_index_long(&slots, (ulong)i, j);
		        }
		    }

		    /* For each slot pending deletion */
		    array_init(&gc);

		    for (p = cache->header->gc; p != NULL; p = p->next) {
		        zval link = apc_cache_link_info(cache, p);
		        add_next_index_zval(&gc, &link);
		    }
		    
		    add_assoc_zval(&info, "cache_list", &list);
		    add_assoc_zval(&info, "deleted_list", &gc);
		    add_assoc_zval(&info, "slot_distribution", &slots);
		}

	}, APC_RUNLOCK(cache->header));

    return info;
}
/* }}} */
#undef apc_cache_info_set_memory_type

/*
 fetches information about the key provided
*/
PHP_APCU_API zval* apc_cache_stat(apc_cache_t* cache, zend_string *key, zval *stat) {
    apc_cache_slot_t** slot;
	zend_ulong h, s;

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	php_apc_try(APC_RLOCK(cache->header), {
		/* find head */
		slot = &cache->slots[s];

		while (*slot) {
			/* check for a matching key by has and identifier */
			if ((h == ZSTR_HASH((*slot)->key.str)) && 
				memcmp(ZSTR_VAL((*slot)->key.str), ZSTR_VAL(key), ZSTR_LEN(key)) == SUCCESS) {
		        array_init(stat);
		        
		        add_assoc_long(stat, "hits",  (*slot)->nhits);
		        add_assoc_long(stat, "access_time", (*slot)->atime);
		        add_assoc_long(stat, "mtime", (*slot)->key.mtime);
		        add_assoc_long(stat, "creation_time", (*slot)->ctime);
		        add_assoc_long(stat, "deletion_time", (*slot)->dtime);
				add_assoc_long(stat, "ttl",   (*slot)->value->ttl);
				add_assoc_long(stat, "refs",  (*slot)->value->ref_count);
			
				break;
			}

			/* next */
			slot = &(*slot)->next;		
		}
	}, APC_RUNLOCK(cache->header));

    return stat;
}

/* {{{ apc_cache_busy */
PHP_APCU_API zend_bool apc_cache_busy(apc_cache_t* cache)
{	
	return (cache->header->state & APC_CACHE_ST_BUSY);
}
/* }}} */

/* {{{ apc_cache_defense */
PHP_APCU_API zend_bool apc_cache_defense(apc_cache_t* cache, apc_cache_key_t* key)
{
	zend_bool result = 0;

#ifdef ZTS
#	define FROM_DIFFERENT_THREAD(k) ((key->owner = TSRMLS_CACHE) != (k)->owner) 
#else
#	define FROM_DIFFERENT_THREAD(k) ((key->owner = getpid()) != (k)->owner) 
#endif

	/* only continue if slam defense is enabled */
	if (cache->defend) {

		/* for copy of locking key struct */
		apc_cache_key_t *last = &cache->header->lastkey;

		if (!last->str) {
			return 0;
		}
		
		/* check the hash and length match */
		if(ZSTR_HASH(last->str) == ZSTR_HASH(key->str) && ZSTR_LEN(last->str) == ZSTR_LEN(key->str)) {
			/* check the time ( last second considered slam ) and context */
			if(last->mtime == key->mtime && FROM_DIFFERENT_THREAD(last)) {
				/* potential cache slam */
				apc_debug(
					"Potential cache slam averted for key '%s'", key->str);
				result = 1;
			} else {
				/* sets enough information for an educated guess, but is not exact */
				last->str = key->str;
				last->mtime = apc_time();

				/* required to tell contexts apart */
#ifdef ZTS
				last->owner = TSRMLS_CACHE;	
#else
				last->owner = getpid();		
#endif	
			}
		}
	}

    return result;
}
/* }}} */

/* {{{ apc_cache_serializer */
PHP_APCU_API void apc_cache_serializer(apc_cache_t* cache, const char* name) {
	if (cache && !cache->serializer) {
		cache->serializer = apc_find_serializer(name);
	}
} /* }}} */

#ifndef APC_LOCK_RECURSIVE
# define apc_cache_entry_try_begin() { \
	if (APCG(recursion)++ == 0) { \
		APC_LOCK(cache->header); \
	} \
}

# define apc_cache_entry_try_end() { \
	if (--APCG(recursion) == 0) { \
		APC_UNLOCK(cache->header); \
	} \
}
#else
# define apc_cache_entry_try_begin() APC_LOCK(cache->header);
# define apc_cache_entry_try_end() APC_UNLOCK(cache->header);
#endif

PHP_APCU_API void apc_cache_entry(apc_cache_t *cache, zval *key, zend_fcall_info *fci, zend_fcall_info_cache *fcc, zend_long ttl, zend_long now, zval *return_value) {/*{{{*/
	apc_cache_entry_t *entry = NULL;

	if(!cache || apc_cache_busy(cache)) {
        return;
    }

	if (!key || Z_TYPE_P(key) != IS_STRING) {
		/* only strings, for now */
		return;
	}

	php_apc_try(apc_cache_entry_try_begin(), {
		entry = apc_cache_find_internal(cache, Z_STR_P(key), now, 0);
		if (!entry) {
			int result = 0;

			fci->retval = return_value;
			zend_fcall_info_argn(fci, 1, key);

			zend_try {
				result = zend_call_function(fci, fcc);
			} zend_end_try ();

			if (result == SUCCESS) {
				zend_fcall_info_args_clear(fci, 1);
			
				if (!EG(exception)) {
					apc_cache_store_internal(
						cache, Z_STR_P(key), return_value, (uint32_t) ttl, 1);
				}
			}
		} else apc_cache_fetch_internal(cache, Z_STR_P(key), entry, now, &return_value);
	}, apc_cache_entry_try_begin());
}/*}}}*/
#undef apc_cache_entry_try_begin
#undef apc_cache_entry_try_end

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: noexpandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4 sts=4
 */
