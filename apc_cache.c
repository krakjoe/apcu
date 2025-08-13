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
#include "apc_time.h"
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
		apc_sma_t *sma, apc_serializer_t *serializer, zend_string *key, const zval *val);
zend_bool apc_unpersist(zval *dst, const apc_cache_entry_t *entry, apc_serializer_t *serializer);

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
1310627,/* 1310720 */
1474489,/* 1474560 */
1965983,/* 1966080 */
2621347,/* 2621440 */
3276719,/* 3276800 */
3932063,/* 3932160 */
4587431,/* 4587520 */
5242801,/* 5242880 */
6553511,/* 6553600 */
7864243,/* 7864320 */
8847271,/* 8847360 */
9830321,/* 9830400 */
10485667,/* 10485760 */
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
		apc_cache_t* cache, zend_string *key, zend_ulong* hash, size_t* slot) {
	*hash = ZSTR_HASH(key);
	*slot = *hash % cache->nslots;
} /* }}} */

static inline zend_bool apc_entry_key_equals(const apc_cache_entry_t *entry, zend_string *key, zend_ulong hash) {
	return ZSTR_H(&entry->key) == hash
		&& ZSTR_LEN(&entry->key) == ZSTR_LEN(key)
		&& memcmp(ZSTR_VAL(&entry->key), ZSTR_VAL(key), ZSTR_LEN(key)) == 0;
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

/* apc_cache_wlocked_move_entry() is called during defragmentation, before an entry is moved to a new position. */
static zend_bool apc_cache_wlocked_move_entry(apc_cache_t *cache, apc_cache_entry_t *old, apc_cache_entry_t *new) {
	/* Check if the entry can be moved. */
	if (old->ref_count > 0) {
		return 0;
	}

	/* Change all references to this entry to the new position.
	 * Since “next” is the 1st field of apc_cache_entry_t, the head pointer of the list
	 * can be changed like a previous entry via ENTRYAT(old->prev)->next. */
	ENTRYAT(old->prev)->next = ENTRYOF(new);
	if (old->next) {
		ENTRYAT(old->next)->prev = ENTRYOF(new);
	}

	return 1;
}

/* Inserts an entry into a linked list. The argument entry_offset must point either
 * to entry->next of an existing entry or to the head pointer of a linked list. */
static void apc_cache_wlocked_link_entry(apc_cache_t *cache, uintptr_t *entry_offset, apc_cache_entry_t *entry) {
	entry->next = *entry_offset;
	entry->prev = ENTRYOF(entry_offset);
	*entry_offset = ENTRYOF(entry);
	if (entry->next) {
		ENTRYAT(entry->next)->prev = *entry_offset;
	}
}

/* Removes an entry from a linked list. */
static void apc_cache_wlocked_unlink_entry(apc_cache_t *cache, apc_cache_entry_t *entry) {
	/* Since “next” is the 1st field of apc_cache_entry_t, the head pointer of the list
	 * can be changed like a previous entry via ENTRYAT(entry->prev)->next. */
	ENTRYAT(entry->prev)->next = entry->next;
	if (entry->next) {
		ENTRYAT(entry->next)->prev = entry->prev;
	}
}

/* {{{ apc_cache_wlocked_remove_entry  */
static void apc_cache_wlocked_remove_entry(apc_cache_t *cache, apc_cache_entry_t *entry)
{
    /* unlink entry from list */
	apc_cache_wlocked_unlink_entry(cache, entry);

	/* adjust header info */
	if (cache->header->mem_size)
		cache->header->mem_size -= entry->mem_size;

	if (cache->header->nentries)
		cache->header->nentries--;

	/* free entry if there are no references */
	if (entry->ref_count <= 0) {
		free_entry(cache, entry);
	} else {
		/* add to gc if there are still refs */
		entry->dtime = time(0);
		apc_cache_wlocked_link_entry(cache, &cache->header->gc, entry);
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

	time_t now = time(0);

	uintptr_t *entry_offset = &cache->header->gc;
	while (*entry_offset) {
		apc_cache_entry_t *entry = ENTRYAT(*entry_offset);
		time_t gc_sec = cache->gc_ttl ? (now - entry->dtime) : 0;

		if (entry->ref_count > 0 && gc_sec <= (time_t)cache->gc_ttl) {
			entry_offset = &entry->next;
			continue;
		}

		/* good ol' whining */
		if (entry->ref_count > 0) {
			apc_debug(
				"GC cache entry '%s' was on gc-list for %lld seconds",
				ZSTR_VAL(&entry->key), (long long) gc_sec
			);
		}

		/* set next and free current entry */
		apc_cache_wlocked_unlink_entry(cache, entry);
		free_entry(cache, entry);
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
		php_error_docref(NULL, E_NOTICE, "Error at offset %td of %zd bytes", tmp - buf, buf_len);
		ZVAL_NULL(value);
		return 0;
	}
	return 1;
} /* }}} */

/* {{{ apc_cache_create */
PHP_APCU_API apc_cache_t* apc_cache_create(apc_sma_t* sma, apc_serializer_t* serializer, zend_long size_hint, zend_long gc_ttl, zend_long ttl, zend_long smart, zend_bool defend) {
	apc_cache_t* cache;
	zend_long cache_size;
	size_t nslots;

	/* calculate number of slots. Default: 512 slots per MB of shared memory */
	nslots = make_prime(size_hint > 0 ? (size_t)size_hint : sma->size / 2048);

	/* allocate pointer by normal means */
	cache = pemalloc(sizeof(apc_cache_t), 1);

	/* calculate cache size for shm allocation */
	cache_size = sizeof(apc_cache_header_t) + nslots * sizeof(uintptr_t);

	/* allocate shm */
	cache->header = apc_sma_malloc(sma, cache_size, NULL);

	if (!cache->header) {
		zend_error_noreturn(E_CORE_ERROR, "Unable to allocate " ZEND_LONG_FMT " bytes of shared memory for cache structures. Either apc.shm_size is too small or apc.entries_hint too large", cache_size);
		return NULL;
	}

	/* zero cache header and hash slots */
	memset(cache->header, 0, cache_size);

	/* set header values */
	cache->header->nhits = 0;
	cache->header->nmisses = 0;
	cache->header->nentries = 0;
	cache->header->ncleanups = 0;
	cache->header->ndefragmentations = 0;
	cache->header->nexpunges = 0;
	cache->header->gc = 0;
	cache->header->stime = time(NULL);

	/* set cache options */
	cache->slots = (uintptr_t *)((uintptr_t)cache->header + sizeof(apc_cache_header_t));
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
	zend_string *key = &new_entry->key;
	time_t t = new_entry->ctime;
	zend_ulong h;
	size_t s;

	/* process deleted list  */
	apc_cache_wlocked_gc(cache);

	/* calculate hash and entry */
	apc_cache_hash_slot(cache, key, &h, &s);

	uintptr_t *entry_offset = &cache->slots[s];
	while (*entry_offset) {
		apc_cache_entry_t *entry = ENTRYAT(*entry_offset);

		/* check for a match by hash and string */
		if (apc_entry_key_equals(entry, key, h)) {
			/*
			 * At this point we have found the user cache entry.  If we are doing
			 * an exclusive insert (apc_add) we are going to bail right away if
			 * the user entry already exists and is not hard expired.
			 */
			if (exclusive && !apc_cache_entry_hard_expired(entry, t)) {
				return 0;
			}

			apc_cache_wlocked_remove_entry(cache, entry);
			break;
		}

		/*
		 * This is a bit nasty. The idea here is to do runtime cleanup of the linked list of
		 * entries, so we don't always have to skip past a bunch of stale entries.
		 */
		if (apc_cache_entry_expired(cache, entry, t)) {
			apc_cache_wlocked_remove_entry(cache, entry);
			continue;
		}

		/* set next entry */
		entry_offset = &entry->next;
	}

	/* link in new entry */
	apc_cache_wlocked_link_entry(cache, entry_offset, new_entry);

	cache->header->mem_size += new_entry->mem_size;
	cache->header->nentries++;
	cache->header->ninserts++;

	return 1;
}

static void apc_cache_set_entry_values(apc_cache_entry_t *entry, const int32_t ttl, const time_t t)
{
	entry->ttl = ttl;
	entry->next = 0;
	entry->prev = 0;
	entry->nhits = 0;
	entry->ctime = t;
	entry->mtime = t;
	entry->atime = t;
	entry->dtime = 0;
}

/* TODO This function may lead to a deadlock on expunge */
static inline zend_bool apc_cache_store_internal(
		apc_cache_t *cache, zend_string *key, const zval *val,
		const int32_t ttl, const zend_bool exclusive) {
	time_t t = apc_time();

	if (apc_cache_defense(cache, key, t)) {
		return 0;
	}

	/* create entry in the shared memory */
	apc_cache_entry_t *entry = apc_persist(cache->sma, cache->serializer, key, val);
	if (!entry) {
		return 0;
	}

	/* init remaining values of the entry */
	apc_cache_set_entry_values(entry, ttl, t);

	/* execute an insertion */
	if (!apc_cache_wlocked_insert(cache, entry, exclusive)) {
		free_entry(cache, entry);
		return 0;
	}

	/* release entry, because the ref_count of a new entry is initialized to 1 during allocation */
	apc_cache_entry_release(cache, entry);

	return 1;
}

/* Find entry, without updating stat counters or access time */
static inline apc_cache_entry_t *apc_cache_rlocked_find_nostat(
		apc_cache_t *cache, zend_string *key, time_t t) {
	zend_ulong h;
	size_t s;

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	uintptr_t entry_offset = cache->slots[s];
	while (entry_offset) {
		apc_cache_entry_t *entry = ENTRYAT(entry_offset);

		/* check for a matching key by has and identifier */
		if (apc_entry_key_equals(entry, key, h)) {
			/* Check to make sure this entry isn't expired by a hard TTL */
			if (apc_cache_entry_hard_expired(entry, t)) {
				break;
			}

			return entry;
		}

		entry_offset = entry->next;
	}

	return NULL;
}

/* Find entry, updating stat counters and access time */
static inline apc_cache_entry_t *apc_cache_rlocked_find(
		apc_cache_t *cache, zend_string *key, time_t t) {

	zend_ulong h;
	size_t s;

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	uintptr_t entry_offset = cache->slots[s];
	while (entry_offset) {
		apc_cache_entry_t *entry = ENTRYAT(entry_offset);

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

		entry_offset = entry->next;
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
	time_t t = apc_time();
	zend_bool ret = 0;

	if (!cache) {
		return 0;
	}

	/* run cache defense */
	if (apc_cache_defense(cache, key, t)) {
		return 0;
	}

	/* create entry in the shared memory */
	apc_cache_entry_t *entry = apc_persist(cache->sma, cache->serializer, key, val);
	if (!entry) {
		return 0;
	}

	/* init remaining values of the entry */
	apc_cache_set_entry_values(entry, ttl, t);

	/* execute an insertion */
	if (!apc_cache_wlock(cache)) {
		free_entry(cache, entry);
		return 0;
	}

	php_apc_try {
		ret = apc_cache_wlocked_insert(cache, entry, exclusive);
	} php_apc_finally {
		apc_cache_wunlock(cache);

		if (ret) {
			/* release entry, because the ref_count of a new entry is initialized to 1 during allocation */
			apc_cache_entry_release(cache, entry);
		} else {
			/* the entry mustn't be released before it is freed to prevent defragmentation from moving the entry */
			free_entry(cache, entry);
		}
	} php_apc_end_try();

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
	size_t i;

	/* increment counter */
	cache->header->nexpunges++;

	/* expunge */
	for (i = 0; i < cache->nslots; i++) {
		uintptr_t *entry_offset = &cache->slots[i];
		while (*entry_offset) {
			apc_cache_wlocked_remove_entry(cache, ENTRYAT(*entry_offset));
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
	cache->header->ncleanups = 0;
	cache->header->ndefragmentations = 0;
	cache->header->nexpunges = 0;

	apc_cache_wunlock(cache);
}
/* }}} */

/* {{{ apc_cache_default_expunge */
PHP_APCU_API zend_bool apc_cache_default_expunge(apc_cache_t* cache, size_t size)
{
	time_t t;
	size_t i;

	if (!cache) {
		return 1;
	}

	/* get the number of cleanups before acquiring the lock */
	zend_long ncleanups = cache->header->ncleanups;

	/* apc_time() depends on globals, don't read it if there's no cache. This may happen if SHM
	 * is too small and the initial cache creation during MINIT triggers an expunge. */
	t = apc_time();

	/* get the lock for header */
	if (!apc_cache_wlock(cache)) {
		return 1;
	}

	/* skip processing if another default expunge operation was performed while waiting for the write lock */
	if (ncleanups < cache->header->ncleanups) {
		apc_cache_wunlock(cache);
		return 0;
	}

	/* smart > 1 increases the probability of a full cache wipe,
	 * so expunge() is called less often when memory is low. */
	size = (cache->smart > 0L) ? (size_t) (cache->smart * size) : size;

	/* look for junk */
	for (i = 0; i < cache->nslots; i++) {
		uintptr_t *entry_offset = &cache->slots[i];
		while (*entry_offset) {
			apc_cache_entry_t *entry = ENTRYAT(*entry_offset);

			if (apc_cache_entry_expired(cache, entry, t)) {
				apc_cache_wlocked_remove_entry(cache, entry);
				continue;
			}

			/* grab next entry */
			entry_offset = &entry->next;
		}
	}

	/* gc */
	apc_cache_wlocked_gc(cache);

	/* if all free blocks together do not provide enough memory, we immediately perform a real expunge */
	if (!apc_sma_check_avail(cache->sma, size)) {
		apc_cache_wlocked_real_expunge(cache);
		goto end_lbl;
	}

	/* increment defragmentation statistics */
	cache->header->ndefragmentations++;

	/* run defragmentation to coalesce free blocks */
	apc_sma_defrag(cache->sma, cache, (apc_sma_move_f)apc_cache_wlocked_move_entry);

	/* if size bytes can't be allocated as a contiguous block after defragmentation, we do a real expunge */
	if (!apc_sma_check_avail_contiguous(cache->sma, size)) {
		apc_cache_wlocked_real_expunge(cache);
		goto end_lbl;
	}

	/* wipe lastkey */
	memset(&cache->header->lastkey, 0, sizeof(apc_cache_slam_key_t));

end_lbl:
	/* Increment cache cleanup statistics (removal of expired entries).
	 * This should be done late to detect stacking of default expunge operations. */
	cache->header->ncleanups++;

	apc_cache_wunlock(cache);
	return 1;
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

	entry = apc_cache_rlocked_find(cache, key, t);
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
	zend_ulong h;
	size_t s;

	if (!cache) {
		return 0;
	}

	/* calculate hash and slot */
	apc_cache_hash_slot(cache, key, &h, &s);

	if (!apc_cache_wlock(cache)) {
		return 0;
	}

	/* find head */
	uintptr_t *entry_offset = &cache->slots[s];
	while (*entry_offset) {
		apc_cache_entry_t *entry = ENTRYAT(*entry_offset);

		/* check for a match by hash and identifier */
		if (apc_entry_key_equals(entry, key, h)) {
			/* executing removal */
			apc_cache_wlocked_remove_entry(cache, entry);

			apc_cache_wunlock(cache);
			return 1;
		}

		entry_offset = &entry->next;
	}

	apc_cache_wunlock(cache);
	return 0;
}
/* }}} */

/* {{{ apc_cache_entry_fetch_zval */
PHP_APCU_API zend_bool apc_cache_entry_fetch_zval(
		apc_cache_t *cache, apc_cache_entry_t *entry, zval *dst)
{
	return apc_unpersist(dst, entry, cache->serializer);
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

	ZVAL_STR(&zv, zend_string_dup(&p->key, 0));
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
	uintptr_t entry_offset;
	zend_ulong j;

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
		add_assoc_long(info, "cleanups", cache->header->ncleanups);
		add_assoc_long(info, "defragmentations", cache->header->ndefragmentations);
		add_assoc_long(info, "expunges", cache->header->nexpunges);
		add_assoc_long(info, "start_time", cache->header->stime);
		array_add_double(info, apc_str_mem_size, (double) cache->header->mem_size);

#ifdef APC_MMAP
		add_assoc_stringl(info, "memory_type", "mmap", sizeof("mmap")-1);
#else
		add_assoc_stringl(info, "memory_type", "IPC shared", sizeof("IPC shared")-1);
#endif

		if (!limited) {
			size_t i;

			/* For each hashtable slot */
			array_init(&list);
			array_init(&slots);

			for (i = 0; i < cache->nslots; i++) {
				j = 0;
				entry_offset = cache->slots[i];
				while (entry_offset) {
					apc_cache_entry_t *entry = ENTRYAT(entry_offset);
					zval link = apc_cache_link_info(cache, entry);

					add_next_index_zval(&list, &link);
					j++;
					entry_offset = entry->next;
				}
				if (j != 0) {
					add_index_long(&slots, (zend_ulong)i, j);
				}
			}

			/* For each slot pending deletion */
			array_init(&gc);

			entry_offset = cache->header->gc;
			while (entry_offset) {
				apc_cache_entry_t *entry = ENTRYAT(entry_offset);
				zval link = apc_cache_link_info(cache, entry);

				add_next_index_zval(&gc, &link);
				entry_offset = entry->next;
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
	zend_ulong h;
	size_t s;

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
		uintptr_t entry_offset = cache->slots[s];
		while (entry_offset) {
			apc_cache_entry_t *entry = ENTRYAT(entry_offset);

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
			entry_offset = entry->next;
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
#ifdef ZTS
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
