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

/* $Id: apc_cache.h 328172 2012-10-28 21:44:47Z rasmus $ */

#ifndef APC_CACHE_H
#define APC_CACHE_H

/*
 * This module defines the shared memory file cache. Basically all of the
 * logic for storing and retrieving cache entries lives here.
 */

#include "apc.h"
#include "apc_lock.h"
#include "apc_pool.h"
#include "apc_main.h"
#include "TSRM.h"

#ifdef PHP_WIN32
typedef unsigned __int64 apc_ino_t;
typedef unsigned __int64 apc_dev_t;
#else
typedef ino_t apc_ino_t;
typedef dev_t apc_dev_t;
#endif

/* {{{ cache locking macros */
#define CACHE_LOCK(cache)        { WLOCK(&cache->header->lock);   cache->has_lock = 1; }
#define CACHE_UNLOCK(cache)      { WUNLOCK(&cache->header->lock); cache->has_lock = 0; }
#define CACHE_SAFE_LOCK(cache)   CACHE_LOCK(&cache->header->lock)
#define CACHE_SAFE_UNLOCK(cache) CACHE_UNLOCK(&cache->header->lock)
#define CACHE_FAST_INC(cache, obj) { obj++; }
#define CACHE_FAST_DEC(cache, obj) { obj--; }
/* }}} */

/* {{{ struct definition: apc_cache_key_t */
#define T apc_cache_t*
typedef struct apc_cache_t apc_cache_t; /* opaque cache type */

typedef struct _apc_cache_key_data_t {
    const char *identifier;
    int identifier_len;
} apc_cache_key_data_t;

typedef struct apc_cache_key_t apc_cache_key_t;
struct apc_cache_key_t {
    apc_cache_key_data_t data;
    unsigned long h;              /* pre-computed hash value */
    time_t mtime;                 /* the mtime of this cached entry */
};

typedef struct apc_keyid_t apc_keyid_t;

struct apc_keyid_t {
    unsigned int h;
    unsigned int keylen;
    time_t mtime;
#ifdef ZTS
    THREAD_T tid;
#else
    pid_t pid;
#endif
};
/* }}} */

/* {{{ struct definition: apc_cache_entry_t */
typedef struct _apc_cache_entry_value_t {
        char *info;
        int info_len;
        zval *val;
        unsigned int ttl;
} apc_cache_entry_value_t;

typedef struct apc_cache_entry_t apc_cache_entry_t;
struct apc_cache_entry_t {
    apc_cache_entry_value_t data;
    int ref_count;
    size_t mem_size;
    apc_pool *pool;
};
/* }}} */

/*
 * apc_cache_create creates the shared memory compiler cache. This function
 * should be called just once (ideally in the web server parent process, e.g.
 * in apache), otherwise you will end up with multiple caches (which won't
 * necessarily break anything). Returns a pointer to the cache object.
 *
 * size_hint is a "hint" at the total number of source files that will be
 * cached. It determines the physical size of the hash table. Passing 0 for
 * this argument will use a reasonable default value.
 *
 * gc_ttl is the maximum time a cache entry may speed on the garbage
 * collection list. This is basically a work around for the inherent
 * unreliability of our reference counting mechanism (see apc_cache_release).
 *
 * ttl is the maximum time a cache entry can idle in a slot in case the slot
 * is needed.  This helps in cleaning up the cache and ensuring that entries 
 * hit frequently stay cached and ones not hit very often eventually disappear.
 */
extern T apc_cache_create(int size_hint, int gc_ttl, int ttl TSRMLS_DC);

/*
 * apc_cache_destroy releases any OS resources associated with a cache object.
 * Under apache, this function can be safely called by the child processes
 * when they exit.
 */
extern void apc_cache_destroy(T cache TSRMLS_DC);

/*
 * apc_cache_clear empties a cache. This can safely be called at any time,
 * even while other server processes are executing cached source files.
 */
extern void apc_cache_clear(T cache TSRMLS_DC);

/*
 * apc_cache_user_insert adds an entry to the cache.
 * Returns non-zero if the entry was successfully inserted, 0 otherwise. 
 * If 0 is returned, the caller must free the cache entry by calling
 * apc_cache_free_entry (see below).
 *
 * key is the value created by apc_cache_make_user_key for file keys.
 *
 * value is a cache entry returned by apc_cache_make_entry (see below).
 */
extern int apc_cache_user_insert(T cache, apc_cache_key_t key,
                            apc_cache_entry_t* value, apc_context_t* ctxt, time_t t, int exclusive TSRMLS_DC);

/*
 * apc_cache_user_find searches for a cache entry by its hashed identifier,
 * and returns a pointer to the entry if found, NULL otherwise.
 *
 */
extern apc_cache_entry_t* apc_cache_user_find(T cache, char* strkey, int keylen, time_t t TSRMLS_DC);

/*
 * apc_cache_user_exists searches for a cache entry by its hashed identifier,
 * and returns a pointer to the entry if found, NULL otherwise.  This is a
 * quick non-locking version of apc_cache_user_find that does not modify the
 * shared memory segment in any way.
 *
 */
extern apc_cache_entry_t* apc_cache_user_exists(T cache, char* strkey, int keylen, time_t t TSRMLS_DC);

/*
 * apc_cache_delete and apc_cache_user_delete finds an entry in the cache and deletes it.
 */
extern int apc_cache_user_delete(apc_cache_t* cache, char *strkey, int keylen TSRMLS_DC);

/* apc_cach_fetch_zval takes a zval in the cache and reconstructs a runtime
 * zval from it.
 *
 */
zval* apc_cache_fetch_zval(zval* dst, const zval* src, apc_context_t* ctxt TSRMLS_DC);

/*
 * apc_cache_release decrements the reference count associated with a cache
 * entry. Calling apc_cache_find automatically increments the reference count,
 * and this function must be called post-execution to return the count to its
 * original value. Failing to do so will prevent the entry from being
 * garbage-collected.
 *
 * entry is the cache entry whose ref count you want to decrement.
 */
extern void apc_cache_release(T cache, apc_cache_entry_t* entry TSRMLS_DC);

/*
 * apc_cache_make_user_entry creates an apc_cache_entry_t object given an info string
 * and the zval to be stored.
 */
extern apc_cache_entry_t* apc_cache_make_user_entry(const char* info, int info_len, const zval *val, apc_context_t* ctxt, const unsigned int ttl TSRMLS_DC);

extern int apc_cache_make_user_key(apc_cache_key_t* key, char* identifier, int identifier_len, const time_t t);

/* {{{ struct definition: slot_t */
typedef struct slot_t slot_t;
struct slot_t {
    apc_cache_key_t key;        /* slot key */
    apc_cache_entry_t* value;   /* slot value */
    slot_t* next;               /* next slot in linked list */
    unsigned long num_hits;     /* number of hits to this bucket */
    time_t creation_time;       /* time slot was initialized */
    time_t deletion_time;       /* time slot was removed from cache */
    time_t access_time;         /* time slot was last accessed */
};
/* }}} */

/* {{{ struct definition: cache_header_t
   Any values that must be shared among processes should go in here. */
typedef struct cache_header_t cache_header_t;
struct cache_header_t {
    apc_lock_t lock;            /* read/write lock (exclusive blocking cache lock) */
    unsigned long num_hits;     /* total successful hits in cache */
    unsigned long num_misses;   /* total unsuccessful hits in cache */
    unsigned long num_inserts;  /* total successful inserts in cache */
    unsigned long expunges;     /* total number of expunges */
    slot_t* deleted_list;       /* linked list of to-be-deleted slots */
    time_t start_time;          /* time the above counters were reset */
    zend_bool busy;             /* Flag to tell clients when we are busy cleaning the cache */
    int num_entries;            /* Statistic on the number of entries */
    size_t mem_size;            /* Statistic on the memory size used by this cache */
    apc_keyid_t lastkey;        /* the key that is being inserted (user cache) */
};
/* }}} */

typedef void (*apc_expunge_cb_t)(T cache, size_t n TSRMLS_DC); 

/* {{{ struct definition: apc_cache_t */
struct apc_cache_t {
    void* shmaddr;                /* process (local) address of shared cache */
    cache_header_t* header;       /* cache header (stored in SHM) */
    slot_t** slots;               /* array of cache slots (stored in SHM) */
    int num_slots;                /* number of slots in cache */
    int gc_ttl;                   /* maximum time on GC list for a slot */
    int ttl;                      /* if slot is needed and entry's access time is older than this ttl, remove it */
    apc_expunge_cb_t expunge_cb;  /* cache specific expunge callback to free up sma memory */
    uint has_lock;                /* flag for possible recursive locks within the same process */
};
/* }}} */

#if !defined(_WIN32) && !defined(ZTS)
/* {{{ struct definition: apc_async_insert_t */
typedef struct _apc_async_insert {
	apc_cache_t* cache;			 /* the cache to insert into */
	apc_context_t  ctx;          /* context in which the insertion is made */
	apc_cache_key_t key;		 /* the key to insert */
	apc_cache_entry_t *entry;	 /* the value to insert */
	time_t ctime;				 /* ctime for the entry */
	int ttl;                     /* ttl for entry */
	int exclusive;         		 /* exclusivity: add/store */
	pthread_t thread;            /* the thread doing the work */
} apc_async_insert_t; /* }}} */

typedef void* (*apc_async_worker_t) (void *insert);

/*
* apc_cache_make_async_insert creates an apc_async_insert_t to be inserted in a separate context
*/
extern apc_async_insert_t* apc_cache_make_async_insert(char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC);
#endif

extern zval* apc_cache_info(T cache, zend_bool limited TSRMLS_DC);
extern void apc_cache_unlock(apc_cache_t* cache TSRMLS_DC);
extern zend_bool apc_cache_busy(apc_cache_t* cache);
extern zend_bool apc_cache_write_lock(apc_cache_t* cache TSRMLS_DC);
extern void apc_cache_write_unlock(apc_cache_t* cache TSRMLS_DC);
extern zend_bool apc_cache_is_last_key(apc_cache_t* cache, apc_cache_key_t* key, time_t t TSRMLS_DC);

/* used by apc_rfc1867 to update data in-place - not to be used elsewhere */

typedef int (*apc_cache_updater_t)(apc_cache_t*, apc_cache_entry_t*, void* data);
extern int _apc_cache_user_update(apc_cache_t* cache, char *strkey, int keylen,
                                    apc_cache_updater_t updater, void* data TSRMLS_DC);

#undef T
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
