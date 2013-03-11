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

#ifndef APC_CACHE_API_H
#define APC_CACHE_API_H

/* {{{ used for slam defense to determine the context which created any key */
#ifdef ZTS
typedef void*** apc_cache_owner_t;
#else
typedef pid_t apc_cache_owner_t;
#endif /* }}} */

/* {{{ struct definition: apc_cache_key_t */
typedef struct _apc_cache_key_t {
    const char *identifier;		  /* the identifier for this key */
    int identifier_len;			  /* the length of the identifier string */
    zend_ulong h;                 /* pre-computed hash */
	zend_ulong s;                 /* pre-computed position */
    time_t mtime;                 /* the mtime of this cached entry */
	apc_cache_owner_t owner;      /* the context that created this key */
} apc_cache_key_t; /* }}} */

/* {{{ struct definition: apc_cache_entry_t */
typedef struct _apc_cache_entry_t {
    zval *val;                    /* the zval copied at store time */
    unsigned int ttl;             /* the ttl on this specific entry */
    int ref_count;                /* the reference count of this entry */
    size_t mem_size;              /* memory used */
    apc_pool *pool;               /* pool which allocated the value */
} apc_cache_entry_t;
/* }}} */

/* {{{ struct definition: apc_cache_slot_t */
typedef struct apc_cache_slot_t apc_cache_slot_t;
struct apc_cache_slot_t {
    apc_cache_key_t key;        /* slot key */
    apc_cache_entry_t* value;   /* slot value */
    apc_cache_slot_t* next;     /* next slot in linked list */
    unsigned long num_hits;     /* number of hits to this bucket */
    time_t creation_time;       /* time slot was initialized */
    time_t deletion_time;       /* time slot was removed from cache */
    time_t access_time;         /* time slot was last accessed */
};
/* }}} */

/* {{{ struct definition: apc_cache_header_t
   Any values that must be shared among processes should go in here. */
typedef struct _apc_cache_header_t {
	apc_lock_t lock;                 /* header lock */
	apc_sma_t* sma;                  /* shared memory allocator */
    unsigned long num_hits;          /* total successful hits in cache */
    unsigned long num_misses;        /* total unsuccessful hits in cache */
    unsigned long num_inserts;       /* total successful inserts in cache */
    unsigned long expunges;          /* total number of expunges */
    apc_cache_slot_t* deleted_list;  /* linked list of to-be-deleted slots */
    time_t start_time;               /* time the above counters were reset */
    volatile zend_ushort state;      /* cache state */
    int num_entries;                 /* Statistic on the number of entries */
    size_t mem_size;                 /* Statistic on the memory size used by this cache */
	long smart;                      /* adjustable smart expunges of data */
    apc_cache_key_t lastkey;         /* information about the last key inserted */
} apc_cache_header_t;
/* }}} */

/* {{{ state constants */
#define APC_CACHE_ST_NONE 0
#define APC_CACHE_ST_BUSY 1
#define APC_CACHE_ST_PROC 2
#define APC_CACHE_ST_IBUSY (APC_CACHE_ST_BUSY|APC_CACHE_ST_PROC) /* }}} *

/* {{{ struct definition: apc_cache_t */
typedef struct apc_cache_t apc_cache_t;
typedef void (*apc_expunge_cb_t)(apc_cache_t* cache, size_t n TSRMLS_DC); 
struct apc_cache_t {
    void* shmaddr;                /* process (local) address of shared cache */
    apc_cache_header_t* header;   /* cache header (stored in SHM) */
    apc_cache_slot_t** slots;     /* array of cache slots (stored in SHM) */
    int num_slots;                /* number of slots in cache */
    int gc_ttl;                   /* maximum time on GC list for a slot */
    int ttl;                      /* if slot is needed and entry's access time is older than this ttl, remove it */
    apc_expunge_cb_t expunge_cb;  /* cache specific expunge callback to free up sma memory */
};
/* }}} */

/* {{{ typedef: apc_cache_updater_t */
typedef zend_bool (*apc_cache_updater_t)(apc_cache_t*, apc_cache_entry_t*, void* data); /* }}} */

/*
 * apc_cache_create creates the shared memory cache. 
 *
 * This function should be called once per process per cache
 *
 * size_hint is a "hint" at the total number entries that will be expected. 
 * It determines the physical size of the hash table. Passing 0 for
 * this argument will use a reasonable default value (2000)
 * Note: APCG(entries_hint)
 * 
 * gc_ttl is the maximum time a cache entry may speed on the garbage
 * collection list. This is basically a work around for the inherent
 * unreliability of our reference counting mechanism (see apc_cache_release).
 *
 * ttl is the maximum time a cache entry can idle in a slot in case the slot
 * is needed.  This helps in cleaning up the cache and ensuring that entries 
 * hit frequently stay cached and ones not hit very often eventually disappear.
 *
 */
extern apc_cache_t* apc_cache_create(apc_sma_t* sma,
                                     int size_hint,
                                     int gc_ttl,
                                     int ttl,
                                     long smart TSRMLS_DC);
/*
* apc_cache_preload preloads the data at path into the specified cache
*/
extern zend_bool apc_cache_preload(apc_cache_t* cache,
                                   const char* path TSRMLS_DC);

/*
 * apc_cache_destroy releases any OS resources associated with a cache object.
 * Under apache, this function can be safely called by the child processes
 * when they exit.
 */
extern void apc_cache_destroy(apc_cache_t* cache TSRMLS_DC);

/*
 * apc_cache_clear empties a cache. This can safely be called at any time.
 */
extern void apc_cache_clear(apc_cache_t* cache TSRMLS_DC);

/* {{{ apc_cache_default_expunge 
* Where smart is not set:
*  Where no ttl is set on cache:
*   Expunge if available memory is less than seg_size/2
*  Where ttl is set on cache:
*   Perform cleanup of stale entries
*   If available memory if less than the size requested, run full expunge
*
* Where smart is set:
*  Where no ttl is set on cache:
*   Expunge is available memory is less than size * smart
*  Where ttl is set on cache:
*   If available memory if less than the size requested, run full expunge
*
* The TTL of an entry takes precedence over the TTL of a cache
*/
extern void apc_cache_default_expunge(apc_cache_t* cache, size_t size TSRMLS_DC);

/*
* apc_cache_make_context initializes a context with an appropriate pool and options provided
*
* Some of the APC API requires a context in which to operate
*
* The type of context required depends on the operation being performed, for example
* an insert should happen in a shared context, a fetch should happen in a nonshared context
*/
extern zend_bool apc_cache_make_context(apc_cache_t* cache,
                                        apc_context_t* context, 
                                        apc_context_type context_type, 
                                        apc_pool_type pool_type,
                                        apc_copy_type copy_type,
                                        uint force_update TSRMLS_DC);

/*
* apc_cache_make_context_ex is an advanced/external version of make_context
*/
extern zend_bool apc_cache_make_context_ex(apc_context_t* context,
                                           apc_malloc_t _malloc, 
                                           apc_free_t _free, 
                                           apc_protect_t _protect, 
                                           apc_unprotect_t _unprotect, 
                                           apc_pool_type pool_type, 
                                           apc_copy_type copy_type, 
                                           uint force_update TSRMLS_DC);
/*
* apc_context_destroy should be called when a context is finished being used 
*/
extern zend_bool apc_cache_destroy_context(apc_context_t* context TSRMLS_DC);

/*
 * apc_cache_insert adds an entry to the cache.
 * Returns true if the entry was successfully inserted, false otherwise. 
 * If false is returned, the caller must free the cache entry by calling
 * apc_cache_free_entry (see below).
 *
 * key is the value created by apc_cache_make_key
 *
 * value is a cache entry returned by apc_cache_make_entry (see below).
 *
 * an easier (and faster for bulk data) API exists in the form of apc_cache_store|all
 */
extern zend_bool apc_cache_insert(apc_cache_t* cache,
                                  apc_cache_key_t key,
                                  apc_cache_entry_t* value,
                                  apc_context_t* ctxt,
                                  time_t t,
                                  int exclusive TSRMLS_DC);

/* 
 * apc_cache_store creates key, entry and context in which to make an insertion of val into the specified cache
 */
extern zend_bool apc_cache_store(apc_cache_t* cache,
                                 char *strkey,
                                 int strkey_len,
                                 const zval *val,
                                 const unsigned int ttl,
                                 const int exclusive TSRMLS_DC);

/*
* apc_cache_store_all takes an array of data, retains the lock while all insertions are made
*  results is populated with -1 where there is an error inserting an entry from the set
*/
extern zend_bool apc_cache_store_all(apc_cache_t* cache,
                                     zval *data,
                                     zval *results,
                                     const unsigned int ttl,
                                     const int exclusive TSRMLS_DC);

/*
* apc_cache_update updates an entry in place, this is used for rfc1867 and inc/dec/cas
*/
extern zend_bool apc_cache_update(apc_cache_t* cache,
                                  char *strkey,
                                  int keylen,
                                  apc_cache_updater_t updater,
                                  void* data TSRMLS_DC);

/*
 * apc_cache_find searches for a cache entry by its hashed identifier,
 * and returns a pointer to the entry if found, NULL otherwise.
 *
 */
extern apc_cache_entry_t* apc_cache_find(apc_cache_t* cache,
                                         char* strkey,
                                         int keylen,
                                         time_t t TSRMLS_DC);

/*
 * apc_cache_fetch fetches an entry from the cache directly into dst
 *
 */
extern zend_bool apc_cache_fetch(apc_cache_t* cache,
                                 char* strkey,
                                 int keylen,
                                 time_t t,
                                 zval **dst TSRMLS_DC);

/*
 * apc_cache_exists searches for a cache entry by its hashed identifier,
 * and returns a pointer to the entry if found, NULL otherwise.  This is a
 * quick non-locking version of apc_cache_find that does not modify the
 * shared memory segment in any way.
 *
 */
extern apc_cache_entry_t* apc_cache_exists(apc_cache_t* cache,
                                           char* strkey,
                                           int keylen,
                                           time_t t TSRMLS_DC);

/*
 * apc_cache_delete and apc_cache_delete finds an entry in the cache and deletes it.
 */
extern zend_bool apc_cache_delete(apc_cache_t* cache,
                                  char *strkey,
                                  int keylen TSRMLS_DC);

/* apc_cach_fetch_zval takes a zval in the cache and reconstructs a runtime
 * zval from it.
 *
 */
extern zval* apc_cache_fetch_zval(apc_context_t* ctxt,
                                  zval* dst,
                                  const zval* src TSRMLS_DC);

/*
 * apc_cache_release decrements the reference count associated with a cache
 * entry. Calling apc_cache_find automatically increments the reference count,
 * and this function must be called post-execution to return the count to its
 * original value. Failing to do so will prevent the entry from being
 * garbage-collected.
 *
 * entry is the cache entry whose ref count you want to decrement.
 */
extern void apc_cache_release(apc_cache_t* cache,
                              apc_cache_entry_t* entry TSRMLS_DC);

/*
* apc_cache_make_key creates an apc_cache_key_t from an identifier, it's length and the current time
*/
extern zend_bool apc_cache_make_key(apc_cache_key_t* key,
                                    char* identifier,
                                    int identifier_len TSRMLS_DC);

/*
 * apc_cache_make_entry creates an apc_cache_entry_t given a zval, context and ttl
 */
extern apc_cache_entry_t* apc_cache_make_entry(apc_context_t* ctxt,
                                               const zval *val,
                                               const unsigned int ttl TSRMLS_DC);

/*
 fetches information about the cache provided for userland status functions
*/
extern zval* apc_cache_info(apc_cache_t* cache,
                            zend_bool limited TSRMLS_DC);

/*
* apc_cache_busy returns true while the cache is being cleaned
*/
extern zend_bool apc_cache_busy(apc_cache_t* cache TSRMLS_DC);

/*
* apc_cache_processing returns true while the cache is in collection
*/
extern zend_bool apc_cache_processing(apc_cache_t* cache TSRMLS_DC);

/*
* apc_cache_defense: guard against slamming a key
*  will return true if the following conditions are met:
*	the key provided has a matching hash and length to the last key inserted into cache
*   the last key has a different owner
* in ZTS mode, TSRM determines owner
* in non-ZTS mode, PID determines owner
* Note: this function sets the owner of key during execution
*/
extern zend_bool apc_cache_defense(apc_cache_t* cache,
                                   apc_cache_key_t* key TSRMLS_DC);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
