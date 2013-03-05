#ifndef HAVE_APC_ASYNC
#define HAVE_APC_ASYNC

#ifndef HAVE_APC_GLOBALS_H
# include "apc_globals.h"
#endif

extern apc_cache_t *apc_user_cache;

#if !defined(_WIN32) && !defined(ZTS)

/* {{{ true global: apc_async_signal */
apc_async_signal_t apc_async_signal; /* }}} */

static inline apc_async_insert_dtor(void *data) {}

/* {{{ initialize the global async signal for APC */
void apc_async_signal_init(apc_async_signal_t *signal TSRMLS_DC) {
	if (!signal->ready) {
		apc_lock_create(
			&signal->lock TSRMLS_CC);
		{
			pthread_condattr_t attr;
			/** needs checking, I am sketching */
			pthread_condattr_init(&attr);
			pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
			pthread_cond_init(&signal->wait, &attr);
			pthread_condattr_destroy(&attr);
		}
		signal->ready = 1;
	}
} /* }}} */

/* {{{ destroy the async signal for APC */
void apc_async_signal_destroy(apc_async_signal_t *signal TSRMLS_DC) {
	if (signal->ready) {
		apc_lock_destroy(
			&signal->lock TSRMLS_CC);
		signal->ready = 0;
	}
} /* }}} */

apc_async_insert_t* apc_cache_make_async_insert(char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC) 
{
	apc_async_insert_t *insert = apc_sma_malloc(sizeof(apc_async_insert_t) TSRMLS_CC);
		
	if (!insert)
		return NULL;

	insert->cache = apc_user_cache;
	insert->ctime = apc_time();
	insert->ttl   = ttl;
	insert->exclusive = exclusive;	

	if (!apc_cache_make_key(&insert->key, strkey, strkey_len TSRMLS_CC) ||
		apc_cache_defense(insert->cache, &insert->key TSRMLS_CC)) {
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
	
	insert->entry = apc_cache_make_entry(val, &insert->ctx, insert->ttl TSRMLS_CC);	

	if (!insert->entry) {
		apc_pool_destroy(insert->ctx.pool TSRMLS_CC);
		apc_sma_free(insert TSRMLS_CC);
		
		return NULL;
	}

	return insert;
}
#endif

#endif
