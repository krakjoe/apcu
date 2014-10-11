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
#include "ext/standard/php_smart_str.h"

typedef void* (*ht_copy_fun_t)(void*, void*, apc_context_t* TSRMLS_DC);
typedef int (*ht_check_copy_fun_t)(Bucket*, va_list);

#define CHECK(p) { if ((p) == NULL) return NULL; }

static APC_HOTSPOT zval* my_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC);
static HashTable* my_copy_hashtable_ex(HashTable*, HashTable* TSRMLS_DC, ht_copy_fun_t, int, apc_context_t*, ht_check_copy_fun_t, ...);
#define my_copy_hashtable( dst, src, copy_fn, holds_ptr, ctxt) \
    my_copy_hashtable_ex(dst, src TSRMLS_CC, copy_fn, holds_ptr, ctxt, NULL)

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
apc_cache_slot_t* make_slot(apc_cache_t* cache, apc_cache_key_t *key, apc_cache_entry_t* value, apc_cache_slot_t* next, time_t t TSRMLS_DC)
{
	apc_cache_slot_t* p = NULL;
	
	/* allocate slot */
	if ((p = value->pool->palloc(value->pool, sizeof(apc_cache_slot_t) TSRMLS_CC))) {	
		
		/* copy identifier */
		char* strkey = (char*) apc_pmemcpy(
			key->str, key->len, 
			value->pool TSRMLS_CC
		);

		if (strkey) {
			/* set idenfieir */
			key->str = strkey;
			
			/* set slot data */
			p->key = key[0];
			p->value = value;
			
			/* set slot relation */
			p->next = next;
			
			/* set slot defaults */
			p->nhits = 0;
			p->ctime = t;
			p->atime = t;
			p->dtime = 0;
		}
	}

    return p;
}
/* }}} */

/* {{{ free_slot */
static void free_slot(apc_cache_slot_t* slot TSRMLS_DC)
{
	/* destroy slot pool */
    apc_pool_destroy(
		slot->value->pool TSRMLS_CC);
}
/* }}} */

/* {{{ apc_cache_hash_slot
 Note: These calculations can and should be done outside of a lock */
static void apc_cache_hash_slot(apc_cache_t* cache, 
                                char *str,
                                zend_uint len, 
                                zend_ulong* hash, 
                                zend_ulong* slot) {
	(*hash) = zend_inline_hash_func(str, len);
	(*slot) = (*hash) % (cache->nslots);
} /* }}} */

/* {{{ apc_cache_remove_slot  */
PHP_APCU_API void apc_cache_remove_slot(apc_cache_t* cache, apc_cache_slot_t** slot TSRMLS_DC)
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
        free_slot(dead TSRMLS_CC);
    } else {
		/* add to gc if there are still refs */
        dead->next = cache->header->gc;
        dead->dtime = time(0);
        cache->header->gc = dead;
    }
}
/* }}} */

/* {{{ apc_cache_gc */
PHP_APCU_API void apc_cache_gc(apc_cache_t* cache TSRMLS_DC)
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
						"GC cache entry '%s' was on gc-list for %d seconds" TSRMLS_CC, 
						dead->key.str, gc_sec
					);
			    }

				/* set next slot */
			    *slot = dead->next;
			
				/* free slot */
			    free_slot(
					dead TSRMLS_CC);
			
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
    php_var_serialize(&strbuf, (zval**)&value, &var_hash TSRMLS_CC);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    if(strbuf.c) {
        *buf = (unsigned char*)strbuf.c;
        *buf_len = strbuf.len;
        smart_str_0(&strbuf);
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
    if(!php_var_unserialize(value, &tmp, buf + buf_len, &var_hash TSRMLS_CC)) {
        PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
        zval_dtor(*value);
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %ld of %ld bytes", (long)(tmp - buf), (long)buf_len);
        (*value)->type = IS_NULL;
        return 0;
    }
    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
    return 1;
} /* }}} */

/* {{{ apc_cache_create */
PHP_APCU_API apc_cache_t* apc_cache_create(apc_sma_t* sma, apc_serializer_t* serializer, int size_hint, int gc_ttl, int ttl, long smart, zend_bool defend TSRMLS_DC) {
	apc_cache_t* cache;
    int cache_size;
    int nslots;

	/* calculate number of slots */
    nslots = make_prime(size_hint > 0 ? size_hint : 2000);

	/* allocate pointer by normal means */
    cache = (apc_cache_t*) apc_emalloc(sizeof(apc_cache_t) TSRMLS_CC);
	
	/* calculate cache size for shm allocation */
    cache_size = sizeof(apc_cache_header_t) + nslots*sizeof(apc_cache_slot_t*);

	/* allocate shm */
    cache->shmaddr = sma->smalloc(cache_size TSRMLS_CC);
    if(!cache->shmaddr) {
        apc_error("Unable to allocate shared memory for cache structures.  (Perhaps your shared memory size isn't large enough?). " TSRMLS_CC);
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

/* {{{ apc_cache_store */
PHP_APCU_API zend_bool apc_cache_store(apc_cache_t* cache, char *strkey, zend_uint keylen, const zval *val, const zend_uint ttl, const zend_bool exclusive TSRMLS_DC) {
    apc_cache_entry_t *entry;
    apc_cache_key_t key;
    time_t t;
    apc_context_t ctxt={0,};
    zend_bool ret = 0;

    t = apc_time();

	/* initialize a context suitable for making an insert */
    if (apc_cache_make_context(cache, &ctxt, APC_CONTEXT_SHARE, APC_SMALL_POOL, APC_COPY_IN, 0 TSRMLS_CC)) {

        /* initialize the key for insertion */
        if (apc_cache_make_key(&key, strkey, keylen TSRMLS_CC)) {

            /* run cache defense */
            if (!apc_cache_defense(cache, &key TSRMLS_CC)) {
                
                /* initialize the entry for insertion */
                if ((entry = apc_cache_make_entry(&ctxt, &key, val, ttl TSRMLS_CC))) {
                
                    /* execute an insertion */
                    if (apc_cache_insert(cache, key, entry, &ctxt, t, exclusive TSRMLS_CC)) {
                        ret = 1;
                    }
                }
            }
        }

        /* in any case of failure the context should be destroyed */
        if (!ret) {
            apc_cache_destroy_context(&ctxt TSRMLS_CC);
        }
    }

    return ret;
} /* }}} */

/* {{{ data_unserialize */
static zval* data_unserialize(const char *filename TSRMLS_DC)
{
    zval* retval;
    long len = 0;
    struct stat sb;
    char *contents, *tmp;
    FILE *fp;
    php_unserialize_data_t var_hash = {0,};

    if(VCWD_STAT(filename, &sb) == -1) {
        return NULL;
    }

    fp = fopen(filename, "rb");

    len = sizeof(char)*sb.st_size;

    tmp = contents = malloc(len);

    if(!contents) {
        fclose(fp);
        return NULL;
    }

    if(fread(contents, 1, len, fp) < 1) {	
        fclose(fp);
        free(contents);
        return NULL;
    }

    MAKE_STD_ZVAL(retval);

    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    
    /* I wish I could use json */
    if(!php_var_unserialize(&retval, (const unsigned char**)&tmp, (const unsigned char*)(contents+len), &var_hash TSRMLS_CC)) {
        fclose(fp);
        zval_ptr_dtor(&retval);
        return NULL;
    }

    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

    free(contents);
    fclose(fp);

    return retval;
}

static int apc_load_data(apc_cache_t* cache, const char *data_file TSRMLS_DC)
{
    char *p;
    char key[MAXPATHLEN] = {0,};
    unsigned int key_len;
    zval *data;

    p = strrchr(data_file, DEFAULT_SLASH);

    if(p && p[1]) {
        strlcpy(key, p+1, sizeof(key));
        p = strrchr(key, '.');

        if(p) {
            p[0] = '\0';
            key_len = strlen(key)+1;

            data = data_unserialize(data_file TSRMLS_CC);
            if(data) {
                apc_cache_store(cache, key, key_len, data, 0, 1 TSRMLS_CC);
            }
            return 1;
        }
    }

    return 0;
}

/* {{{ apc_cache_preload shall load the prepared data files in path into the specified cache */
PHP_APCU_API zend_bool apc_cache_preload(apc_cache_t* cache, const char *path TSRMLS_DC)
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

			if(apc_load_data(cache, file TSRMLS_CC)) {
				result = 1;
			}
			free(namelist[i]);
		}
		free(namelist);
	}
	return result;
#else 
	apc_error("Cannot load data from apc.preload_path=%s in thread-safe mode" TSRMLS_CC, path);
	return 0;
#endif	
} /* }}} */

/* {{{ apc_cache_release */
PHP_APCU_API void apc_cache_release(apc_cache_t* cache, apc_cache_entry_t* entry TSRMLS_DC)
{
    entry->ref_count--;
}
/* }}} */

/* {{{ apc_cache_destroy */
PHP_APCU_API void apc_cache_destroy(apc_cache_t* cache TSRMLS_DC)
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
    apc_efree(cache TSRMLS_CC);
}
/* }}} */

/* {{{ apc_cache_real_expunge */
PHP_APCU_API void apc_cache_real_expunge(apc_cache_t* cache TSRMLS_DC) {
	/* increment counter */	
	cache->header->nexpunges++;
    
	/* expunge */
    {
		zend_ulong i;

		for (i = 0; i < cache->nslots; i++) {
		    apc_cache_slot_t* p = cache->slots[i];
		    while (p) {
		        apc_cache_remove_slot(cache, &p TSRMLS_CC);
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
PHP_APCU_API void apc_cache_clear(apc_cache_t* cache TSRMLS_DC)
{
	/* check there is a cache and it is not busy */
    if(!cache || apc_cache_busy(cache TSRMLS_CC)) {
		return;
	}
	
	/* lock header */
	APC_LOCK(cache->header);
	
	/* set busy */
	cache->header->state |= APC_CACHE_ST_BUSY;
	
	/* expunge cache */
	apc_cache_real_expunge(cache TSRMLS_CC);

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
PHP_APCU_API void apc_cache_default_expunge(apc_cache_t* cache, size_t size TSRMLS_DC)
{
    time_t t;
	size_t suitable = 0L;
    size_t available = 0L;

    t = apc_time();

	/* check there is a cache, and it is not busy */
    if(!cache || apc_cache_busy(cache TSRMLS_CC)) {
		return;
	}
	
	/* get the lock for header */
	APC_LOCK(cache->header);

	/* update state in header */
	cache->header->state |= APC_CACHE_ST_BUSY;

	/* make suitable selection */
	suitable = (cache->smart > 0L) ? (size_t) (cache->smart * size) : (size_t) (cache->sma->size/2);

	/* gc */
    apc_cache_gc(cache TSRMLS_CC);

    /* get available */
	available = cache->sma->get_avail_mem();

	/* perform expunge processing */
    if(!cache->ttl) {

		/* check it is necessary to expunge */
		if (available < suitable) {
			apc_cache_real_expunge(cache TSRMLS_CC);
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
		                    apc_cache_remove_slot(cache, slot TSRMLS_CC);
		                    continue;
		                }
		            } else if(cache->ttl) {
		                if((time_t) ((*slot)->ctime + cache->ttl) < t) {
		                    apc_cache_remove_slot(cache, slot TSRMLS_CC);
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
				apc_cache_real_expunge(cache TSRMLS_CC);
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
                                              uint force_update TSRMLS_DC) {
	switch (context_type) {
		case APC_CONTEXT_SHARE: {
			return apc_cache_make_context_ex(
				context,
                cache->serializer,
				(apc_malloc_t) cache->sma->smalloc,
                cache->sma->sfree,
                cache->sma->protect,
                cache->sma->unprotect,
				pool_type, copy_type, force_update TSRMLS_CC
			);
		} break;
		
		case APC_CONTEXT_NOSHARE: {
			return apc_cache_make_context_ex(
				context,
                cache->serializer,
				apc_php_malloc, apc_php_free, NULL, NULL,
				pool_type, copy_type, force_update TSRMLS_CC
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
                                                 uint force_update TSRMLS_DC) {
	/* attempt to create the pool */
	context->pool = apc_pool_create(
		pool_type, _malloc, _free, _protect, _unprotect TSRMLS_CC
	);

	if (!context->pool) {
		apc_warning("Unable to allocate memory for pool." TSRMLS_CC);
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
PHP_APCU_API zend_bool apc_cache_destroy_context(apc_context_t* context TSRMLS_DC) {
    if (!context->pool) {
        return 0;
    }

    apc_pool_destroy(context->pool TSRMLS_CC);

    return 1;
} /* }}} */

/* {{{ apc_cache_insert */
PHP_APCU_API zend_bool apc_cache_insert(apc_cache_t* cache, 
                                        apc_cache_key_t key, 
                                        apc_cache_entry_t* value, 
                                        apc_context_t* ctxt, 
                                        time_t t, 
                                        zend_bool exclusive TSRMLS_DC)
{
    zend_bool result = 0;

	/* at least */
	if (!value) {
		return result;
	}
	
	/* check we are able to deal with this request */
	if (!cache || apc_cache_busy(cache TSRMLS_CC)) {
		return result;
	}

	/* lock header */
	APC_LOCK(cache->header);
	
	/* process deleted list  */
    apc_cache_gc(cache TSRMLS_CC);

	/* make the insertion */	
	{
		apc_cache_slot_t** slot;

		/*
		* select appropriate slot ...
		*/
		slot = &cache->slots[key.h % cache->nslots];

		while (*slot) {
			
			/* check for a match by hash and string */
		    if (((*slot)->key.h == key.h) && (!memcmp((*slot)->key.str, key.str, key.len))) {

		        /* 
		         * At this point we have found the user cache entry.  If we are doing 
		         * an exclusive insert (apc_add) we are going to bail right away if
		         * the user entry already exists and it has no ttl, or
		         * there is a ttl and the entry has not timed out yet.
		         */
		        if(exclusive) {
                    if (!(*slot)->value->ttl || (time_t) ((*slot)->ctime + (*slot)->value->ttl) >= t) {
                        goto nothing;
                    }
		        }
                apc_cache_remove_slot(cache, slot TSRMLS_CC);
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
                apc_cache_remove_slot(cache, slot TSRMLS_CC);
		        continue;
		    }
		
			/* set next slot */
            slot = &(*slot)->next;      
		}

		if ((*slot = make_slot(cache, &key, value, *slot, t TSRMLS_CC)) != NULL) {
            /* set value size from pool size */
            value->mem_size = ctxt->pool->size;

            cache->header->mem_size += ctxt->pool->size;
            cache->header->nentries++;
            cache->header->ninserts++;
		} else {
			goto nothing;
		}
	}

    /* unlock and return succesfull */	
    APC_UNLOCK(cache->header);

    return 1;

    /* bail */
nothing:
    APC_UNLOCK(cache->header);

    return 0;
}
/* }}} */

/* {{{ apc_cache_find */
PHP_APCU_API apc_cache_entry_t* apc_cache_find(apc_cache_t* cache, char *strkey, zend_uint keylen, time_t t TSRMLS_DC)
{
	/* check we are able to deal with the request */
    if(!cache || apc_cache_busy(cache TSRMLS_CC)) {
        return NULL;
    }

	/* we only declare a volatile we need */
    {
        apc_cache_slot_t** slot;
		zend_ulong h, s;

        volatile apc_cache_entry_t* value = NULL;
        
		/* calculate hash and slot */
		apc_cache_hash_slot(cache, strkey, keylen, &h, &s);

        /* read lock header */
		APC_RLOCK(cache->header);

		/* find head */
		slot = &cache->slots[s];

		while (*slot) {
			/* check for a matching key by has and identifier */
		    if ((h == (*slot)->key.h) && !memcmp((*slot)->key.str, strkey, keylen)) {

		        /* Check to make sure this entry isn't expired by a hard TTL */
		        if((*slot)->value->ttl && (time_t) ((*slot)->ctime + (*slot)->value->ttl) < t) {
					/* increment misses on cache */
					cache->header->nmisses++;

					/* unlock header */			
					APC_RUNLOCK(cache->header);
		            
		            return NULL;
		        }

		        /* Otherwise we are fine, increase counters and return the cache entry */
		        (*slot)->nhits++;
		        (*slot)->value->ref_count++;
		        (*slot)->atime = t;
			
				/* set cache num hits */
				cache->header->nhits++;
		        
				/* grab value */
		        value = (*slot)->value;

				/* unlock header */			
				APC_RUNLOCK(cache->header);
			
		        return (apc_cache_entry_t*)value;
		    }

			/* next */
		    slot = &(*slot)->next;		
		}
	 	
		/* not found, so increment misses */
		cache->header->nmisses++;

		/* unlock header */
		APC_RUNLOCK(cache->header);
    }

    return NULL;
}
/* }}} */

/* {{{ apc_cache_fetch */
PHP_APCU_API zend_bool apc_cache_fetch(apc_cache_t* cache, char* strkey, zend_uint keylen, time_t t, zval **dst TSRMLS_DC) 
{
	apc_cache_entry_t *entry;
	zend_bool ret = 0;
	
	/* find the entry */
	if ((entry = apc_cache_find(cache, strkey, keylen, t TSRMLS_CC))) {
        /* context for copying out */
		apc_context_t ctxt = {0, };

		/* create unpool context */
		if (apc_cache_make_context(cache, &ctxt, APC_CONTEXT_NOSHARE, APC_UNPOOL, APC_COPY_OUT, 0 TSRMLS_CC)) {

			/* copy to destination */
			apc_cache_fetch_zval(&ctxt, *dst, entry->val TSRMLS_CC);

			/* release entry */
			apc_cache_release(
				cache, entry TSRMLS_CC);

			/* destroy context */
			apc_cache_destroy_context(&ctxt TSRMLS_CC );

			/* set result */
			ret = 1;
		}
	}
	
	return ret;
} /* }}} */

/* {{{ apc_cache_exists */
PHP_APCU_API apc_cache_entry_t* apc_cache_exists(apc_cache_t* cache, char *strkey, zend_uint keylen, time_t t TSRMLS_DC)
{
    if(apc_cache_busy(cache TSRMLS_CC))
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
		apc_cache_hash_slot(cache, strkey, keylen, &h, &s);

        /* read lock header */
		APC_RLOCK(cache->header);	

		/* find head */
		slot = &cache->slots[s];

		while (*slot) {
			/* check for match by hash and identifier */
		    if ((h == (*slot)->key.h) &&
		        !memcmp((*slot)->key.str, strkey, keylen)) {

		        /* Check to make sure this entry isn't expired by a hard TTL */
		        if((*slot)->value->ttl && (time_t) ((*slot)->ctime + (*slot)->value->ttl) < t) {
                    /* marked as a miss */
                    cache->header->nmisses++;

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
PHP_APCU_API zend_bool apc_cache_update(apc_cache_t* cache, char *strkey, zend_uint keylen, apc_cache_updater_t updater, void* data TSRMLS_DC)
{
    apc_cache_slot_t** slot;
	
    zend_bool retval = 0;
    zend_ulong h, s;

    if(apc_cache_busy(cache TSRMLS_CC))
    {
        /* cannot service request right now */ 
        return 0;
    }

    /* calculate hash */
    apc_cache_hash_slot(cache, strkey, keylen, &h, &s);
	
	/* lock header */
	APC_LOCK(cache->header);

	/* find head */
    slot = &cache->slots[s];

    while (*slot) {
		/* check for a match by hash and identifier */
        if ((h == (*slot)->key.h) &&
            !memcmp((*slot)->key.str, strkey, keylen)) {
			/* attempt to perform update */
            switch(Z_TYPE_P((*slot)->value->val) & ~IS_CONSTANT_TYPE_MASK) {
                case IS_ARRAY:
                case IS_OBJECT:
                {
                    if(cache->serializer) {
                        retval = 0;
                        break;
                    } else {
                        /* fall through */
                    }
                }
                /* fall through */
                default:
                {
					/* executing update */
                    retval = updater(
						cache, (*slot)->value, data);

					/* set modified time */
                    (*slot)->key.mtime = apc_time();
                }
                break;
            }
			/* unlock header */
			APC_UNLOCK(cache->header);

            return retval;
        }

		/* set next slot */
        slot = &(*slot)->next;
	}
	
	/* unlock header */
	APC_UNLOCK(cache->header);

    return 0;
}
/* }}} */

/* {{{ apc_cache_delete */
PHP_APCU_API zend_bool apc_cache_delete(apc_cache_t* cache, char *strkey, zend_uint keylen TSRMLS_DC)
{
    apc_cache_slot_t** slot;
	
    zend_ulong h, s;

	if (!cache) {
		return 1;
	}

    /* calculate hash and slot */
    apc_cache_hash_slot(cache, strkey, keylen, &h, &s);

	/* lock cache */
	APC_LOCK(cache->header);	
	
	/* find head */
    slot = &cache->slots[s];

    while (*slot) {
		/* check for a match by hash and identifier */
        if ((h == (*slot)->key.h) && 
            !memcmp((*slot)->key.str, strkey, keylen)) {
			/* executing removal */
            apc_cache_remove_slot(
				cache, slot TSRMLS_CC);
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
PHP_APCU_API zend_bool apc_cache_make_key(apc_cache_key_t* key, char* str, zend_ulong len TSRMLS_DC)
{
    assert(key != NULL);

    if (!str) {
	    return 0;
    }
    
	if (!len)
	    len = strlen(str) + 1;
	
    key->str = str;
    key->len = len;
    key->h = zend_inline_hash_func((char *)key->str, key->len);
    key->mtime = apc_time();

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

    if(ctxt->serializer) {
        serialize = ctxt->serializer->serialize;
        config = (ctxt->serializer->config != NULL) ? ctxt->serializer->config : ctxt;
    }

    if(serialize((unsigned char**)&buf.c, &buf.len, src, config TSRMLS_CC)) {
        dst->type = src->type & ~IS_CONSTANT; 
        dst->value.str.len = buf.len;
        CHECK(dst->value.str.val = apc_pmemcpy(buf.c, (buf.len + 1), pool TSRMLS_CC));
    }

    if(buf.c) {
		smart_str_free(&buf);
	}

    return dst;
}
/* }}} */

/* {{{ my_unserialize_object */
static zval* my_unserialize_object(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    apc_unserialize_t unserialize = APC_UNSERIALIZER_NAME(php);
    unsigned char *p = (unsigned char*)Z_STRVAL_P(src);
    void *config = NULL;

    if(ctxt->serializer) {
        unserialize = ctxt->serializer->unserialize;
        config = (ctxt->serializer->config != NULL) ? ctxt->serializer->config : ctxt;
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
        CHECK(dst = (HashTable*) pool->palloc(pool, sizeof(src[0]) TSRMLS_CC));
    }

    memcpy(dst, src, sizeof(src[0]));

    /* allocate buckets for the new hashtable */
    CHECK((dst->arBuckets = pool->palloc(pool, (dst->nTableSize * sizeof(Bucket*)) TSRMLS_CC)));

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
        } else {
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
    int usegc = (ctxt->copy == APC_COPY_OUT);

    assert(src != NULL);

    if (!dst) {
        CHECK(dst = (zval**) pool->palloc(pool, sizeof(zval*) TSRMLS_CC));
    }

    if(usegc) {
        ALLOC_ZVAL(dst[0]);
        CHECK(dst[0]);
    } else {
        CHECK((dst[0] = (zval*) pool->palloc(pool, sizeof(zval) TSRMLS_CC)));
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

    if(ctxt->copied.nTableSize) {
        if(zend_hash_index_find(&ctxt->copied, (ulong)src, (void**)&tmp) == SUCCESS) {
            if(Z_ISREF_P((zval*)src)) {
                Z_SET_ISREF_PP(tmp);
            }
            Z_ADDREF_PP(tmp);
            return *tmp;
        }

        zend_hash_index_update(&ctxt->copied, (ulong)src, (void**)&dst, sizeof(zval*), NULL);
    }

    if(ctxt->copy == APC_COPY_OUT || ctxt->copy == APC_COPY_IN) {
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
        if(ctxt->serializer == NULL) {

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
        if(ctxt->copy == APC_COPY_IN) {
            dst = my_serialize_object(dst, src, ctxt TSRMLS_CC);
        } else if(ctxt->copy == APC_COPY_OUT) {
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
PHP_APCU_API zval* apc_copy_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    apc_pool* pool = ctxt->pool;
    
    assert(src != NULL);

    if (!dst) {
        if(ctxt->copy == APC_COPY_OUT) {
            ALLOC_ZVAL(dst);
            CHECK(dst);
        } else {
            CHECK(dst = (zval*) pool->palloc(pool, sizeof(zval) TSRMLS_CC));
        }
    }

    CHECK(dst = my_copy_zval(dst, src, ctxt TSRMLS_CC));
    return dst;
}
/* }}} */

/* {{{ apc_cache_store_zval */
PHP_APCU_API zval* apc_cache_store_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC)
{
    if (Z_TYPE_P(src) == IS_ARRAY) {
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        zend_hash_init(&ctxt->copied, 0, NULL, NULL, 0);
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
        zend_hash_destroy(&ctxt->copied);
        ctxt->copied.nTableSize=0;
    } else {
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
    }


    return dst;
}
/* }}} */

/* {{{ apc_cache_fetch_zval */
PHP_APCU_API zval* apc_cache_fetch_zval(apc_context_t* ctxt, zval* dst, const zval* src TSRMLS_DC)
{
    if (Z_TYPE_P(src) == IS_ARRAY) {
        /* Maintain a list of zvals we've copied to properly handle recursive structures */
        zend_hash_init(&ctxt->copied, 0, NULL, NULL, 0);
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
        zend_hash_destroy(&ctxt->copied);
        ctxt->copied.nTableSize=0;
    } else {
        dst = apc_copy_zval(dst, src, ctxt TSRMLS_CC);
    }


    return dst;
}
/* }}} */

/* {{{ apc_cache_make_entry */
PHP_APCU_API apc_cache_entry_t* apc_cache_make_entry(apc_context_t* ctxt, apc_cache_key_t *key, const zval* val, const unsigned int ttl TSRMLS_DC)
{
    apc_cache_entry_t* entry;
    apc_pool* pool = ctxt->pool;

    entry = (apc_cache_entry_t*) pool->palloc(pool, sizeof(apc_cache_entry_t) TSRMLS_CC);
    if (!entry) {
		return NULL;
	}
	
	/* set key for serializer */
	ctxt->key = key;
	
    entry->val = apc_cache_store_zval(NULL, val, ctxt TSRMLS_CC);
    if(!entry->val) {
        return NULL;
    }

    INIT_PZVAL(entry->val);
    entry->ttl = ttl;
    entry->ref_count = 0;
    entry->mem_size = 0;
    entry->pool = pool;
    return entry;
}
/* }}} */

/* {{{ apc_cache_link_info */
static zval* apc_cache_link_info(apc_cache_t *cache, apc_cache_slot_t* p TSRMLS_DC)
{
    zval *link = NULL;

    ALLOC_INIT_ZVAL(link);

    if(!link) {
        return NULL;
    }

    array_init(link);

    add_assoc_stringl(link, "info", (char*) p->key.str, p->key.len-1, 1);
    add_assoc_long(link, "ttl", (long)p->value->ttl);

    add_assoc_double(link, "num_hits", (double)p->nhits);
    add_assoc_long(link, "modification_time", p->key.mtime);
    add_assoc_long(link, "creation_time", p->ctime);
    add_assoc_long(link, "deletion_time", p->dtime);
    add_assoc_long(link, "access_time", p->atime);
    add_assoc_long(link, "ref_count", p->value->ref_count);
    add_assoc_long(link, "mem_size", p->value->mem_size);

    return link;
}
/* }}} */

/* {{{ apc_cache_info */
PHP_APCU_API zval* apc_cache_info(apc_cache_t* cache, zend_bool limited TSRMLS_DC)
{
    zval *info = NULL;
    zval *list = NULL;
    zval *gc = NULL;
    zval *slots = NULL;
    apc_cache_slot_t* p;
    zend_ulong i, j;

    if (!cache) {
        return NULL;
    }

    ALLOC_INIT_ZVAL(info);

    /* read lock header */
    APC_RLOCK(cache->header);

    array_init(info);
    add_assoc_long(info, "num_slots", cache->nslots);
    add_assoc_long(info, "ttl", cache->ttl);
    add_assoc_double(info, "num_hits", (double)cache->header->nhits);
    add_assoc_double(info, "num_misses", (double)cache->header->nmisses);
    add_assoc_double(info, "num_inserts", (double)cache->header->ninserts);
    add_assoc_long(info,   "num_entries", cache->header->nentries);
    add_assoc_double(info, "num_expunges", (double)cache->header->nexpunges);
    add_assoc_long(info, "start_time", cache->header->stime);
    add_assoc_double(info, "mem_size", (double)cache->header->mem_size);

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

    if (!limited) {
        /* For each hashtable slot */
        ALLOC_INIT_ZVAL(list);
        array_init(list);

        ALLOC_INIT_ZVAL(slots);
        array_init(slots);

        for (i = 0; i < cache->nslots; i++) {
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
        ALLOC_INIT_ZVAL(gc);
        array_init(gc);

        for (p = cache->header->gc; p != NULL; p = p->next) {
            zval *link = apc_cache_link_info(cache, p TSRMLS_CC);
            add_next_index_zval(gc, link);
        }
        
        add_assoc_zval(info, "cache_list", list);
        add_assoc_zval(info, "deleted_list", gc);
        add_assoc_zval(info, "slot_distribution", slots);
    }
	
	/* unlock header */
	APC_RUNLOCK(cache->header);

    return info;
}
/* }}} */

/*
 fetches information about the key provided
*/
PHP_APCU_API zval* apc_cache_stat(apc_cache_t* cache,
                                  char *strkey,
                                  zend_uint keylen TSRMLS_DC) {
    zval *stat;
    apc_cache_slot_t** slot;
	zend_ulong h, s;
    
	/* calculate hash and slot */
	apc_cache_hash_slot(cache, strkey, keylen, &h, &s);
	
	/* allocate stat buffer */
	ALLOC_INIT_ZVAL(stat);

    /* read lock header */
	APC_RLOCK(cache->header);

	/* find head */
	slot = &cache->slots[s];

	while (*slot) {
		/* check for a matching key by has and identifier */
	    if ((h == (*slot)->key.h) && !memcmp((*slot)->key.str, strkey, keylen)) {
            array_init(stat);
            
            add_assoc_long(stat, "hits",  (*slot)->nhits);
            add_assoc_long(stat, "access_time", (*slot)->atime);
            add_assoc_long(stat, "modification_time", (*slot)->key.mtime);
            add_assoc_long(stat, "creation_time", (*slot)->ctime);
            add_assoc_long(stat, "deletion_time", (*slot)->dtime);
	        add_assoc_long(stat, "ttl",   (*slot)->value->ttl);
	        add_assoc_long(stat, "refs",  (*slot)->value->ref_count);
	        
	        break;
	    }

		/* next */
	    slot = &(*slot)->next;		
	}
    
    APC_RUNLOCK(cache->header);
    
    return stat;
}

/* {{{ apc_cache_busy */
PHP_APCU_API zend_bool apc_cache_busy(apc_cache_t* cache TSRMLS_DC)
{	
	return (cache->header->state & APC_CACHE_ST_BUSY);
}
/* }}} */

/* {{{ apc_cache_defense */
PHP_APCU_API zend_bool apc_cache_defense(apc_cache_t* cache, apc_cache_key_t* key TSRMLS_DC)
{
	zend_bool result = 0;

	/* in ZTS mode, we use the current TSRM context to determine the owner */
#ifdef ZTS
# define FROM_DIFFERENT_THREAD(k) ((key->owner = TSRMLS_C) != (k)->owner) 
#else
# define FROM_DIFFERENT_THREAD(k) ((key->owner = getpid()) != (k)->owner) 
#endif

	/* only continue if slam defense is enabled */
	if (cache->defend) {

		/* for copy of locking key struct */
		apc_cache_key_t *last = &cache->header->lastkey;

		/* check the hash and length match */
		if(last->h == key->h && last->len == key->len) {

			/* check the time ( last second considered slam ) and context */
			if(last->mtime == key->mtime && 
			   FROM_DIFFERENT_THREAD(last)) {

			    /* potential cache slam */
			    apc_debug(
					"Potential cache slam averted for key '%s'" TSRMLS_CC, key->str);
			    result = 1;
			} else {
				/* sets enough information for an educated guess, but is not exact */
				last->h = key->h;
				last->len = key->len;
				last->mtime = apc_time();

				/* required to tell contexts apart */
#ifdef ZTS
				last->owner = TSRMLS_C;
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
PHP_APCU_API void apc_cache_serializer(apc_cache_t* cache, const char* name TSRMLS_DC) {
	if (cache && !cache->serializer) {
		cache->serializer = apc_find_serializer(name TSRMLS_CC);
	}
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
