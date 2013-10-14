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
typedef struct apc_cache_key_t apc_cache_key_t;
struct apc_cache_key_t {
    const char *str;		      /* pointer to constant string key */
    zend_uint len;                /* length of data at str */
    zend_ulong h;                 /* pre-computed hash of key */
    time_t mtime;                 /* the mtime of this cached entry */
    apc_cache_owner_t owner;      /* the context that created this key */
}; /* }}} */

/* {{{ struct definition: apc_cache_entry_t */
typedef struct apc_cache_entry_t apc_cache_entry_t;
struct apc_cache_entry_t {
    zval *val;                    /* the zval copied at store time */
    zend_uint ttl;                /* the ttl on this specific entry */
    int ref_count;                /* the reference count of this entry */
    size_t mem_size;              /* memory used */
    apc_pool *pool;               /* pool which allocated the value */
};
/* }}} */

/* {{{ struct definition: apc_cache_slot_t */
typedef struct apc_cache_slot_t apc_cache_slot_t;
struct apc_cache_slot_t {
    apc_cache_key_t key;        /* slot key */
    apc_cache_entry_t* value;   /* slot value */
    apc_cache_slot_t* next;     /* next slot in linked list */
    zend_ulong nhits;           /* number of hits to this slot */
    time_t ctime;               /* time slot was initialized */
    time_t dtime;               /* time slot was removed from cache */
    time_t atime;               /* time slot was last accessed */
};
/* }}} */

/* {{{ state constants */
#define APC_CACHE_ST_NONE  0
#define APC_CACHE_ST_BUSY  0x00000001 /* }}} */

/* {{{ struct definition: apc_cache_header_t
   Any values that must be shared among processes should go in here. */
typedef struct _apc_cache_header_t {
    apc_lock_t lock;                 /* header lock */
    zend_ulong nhits;                /* hit count */
    zend_ulong nmisses;              /* miss count */
    zend_ulong ninserts;             /* insert count */
    zend_ulong nexpunges;            /* expunge count */
    zend_ulong nentries;             /* entry count */
    zend_ulong mem_size;             /* used */
    time_t stime;                    /* start time */
    zend_ushort state;               /* cache state */
    apc_cache_key_t lastkey;         /* last key inserted (not necessarily without error) */
    apc_cache_slot_t* gc;            /* gc list */
} apc_cache_header_t; /* }}} */

/* {{{ struct definition: apc_cache_t */
typedef struct _apc_cache_t {
    void* shmaddr;                /* process (local) address of shared cache */
    apc_cache_header_t* header;   /* cache header (stored in SHM) */
    apc_cache_slot_t** slots;     /* array of cache slots (stored in SHM) */
    apc_sma_t* sma;               /* shared memory allocator */
    apc_serializer_t* serializer; /* serializer */
    zend_ulong nslots;            /* number of slots in cache */
    zend_ulong gc_ttl;            /* maximum time on GC list for a slot */
    zend_ulong ttl;               /* if slot is needed and entry's access time is older than this ttl, remove it */
    zend_ulong smart;             /* smart parameter for gc */
    zend_bool defend;             /* defense parameter for runtime */
} apc_cache_t; /* }}} */

/* {{{ typedef: apc_cache_updater_t */
typedef zend_bool (*apc_cache_updater_t)(apc_cache_t*, apc_cache_entry_t*, void* data); /* }}} */

/*
 * apc_cache_create creates the shared memory cache. 
 *
 * This function should be called once per process per cache
 * 
 * serializer for APCu is set by globals on MINIT and ensured with apc_cache_serializer
 * during execution. Using apc_cache_serializer avoids race conditions between MINIT/RINIT of
 * APCU and the third party serializer. API users can choose to leave this null to use default
 * PHP serializers, or search the list of serializers for the preferred serializer
 *
 * size_hint is a "hint" at the total number entries that will be expected. 
 * It determines the physical size of the hash table. Passing 0 for
 * this argument will use a reasonable default value
 * 
 * gc_ttl is the maximum time a cache entry may speed on the garbage
 * collection list. This is basically a work around for the inherent
 * unreliability of our reference counting mechanism (see apc_cache_release).
 *
 * ttl is the maximum time a cache entry can idle in a slot in case the slot
 * is needed.  This helps in cleaning up the cache and ensuring that entries 
 * hit frequently stay cached and ones not hit very often eventually disappear.
 * 
 * for an explanation of smart, see apc_cache_default_expunge
 *
 * defend enables/disables slam defense for this particular cache
 */
PHP_APCU_API apc_cache_t* apc_cache_create(apc_sma_t* sma,
                                           apc_serializer_t* serializer,
                                           int size_hint,
                                           int gc_ttl,
                                           int ttl,
                                           long smart,
                                           zend_bool defend TSRMLS_DC);
/*
* apc_cache_preload preloads the data at path into the specified cache
*/
PHP_APCU_API zend_bool apc_cache_preload(apc_cache_t* cache,
                                         const char* path TSRMLS_DC);

/*
 * apc_cache_destroy releases any OS resources associated with a cache object.
 * Under apache, this function can be safely called by the child processes
 * when they exit.
 */
PHP_APCU_API void apc_cache_destroy(apc_cache_t* cache TSRMLS_DC);

/*
 * apc_cache_clear empties a cache. This can safely be called at any time.
 */
PHP_APCU_API void apc_cache_clear(apc_cache_t* cache TSRMLS_DC);

/*
* apc_cache_make_context initializes a context with an appropriate pool and options provided
*
* Some of the APC API requires a context in which to operate
*
* The type of context required depends on the operation being performed, for example
* an insert should happen in a shared context, a fetch should happen in a nonshared context
*/
PHP_APCU_API zend_bool apc_cache_make_context(apc_cache_t* cache,
                                              apc_context_t* context, 
                                              apc_context_type context_type, 
                                              apc_pool_type pool_type,
                                              apc_copy_type copy_type,
                                              uint force_update TSRMLS_DC);

/*
* apc_cache_make_context_ex is an advanced/external version of make_context
*/
PHP_APCU_API zend_bool apc_cache_make_context_ex(apc_context_t* context,
                                                 apc_serializer_t* serializer,
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
PHP_APCU_API zend_bool apc_cache_destroy_context(apc_context_t* context TSRMLS_DC);

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
 * an easier API exists in the form of apc_cache_store
 */
PHP_APCU_API zend_bool apc_cache_insert(apc_cache_t* cache,
                                        apc_cache_key_t key,
                                        apc_cache_entry_t* value,
                                        apc_context_t* ctxt,
                                        time_t t,
                                        zend_bool exclusive TSRMLS_DC);

/*
 * apc_cache_store creates key, entry and context in which to make an insertion of val into the specified cache
 */
PHP_APCU_API zend_bool apc_cache_store(apc_cache_t* cache,
                                       char *strkey,
                                       zend_uint keylen,
                                       const zval *val,
                                       const zend_uint ttl,
                                       const zend_bool exclusive TSRMLS_DC);
/*
* apc_cache_update updates an entry in place, this is used for rfc1867 and inc/dec/cas
*/
PHP_APCU_API zend_bool apc_cache_update(apc_cache_t* cache,
                                        char *strkey,
                                        zend_uint keylen,
                                        apc_cache_updater_t updater,
                                        void* data TSRMLS_DC);

/*
 * apc_cache_find searches for a cache entry by its hashed identifier,
 * and returns a pointer to the entry if found, NULL otherwise.
 *
 */
PHP_APCU_API apc_cache_entry_t* apc_cache_find(apc_cache_t* cache,
                                               char* strkey,
                                               zend_uint keylen,
                                               time_t t TSRMLS_DC);

/*
 * apc_cache_fetch fetches an entry from the cache directly into dst
 *
 */
PHP_APCU_API zend_bool apc_cache_fetch(apc_cache_t* cache,
                                       char* strkey,
                                       zend_uint keylen,
                                       time_t t,
                                       zval **dst TSRMLS_DC);

/*
 * apc_cache_exists searches for a cache entry by its hashed identifier,
 * and returns a pointer to the entry if found, NULL otherwise.  This is a
 * quick non-locking version of apc_cache_find that does not modify the
 * shared memory segment in any way.
 *
 */
PHP_APCU_API apc_cache_entry_t* apc_cache_exists(apc_cache_t* cache,
                                                 char* strkey,
                                                 zend_uint keylen,
                                                 time_t t TSRMLS_DC);

/*
 * apc_cache_delete and apc_cache_delete finds an entry in the cache and deletes it.
 */
PHP_APCU_API zend_bool apc_cache_delete(apc_cache_t* cache,
                                        char *strkey,
                                        zend_uint keylen TSRMLS_DC);

/* apc_cach_fetch_zval takes a zval in the cache and reconstructs a runtime
 * zval from it.
 *
 */
PHP_APCU_API zval* apc_cache_fetch_zval(apc_context_t* ctxt,
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
PHP_APCU_API void apc_cache_release(apc_cache_t* cache,
                                    apc_cache_entry_t* entry TSRMLS_DC);

/*
* apc_cache_make_key creates an apc_cache_key_t from an identifier, it's length and the current time
*/
PHP_APCU_API zend_bool apc_cache_make_key(apc_cache_key_t* key,
                                          char* str,
                                          zend_ulong len TSRMLS_DC);

/*
 * apc_cache_make_entry creates an apc_cache_entry_t given a zval, context and ttl
 */
PHP_APCU_API apc_cache_entry_t* apc_cache_make_entry(apc_context_t* ctxt,
                                                     apc_cache_key_t* key,
                                                     const zval *val,
                                                     const zend_uint ttl TSRMLS_DC);

/*
 fetches information about the cache provided for userland status functions
*/
PHP_APCU_API zval* apc_cache_info(apc_cache_t* cache,
                                  zend_bool limited TSRMLS_DC);
                                  
/*
 fetches information about the key provided
*/
PHP_APCU_API zval* apc_cache_stat(apc_cache_t* cache,
                                  char *strkey,
                                  zend_uint keylen TSRMLS_DC);

/*
* apc_cache_busy returns true while the cache is busy
*
* a cache is considered busy when any of the following occur:
*  a) the cache becomes busy when the allocator beneath it is running out of resources
*  b) a clear of the cache was requested
*  c) garbage collection is in progress
*
* Note: garbage collection can be invoked by the SMA and is invoked on insert
*/
PHP_APCU_API zend_bool apc_cache_busy(apc_cache_t* cache TSRMLS_DC);

/*
* apc_cache_defense: guard against slamming a key
*  will return true if the following conditions are met:
*	the key provided has a matching hash and length to the last key inserted into cache
*   the last key has a different owner
* in ZTS mode, TSRM determines owner
* in non-ZTS mode, PID determines owner
* Note: this function sets the owner of key during execution
*/
PHP_APCU_API zend_bool apc_cache_defense(apc_cache_t* cache,
                                         apc_cache_key_t* key TSRMLS_DC);

/*
* apc_cache_serializer
* sets the serializer for a cache, and by proxy contexts created for the cache
* Note: this avoids race conditions between third party serializers and APCu
*/
PHP_APCU_API void apc_cache_serializer(apc_cache_t* cache, const char* name TSRMLS_DC);

/*
* The remaining functions allow a third party to reimplement expunge
* 
* Look at the source of apc_cache_default_expunge for what is expected of this function
*
* The default behaviour of expunge is explained below, should no combination of those options
* be suitable, you will need to reimplement apc_cache_default_expunge and pass it to your
* call to apc_sma_api_impl, this will replace the default functionality.
* The functions below you can use during your own implementation of expunge to gain more
* control over how the expunge process works ...
* 
* Note: beware of locking (copy it exactly), setting states is also important
*/

/* {{{ apc_cache_default_expunge
* Where smart is not set:
*  Where no ttl is set on cache:
*   1) Perform cleanup of stale entries
*   2) Expunge if available memory is less than sma->size/2
*  Where ttl is set on cache:
*   1) Perform cleanup of stale entries
*   2) If available memory if less than the size requested, run full expunge
*
* Where smart is set:
*  Where no ttl is set on cache:
*   1) Perform cleanup of stale entries
*   2) Expunge is available memory is less than size * smart
*  Where ttl is set on cache:
*   1) Perform cleanup of stale entries
*   2) If available memory if less than the size requested, run full expunge
*
* The TTL of an entry takes precedence over the TTL of a cache
*/
PHP_APCU_API void apc_cache_default_expunge(apc_cache_t* cache, size_t size TSRMLS_DC);

/*
* The remaining functions are used during the APCu implementation of expunge
*/

/*
* apc_cache_real_expunge: trashes the whole cache
*
* Note: it is assumed you have a write lock on the header when you enter real expunge
*/
PHP_APCU_API void apc_cache_real_expunge(apc_cache_t* cache TSRMLS_DC);

/*
* apc_cache_gc: runs garbage collection on cache
*
* Note: it is assumed you have a write lock on the header when you enter gc
*/
PHP_APCU_API void apc_cache_gc(apc_cache_t* cache TSRMLS_DC);

/*
* apc_cache_remove_slot: removes slot
*
* if no references remain, the slot is free'd immediately
* if there are references remaining, the slot is trashed
*
* Note: it is assumed you have a write lock on the header when you remove slots
*/
PHP_APCU_API void apc_cache_remove_slot(apc_cache_t* cache, apc_cache_slot_t** slot TSRMLS_DC);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
