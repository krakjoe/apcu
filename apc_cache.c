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

#include "apc_cache.h"
#include "apc_sma.h"
#include "apc_globals.h"
#include "apc_strings.h"
#include "php_scandir.h"
#include "SAPI.h"
#include "TSRM.h"
#include "php_main.h"
#include "ext/standard/md5.h"
#include "ext/standard/php_var.h"
#include "zend_smart_str.h"

#if PHP_VERSION_ID < 70300
# define GC_SET_REFCOUNT(ref, rc) (GC_REFCOUNT(ref) = (rc))
# define GC_ADDREF(ref) GC_REFCOUNT(ref)++
#endif

/* If recursive mutexes are used, there is no distinction between read and write locks.
 * As such, if we acquire a read-lock, it's really a write-lock and we are free to perform
 * increments without atomics. */
#ifdef APC_LOCK_RECURSIVE
# define ATOMIC_INC_RLOCKED(a) (a)++
#else
# define ATOMIC_INC_RLOCKED(a) ATOMIC_INC(a)
#endif

/* Defined in apc_persist.c */
apc_cache_entry_t *apc_persist(
		apc_sma_t *sma, apc_serializer_t *serializer, const apc_cache_entry_t *orig_entry);
zend_bool apc_unpersist(zval *dst, const zval *value, apc_serializer_t *serializer);

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

static inline void free_entry(apc_cache_t *cache, apc_cache_entry_t *entry) {
	apc_sma_free(cache->sma, entry);
}

/* {{{ apc_cache_hash_slot
 Note: These calculations can and should be done outside of a lock */
static inline void apc_cache_hash_slot(
		apc_cache_t* cache, zend_string *key, zend_ulong* hash, zend_ulong* slot) {
	*hash = ZSTR_HASH(key);
	*slot = *hash % cache->nslots;
} /* }}} */

static inline zend_bool apc_entry_key_equals(const apc_cache_entry_t *entry, zend_string *key, zend_ulong hash) {
	return ZSTR_H(entry->key) == hash
		&& ZSTR_LEN(entry->key) == ZSTR_LEN(key)
		&& memcmp(ZSTR_VAL(entry->key), ZSTR_VAL(key), ZSTR_LEN(key)) == 0;
}

/* An entry is hard expired if the creation time if older than the per-entry TTL.
 * Hard expired entries must be treated indentially to non-existent entries. */
static zend_bool apc_cache_entry_hard_expired(apc_cache_entry_t *entry, time_t t) {
	return entry->ttl && (time_t) (entry->ctime + entry->ttl) < t;
}

/* An entry is soft expired if no per-entry TTL is set, a global cache TTL is set,
 * and the access time of the entry is older than the global TTL. Soft expired entries
 * are accessible by lookup operation, but may be removed from the cache at any time. */
static zend_bool apc_cache_entry_soft_expired(
		apc_cache_t *cache, apc_cache_entry_t *entry, time_t t) {
	return !entry->ttl && cache->ttl && (time_t) (entry->atime + cache->ttl) < t;
}

static zend_bool apc_cache_entry_expired(
		apc_cache_t *cache, apc_cache_entry_t *entry, time_t t) {
	return apc_cache_entry_hard_expired(entry, t)
		|| apc_cache_entry_soft_expired(cache, entry, t);
}

/* {{{ apc_cache_wlocked_remove_entry  */
static void apc_cache_wlocked_remove_entry(apc_cache_t *cache, apc_cache_entry_t **entry)
{
	apc_cache_entry_t *dead = *entry;

	/* think here is safer */
	*entry = (*entry)->next;

	/* adjust header info */
	if (cache->header->mem_size)
		cache->header->mem_size -= dead->mem_size;

	if (cache->header->nentries)
		cache->header->nentries--;

	/* remove if there are no references */
	if (dead->ref_count <= 0) {
		free_entry(cache, dead);
	} else {
		/* add to gc if there are still refs */
		dead->next = cache->header->gc;
		dead->dtime = time(0);
		cache->header->gc = dead;
	}
}
/* }}} */

/* {{{ apc_cache_wlocked_gc */
static void apc_cache_wlocked_gc(apc_cache_t* cache)
{
	/* This function scans the list of removed cache entries and deletes any
	 * entry whose reference count is zero  or that has been on the gc
	 * list for more than cache->gc_ttl seconds
	 *   (we issue a warning in the latter case).
	 */
	if (!cache->header->gc) {
		return;
	}

	{
		apc_cache_entry_t **entry = &cache->header->gc;
		time_t now = time(0);

		while (*entry != NULL) {
			time_t gc_sec = cache->gc_ttl ? (now - (*entry)->dtime) : 0;

			if (!(*entry)->ref_count || gc_sec > (time_t)cache->gc_ttl) {
				apc_cache_entry_t *dead = *entry;

				/* good ol' whining */
				if (dead->ref_count > 0) {
					apc_debug(
						"GC cache entry '%s' was on gc-list for %ld seconds",
						ZSTR_VAL(dead->key), gc_sec
					);
				}

				/* set next entry */
				*entry = (*entry)->next;

				/* free entry */
				free_entry(cache, dead);
			} else {
				entry = &(*entry)->next;
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

	/* Lock in case apcu is accessed inside Serializer::serialize() */
	BG(serialize_lock)++;
	PHP_VAR_SERIALIZE_INIT(var_hash);
	php_var_serialize(&strbuf, (zval*) value, &var_hash);
	PHP_VAR_SERIALIZE_DESTROY(var_hash);
	BG(serialize_lock)--;

	if (EG(exception)) {
		smart_str_free(&strbuf);
		strbuf.s = NULL;
	}

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
	int result;

	/* Lock in case apcu is accessed inside Serializer::unserialize() */
	BG(serialize_lock)++;
	PHP_VAR_UNSERIALIZE_INIT(var_hash);
	result = php_var_unserialize(value, &tmp, buf + buf_len, &var_hash);
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
	BG(serialize_lock)--;
	
	if (!result) {
		php_error_docref(NULL, E_NOTICE, "Error at offset %ld of %ld bytes", (zend_long)(tmp - buf), (zend_long)buf_len);
		ZVAL_NULL(value);
		return 0;
	}
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
	cache = pemalloc(sizeof(apc_cache_t), 1);

	/* calculate cache size for shm allocation */
	cache_size = sizeof(apc_cache_header_t) + nslots*sizeof(apc_cache_entry_t *);

	/* allocate shm */
	cache->shmaddr = apc_sma_malloc(sma, cache_size);

	if (!cache->shmaddr) {
		zend_error_noreturn(E_CORE_ERROR, "Unable to allocate %zu bytes of shared memory for cache structures. Either apc.shm_size is too small or apc.entries_hint too large", cache_size);
		return NULL;
	}

	/* zero cache header and hash slots */
	memset(cache->shmaddr, 0, cache_size);

	/* set default header */
	cache->header = (apc_cache_header_t*) cache->shmaddr;

	cache->header->nhits = 0;
	cache->header->nmisses = 0;
	cache->header->nentries = 0;
	cache->header->nexpunges = 0;
	cache->header->gc = NULL;
	cache->header->stime = time(NULL);
	cache->header->state = 0;

	/* set cache options */
	cache->slots = (apc_cache_entry_t **) (((char*) cache->shmaddr) + sizeof(apc_cache_header_t));
	cache->sma = sma;
	cache->serializer = serializer;
	cache->nslots = nslots;
	cache->gc_ttl = gc_ttl;
	cache->ttl = ttl;
	cache->smart = smart;
	cache->defend = defend;

	/* header lock */
	CREATE_LOCK(&cache->header->lock);

	return cache;
} /* }}} */

static inline zend_bool apc_cache_wlocked_insert(
		apc_cache_t *cache, apc_cache_entry_t *new_entry, zend_bool exclusive) {
	zend_string *key = new_entry->key;
	time_t t = new_entry->ctime;

	/* process deleted list  */
	apc_cache_wlocked_gc(cache);

	/* make the insertion */
	{
		apc_cache_entry_t **entry;
		zend_ulong h, s;

		/* calculate hash and entry */
		apc_cache_hash_slot(cache, key, &h, &s);

		entry = &cache->slots[s];
		while (*entry) {
			/* check for a match by hash and string */
			if (apc_entry_key_equals(*entry, key, h)) {
				/*
				 * At this point we have found the user cache entry.  If we are doing
				 * an exclusive insert (apc_add) we are going to bail right away if
				 * the user entry already exists and is hard expired.
				 */
				if (exclusive && !apc_cache_entry_hard_expired(*entry, t)) {
					return 0;
				}

				apc_cache_wlocked_remove_entry(cache, entry);
				break;
			}

			/*
			 * This is a bit nasty. The idea here is to do runtime cleanup of the linked list of
			 * entry entries so we don't always have to skip past a bunch of stale entries.
			 */
			if (apc_cache_entry_expired(cache, *entry, t)) {
				apc_cache_wlocked_remove_entry(cache, entry);
				continue;
			}

			/* set next entry */
			entry = &(*entry)->next;
		}

		/* link in new entry */
		new_entry->next = *entry;
		*entry = new_entry;

		cache->header->mem_size += new_entry->mem_size;
		cache->header->nentries++;
		cache->header->ninserts++;
	}

	return 1;
}

static void apc_cache_init_entry(
		apc_cache_entry_t *entry, zend_string *key, const zval* val, const int32_t ttl, time_t t);

/* TODO This function may lead to a deadlock on expunge */
static inline zend_bool apc_cache_store_internal(
		apc_cache_t *cache, zend_string *key, const zval *val,
		const int32_t ttl, const zend_bool exclusive) {
	apc_cache_entry_t tmp_entry, *entry;
	time_t t = apc_time();

	if (apc_cache_defense(cache, key, t)) {
		return 0;
	}

	/* initialize the entry for insertion */
	apc_cache_init_entry(&tmp_entry, key, val, ttl, t);
	entry = apc_persist(cache->sma, cache->serializer, &tmp_entry);
	if (!entry) {
		return 0;
	}

	/* execute an insertion */
	if (!apc_cache_wlocked_insert(cache, entry, exclusive)) {
		free_entry(cache, entry);
		return 0;
	}

	return 1;
}

/* Find entry, without updating stat counters or access time */
static inline apc_cache_entry_t *apc_cache_rlocked_find_nostat(
		apc_cache_t *cache, zend_string *key, time_t t) {
	apc_cache_entry_t *entry;
	zend_ulong h, s;

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	entry = cache->slots[s];
	while (entry) {
		/* check for a matching key by has and identifier */
		if (apc_entry_key_equals(entry, key, h)) {
			/* Check to make sure this entry isn't expired by a hard TTL */
			if (apc_cache_entry_hard_expired(entry, t)) {
				break;
			}

			return entry;
		}

		entry = entry->next;
	}

	return NULL;
}

/* Find entry, updating stat counters and access time */
static inline apc_cache_entry_t *apc_cache_rlocked_find(
		apc_cache_t *cache, zend_string *key, time_t t) {
	apc_cache_entry_t *entry;
	zend_ulong h, s;

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	entry = cache->slots[s];
	while (entry) {
		/* check for a matching key by has and identifier */
		if (apc_entry_key_equals(entry, key, h)) {
			/* Check to make sure this entry isn't expired by a hard TTL */
			if (apc_cache_entry_hard_expired(entry, t)) {
				break;
			}

			ATOMIC_INC_RLOCKED(cache->header->nhits);
			ATOMIC_INC_RLOCKED(entry->nhits);
			entry->atime = t;

			return entry;
		}

		entry = entry->next;
	}

	ATOMIC_INC_RLOCKED(cache->header->nmisses);
	return NULL;
}

static inline apc_cache_entry_t *apc_cache_rlocked_find_incref(
		apc_cache_t *cache, zend_string *key, time_t t) {
	apc_cache_entry_t *entry = apc_cache_rlocked_find(cache, key, t);
	if (!entry) {
		return NULL;
	}

	ATOMIC_INC_RLOCKED(entry->ref_count);
	return entry;
}

/* {{{ apc_cache_store */
PHP_APCU_API zend_bool apc_cache_store(
		apc_cache_t* cache, zend_string *key, const zval *val,
		const int32_t ttl, const zend_bool exclusive) {
	apc_cache_entry_t tmp_entry, *entry;
	time_t t = apc_time();
	zend_bool ret = 0;

	if (!cache) {
		return 0;
	}

	/* run cache defense */
	if (apc_cache_defense(cache, key, t)) {
		return 0;
	}

	/* initialize the entry for insertion */
	apc_cache_init_entry(&tmp_entry, key, val, ttl, t);
	entry = apc_persist(cache->sma, cache->serializer, &tmp_entry);
	if (!entry) {
		return 0;
	}

	/* execute an insertion */
	if (!apc_cache_wlock(cache)) {
		free_entry(cache, entry);
		return 0;
	}

	php_apc_try {
		ret = apc_cache_wlocked_insert(cache, entry, exclusive);
	} php_apc_finally {
		apc_cache_wunlock(cache);
	} php_apc_end_try();

	if (!ret) {
		free_entry(cache, entry);
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
	size_t key_len;
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

/* {{{ apc_cache_entry_release */
PHP_APCU_API void apc_cache_entry_release(apc_cache_t *cache, apc_cache_entry_t *entry)
{
	ATOMIC_DEC(entry->ref_count);
}
/* }}} */

/* {{{ apc_cache_detach */
PHP_APCU_API void apc_cache_detach(apc_cache_t *cache)
{
	/* Important: This function should not clean up anything that's in shared memory,
	 * only detach our process-local use of it. In particular locks cannot be destroyed
	 * here. */

	if (!cache) {
		return;
	}

	free(cache);
}
/* }}} */

/* {{{ apc_cache_wlocked_real_expunge */
static void apc_cache_wlocked_real_expunge(apc_cache_t* cache) {
	/* increment counter */
	cache->header->nexpunges++;

	/* expunge */
	{
		zend_ulong i;

		for (i = 0; i < cache->nslots; i++) {
			apc_cache_entry_t **entry = &cache->slots[i];
			while (*entry) {
				apc_cache_wlocked_remove_entry(cache, entry);
			}
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
	memset(&cache->header->lastkey, 0, sizeof(apc_cache_slam_key_t));
} /* }}} */

/* {{{ apc_cache_clear */
PHP_APCU_API void apc_cache_clear(apc_cache_t* cache)
{
	if (!cache) {
		return;
	}

	if (!apc_cache_wlock(cache)) {
		return;
	}

	/* expunge cache */
	apc_cache_wlocked_real_expunge(cache);

	/* set info */
	cache->header->stime = apc_time();
	cache->header->nexpunges = 0;

	apc_cache_wunlock(cache);
}
/* }}} */

/* {{{ apc_cache_default_expunge */
PHP_APCU_API void apc_cache_default_expunge(apc_cache_t* cache, size_t size)
{
	time_t t;
	size_t suitable = 0L;
	size_t available = 0L;

	if (!cache) {
		return;
	}

	/* apc_time() depends on globals, don't read it if there's no cache. This may happen if SHM
	 * is too small and the initial cache creation during MINIT triggers an expunge. */
	t = apc_time();

	/* get the lock for header */
	if (!apc_cache_wlock(cache)) {
		return;
	}

	/* make suitable selection */
	suitable = (cache->smart > 0L) ? (size_t) (cache->smart * size) : (size_t) (cache->sma->size/2);

	/* gc */
	apc_cache_wlocked_gc(cache);

	/* get available */
	available = apc_sma_get_avail_mem(cache->sma);

	/* perform expunge processing */
	if (!cache->ttl) {
		/* check it is necessary to expunge */
		if (available < suitable) {
			apc_cache_wlocked_real_expunge(cache);
		}
	} else {
		/* check that expunge is necessary */
		if (available < suitable) {
			zend_ulong i;

			/* look for junk */
			for (i = 0; i < cache->nslots; i++) {
				apc_cache_entry_t **entry = &cache->slots[i];
				while (*entry) {
					if (apc_cache_entry_expired(cache, *entry, t)) {
						apc_cache_wlocked_remove_entry(cache, entry);
						continue;
					}

					/* grab next entry */
					entry = &(*entry)->next;
				}
			}

			/* if the cache now has space, then reset last key */
			if (apc_sma_get_avail_size(cache->sma, size)) {
				/* wipe lastkey */
				memset(&cache->header->lastkey, 0, sizeof(apc_cache_slam_key_t));
			} else {
				/* with not enough space left in cache, we are forced to expunge */
				apc_cache_wlocked_real_expunge(cache);
			}
		}
	}

	apc_cache_wunlock(cache);
}
/* }}} */

/* {{{ apc_cache_find */
PHP_APCU_API apc_cache_entry_t *apc_cache_find(apc_cache_t* cache, zend_string *key, time_t t)
{
	apc_cache_entry_t *entry;

	if (!cache) {
		return NULL;
	}

	if (!apc_cache_rlock(cache)) {
		return NULL;
	}

	entry = apc_cache_rlocked_find_incref(cache, key, t);
	apc_cache_runlock(cache);

	return entry;
}
/* }}} */

/* {{{ apc_cache_fetch */
PHP_APCU_API zend_bool apc_cache_fetch(apc_cache_t* cache, zend_string *key, time_t t, zval *dst)
{
	apc_cache_entry_t *entry;
	zend_bool retval = 0;

	if (!cache) {
		return 0;
	}

	if (!apc_cache_rlock(cache)) {
		return 0;
	}

	entry = apc_cache_rlocked_find_incref(cache, key, t);
	apc_cache_runlock(cache);

	if (!entry) {
		return 0;
	}

	php_apc_try {
		retval = apc_cache_entry_fetch_zval(cache, entry, dst);
	} php_apc_finally {
		apc_cache_entry_release(cache, entry);
	} php_apc_end_try();

	return retval;
} /* }}} */

/* {{{ apc_cache_exists */
PHP_APCU_API zend_bool apc_cache_exists(apc_cache_t* cache, zend_string *key, time_t t)
{
	apc_cache_entry_t *entry;

	if (!cache) {
		return 0;
	}

	if (!apc_cache_rlock(cache)) {
		return 0;
	}

	entry = apc_cache_rlocked_find_nostat(cache, key, t);
	apc_cache_runlock(cache);

	return entry != NULL;
}
/* }}} */

/* {{{ apc_cache_update */
PHP_APCU_API zend_bool apc_cache_update(
		apc_cache_t *cache, zend_string *key, apc_cache_updater_t updater, void *data,
		zend_bool insert_if_not_found, zend_long ttl)
{
	apc_cache_entry_t *entry;
	zend_bool retval = 0;
	time_t t = apc_time();

	if (!cache) {
		return 0;
	}

retry_update:
	if (!apc_cache_wlock(cache)) {
		return 0;
	}

	entry = apc_cache_rlocked_find_nostat(cache, key, t);
	if (entry) {
		/* Only allow changes to simple values */
		if (Z_TYPE(entry->val) < IS_STRING) {
			retval = updater(cache, entry, data);
			entry->mtime = t;
		}

		apc_cache_wunlock(cache);
		return retval;
	}

	apc_cache_wunlock(cache);
	if (insert_if_not_found) {
		/* Failed to find matching entry. Add key with value 0 and run the updater again. */
		zval val;
		ZVAL_LONG(&val, 0);

		/* We do not check the return value of the exclusive-store (add), as the entry might have
		 * been added between the cache unlock and the store call. In this case we just want to
		 * update the entry created by a different process. */
		apc_cache_store(cache, key, &val, ttl, 1);

		/* Only attempt to perform insertion once. */
		insert_if_not_found = 0;
		goto retry_update;
	}

	return 0;
}
/* }}} */

/* {{{ apc_cache_atomic_update_long */
PHP_APCU_API zend_bool apc_cache_atomic_update_long(
		apc_cache_t *cache, zend_string *key, apc_cache_atomic_updater_t updater, void *data,
		zend_bool insert_if_not_found, zend_long ttl)
{
	apc_cache_entry_t *entry;
	zend_bool retval = 0;
	time_t t = apc_time();

	if (!cache) {
		return 0;
	}

retry_update:
	if (!apc_cache_rlock(cache)) {
		return 0;
	}

	entry = apc_cache_rlocked_find_nostat(cache, key, t);
	if (entry) {
		/* Only supports integers */
		if (Z_TYPE(entry->val) == IS_LONG) {
			retval = updater(cache, &Z_LVAL(entry->val), data);
			entry->mtime = t;
		}

		apc_cache_runlock(cache);
		return retval;
	}

	apc_cache_runlock(cache);
	if (insert_if_not_found) {
		/* Failed to find matching entry. Add key with value 0 and run the updater again. */
		zval val;
		ZVAL_LONG(&val, 0);

		/* We do not check the return value of the exclusive-store (add), as the entry might have
		 * been added between the cache unlock and the store call. In this case we just want to
		 * update the entry created by a different process. */
		apc_cache_store(cache, key, &val, ttl, 1);

		/* Only attempt to perform insertion once. */
		insert_if_not_found = 0;
		goto retry_update;
	}

	return 0;
}
/* }}} */

/* {{{ apc_cache_delete */
PHP_APCU_API zend_bool apc_cache_delete(apc_cache_t *cache, zend_string *key)
{
	apc_cache_entry_t **entry;
	zend_ulong h, s;

	if (!cache) {
		return 0;
	}

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	if (!apc_cache_wlock(cache)) {
		return 0;
	}

	/* find head */
	entry = &cache->slots[s];

	while (*entry) {
		/* check for a match by hash and identifier */
		if (apc_entry_key_equals(*entry, key, h)) {
			/* executing removal */
			apc_cache_wlocked_remove_entry(cache, entry);

			apc_cache_wunlock(cache);
			return 1;
		}

		entry = &(*entry)->next;
	}

	apc_cache_wunlock(cache);
	return 0;
}
/* }}} */

/* {{{ apc_cache_entry_fetch_zval */
PHP_APCU_API zend_bool apc_cache_entry_fetch_zval(
		apc_cache_t *cache, apc_cache_entry_t *entry, zval *dst)
{
	return apc_unpersist(dst, &entry->val, cache->serializer);
}
/* }}} */

/* {{{ apc_cache_make_entry */
static void apc_cache_init_entry(
		apc_cache_entry_t *entry, zend_string *key, const zval *val, const int32_t ttl, time_t t)
{
	entry->ttl = ttl;
	entry->key = key;
	ZVAL_COPY_VALUE(&entry->val, val);

	entry->next = NULL;
	entry->ref_count = 0;
	entry->mem_size = 0;
	entry->nhits = 0;
	entry->ctime = t;
	entry->mtime = t;
	entry->atime = t;
	entry->dtime = 0;
}
/* }}} */

static inline void array_add_long(zval *array, zend_string *key, zend_long lval) {
	zval zv;
	ZVAL_LONG(&zv, lval);
	zend_hash_add_new(Z_ARRVAL_P(array), key, &zv);
}

static inline void array_add_double(zval *array, zend_string *key, double dval) {
	zval zv;
	ZVAL_DOUBLE(&zv, dval);
	zend_hash_add_new(Z_ARRVAL_P(array), key, &zv);
}

/* {{{ apc_cache_link_info */
static zval apc_cache_link_info(apc_cache_t *cache, apc_cache_entry_t *p)
{
	zval link, zv;
	array_init(&link);

	ZVAL_STR(&zv, zend_string_dup(p->key, 0));
	zend_hash_add_new(Z_ARRVAL(link), apc_str_info, &zv);

	array_add_long(&link, apc_str_ttl, p->ttl);
	array_add_double(&link, apc_str_num_hits, (double) p->nhits);
	array_add_long(&link, apc_str_mtime, p->mtime);
	array_add_long(&link, apc_str_creation_time, p->ctime);
	array_add_long(&link, apc_str_deletion_time, p->dtime);
	array_add_long(&link, apc_str_access_time, p->atime);
	array_add_long(&link, apc_str_ref_count, p->ref_count);
	array_add_long(&link, apc_str_mem_size, p->mem_size);

	return link;
}
/* }}} */

/* {{{ apc_cache_info */
PHP_APCU_API zend_bool apc_cache_info(zval *info, apc_cache_t *cache, zend_bool limited)
{
	zval list;
	zval gc;
	zval slots;
	apc_cache_entry_t *p;
	zend_ulong i, j;

	ZVAL_NULL(info);
	if (!cache) {
		return 0;
	}

	if (!apc_cache_rlock(cache)) {
		return 0;
	}

	php_apc_try {
		array_init(info);
		add_assoc_long(info, "num_slots", cache->nslots);
		array_add_long(info, apc_str_ttl, cache->ttl);
		array_add_double(info, apc_str_num_hits, (double) cache->header->nhits);
		add_assoc_double(info, "num_misses", (double) cache->header->nmisses);
		add_assoc_double(info, "num_inserts", (double) cache->header->ninserts);
		add_assoc_long(info,   "num_entries", cache->header->nentries);
		add_assoc_double(info, "expunges", (double) cache->header->nexpunges);
		add_assoc_long(info, "start_time", cache->header->stime);
		array_add_double(info, apc_str_mem_size, (double) cache->header->mem_size);

#if APC_MMAP
		add_assoc_stringl(info, "memory_type", "mmap", sizeof("mmap")-1);
#else
		add_assoc_stringl(info, "memory_type", "IPC shared", sizeof("IPC shared")-1);
#endif

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
				if (j != 0) {
					add_index_long(&slots, (zend_ulong)i, j);
				}
			}

			/* For each slot pending deletion */
			array_init(&gc);

			for (p = cache->header->gc; p != NULL; p = p->next) {
				zval link = apc_cache_link_info(cache, p);
				add_next_index_zval(&gc, &link);
			}

			add_assoc_zval(info, "cache_list", &list);
			add_assoc_zval(info, "deleted_list", &gc);
			add_assoc_zval(info, "slot_distribution", &slots);
		}
	} php_apc_finally {
		apc_cache_runlock(cache);
	} php_apc_end_try();

	return 1;
}
/* }}} */

/*
 fetches information about the key provided
*/
PHP_APCU_API void apc_cache_stat(apc_cache_t *cache, zend_string *key, zval *stat) {
	zend_ulong h, s;

	ZVAL_NULL(stat);
	if (!cache) {
		return;
	}

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	if (!apc_cache_rlock(cache)) {
		return;
	}

	php_apc_try {
		/* find head */
		apc_cache_entry_t *entry = cache->slots[s];

		while (entry) {
			/* check for a matching key by has and identifier */
			if (apc_entry_key_equals(entry, key, h)) {
				array_init(stat);
				array_add_long(stat, apc_str_hits, entry->nhits);
				array_add_long(stat, apc_str_access_time, entry->atime);
				array_add_long(stat, apc_str_mtime, entry->mtime);
				array_add_long(stat, apc_str_creation_time, entry->ctime);
				array_add_long(stat, apc_str_deletion_time, entry->dtime);
				array_add_long(stat, apc_str_ttl, entry->ttl);
				array_add_long(stat, apc_str_refs, entry->ref_count);
				break;
			}

			/* next */
			entry = entry->next;
		}
	} php_apc_finally {
		apc_cache_runlock(cache);
	} php_apc_end_try();
}

/* {{{ apc_cache_defense */
PHP_APCU_API zend_bool apc_cache_defense(apc_cache_t *cache, zend_string *key, time_t t)
{
	/* only continue if slam defense is enabled */
	if (cache->defend) {

		/* for copy of locking key struct */
		apc_cache_slam_key_t *last = &cache->header->lastkey;
		pid_t owner_pid = getpid();
#ifdef ZTS
		void ***owner_thread = TSRMLS_CACHE;
#endif

		/* check the hash and length match */
		/* check the time (last second considered slam) and context */
		if (last->hash == ZSTR_HASH(key) &&
			last->len == ZSTR_LEN(key) &&
			last->mtime == t &&
			(last->owner_pid != owner_pid
#if ZTS
			 || last->owner_thread != owner_thread
#endif
			)
		) {
			/* potential cache slam */
			return 1;
		}

		/* sets enough information for an educated guess, but is not exact */
		last->hash = ZSTR_HASH(key);
		last->len = ZSTR_LEN(key);
		last->mtime = t;
		last->owner_pid = owner_pid;
#ifdef ZTS
		last->owner_thread = owner_thread;
#endif
	}

	return 0;
}
/* }}} */

/* {{{ apc_cache_serializer */
PHP_APCU_API void apc_cache_serializer(apc_cache_t* cache, const char* name) {
	if (cache && !cache->serializer) {
		cache->serializer = apc_find_serializer(name);
	}
} /* }}} */

PHP_APCU_API void apc_cache_entry(apc_cache_t *cache, zend_string *key, zend_fcall_info *fci, zend_fcall_info_cache *fcc, zend_long ttl, zend_long now, zval *return_value) {/*{{{*/
	apc_cache_entry_t *entry = NULL;

	if (!cache) {
		return;
	}

	if (!apc_cache_wlock(cache)) {
		return;
	}

	APCG(entry_level)++;
	php_apc_try {
		entry = apc_cache_rlocked_find_incref(cache, key, now);
		if (!entry) {
			int result;
			zval params[1];
			ZVAL_STR_COPY(&params[0], key);

			fci->retval = return_value;
			fci->param_count = 1;
			fci->params = params;

			result = zend_call_function(fci, fcc);

			zval_ptr_dtor(&params[0]);

			if (result == SUCCESS && !EG(exception)) {
				apc_cache_store_internal(
					cache, key, return_value, (uint32_t) ttl, 1);
			}
		} else {
			apc_cache_entry_fetch_zval(cache, entry, return_value);
			apc_cache_entry_release(cache, entry);
		}
	} php_apc_finally {
		APCG(entry_level)--;
		apc_cache_wunlock(cache);
	} php_apc_end_try();
}
/*}}}*/

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: noexpandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4 sts=4
 */
