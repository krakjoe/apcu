#ifndef APC_ASYNC_H
#define APC_ASYNC_H

#ifndef HAVE_APC_CACHE_H
# include "apc_cache.h"
#endif

#if !defined(_WIN32) && !defined(ZTS)
/* {{{ struct definition: apc_async_insert_t */
typedef struct apc_async_insert_t {
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

/* {{{ initialize the global async signal for APC */
extern void apc_async_signal_init(apc_async_signal_t *async TSRMLS_DC); /* }}} */

/* {{{ destroy the async signal for APC */
extern void apc_async_signal_destroy(apc_async_signal_t *async TSRMLS_DC); /* }}} */

/*
* apc_cache_make_async_insert creates an apc_async_insert_t to be inserted in a separate context
*/
extern apc_async_insert_t* apc_cache_make_async_insert(apc_async_signal_t *async, 
													   char *strkey, 
													   int strkey_len, 
													   const zval *val, 
													   const unsigned int ttl, 
													   const int exclusive TSRMLS_DC);
#endif

#endif

