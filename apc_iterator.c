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
  | Authors: Brian Shire <shire@php.net>                                 |
  +----------------------------------------------------------------------+

 */

#include "php_apc.h"
#include "apc_iterator.h"
#include "apc_cache.h"
#include "apc_strings.h"

#include "ext/standard/md5.h"
#include "SAPI.h"
#include "zend_interfaces.h"

static zend_class_entry *apc_iterator_ce;
zend_object_handlers apc_iterator_object_handlers;

zend_class_entry* apc_iterator_get_ce(void) {
	return apc_iterator_ce;
}

/* {{{ apc_iterator_item */
static apc_iterator_item_t* apc_iterator_item_ctor(
		apc_iterator_t *iterator, apc_cache_entry_t *entry) {
	zval zv;
	HashTable *ht;
	apc_iterator_item_t *item = ecalloc(1, sizeof(apc_iterator_item_t));

	array_init(&item->value);
	ht = Z_ARRVAL(item->value);

	item->key = zend_string_dup(entry->key, 0);

	if (APC_ITER_TYPE & iterator->format) {
		ZVAL_STR_COPY(&zv, apc_str_user);
		zend_hash_add_new(ht, apc_str_type, &zv);
	}

	if (APC_ITER_KEY & iterator->format) {
		ZVAL_STR_COPY(&zv, item->key);
		zend_hash_add_new(ht, apc_str_key, &zv);
	}

	if (APC_ITER_VALUE & iterator->format) {
		ZVAL_UNDEF(&zv);
		apc_cache_entry_fetch_zval(apc_user_cache, entry, &zv);
		zend_hash_add_new(ht, apc_str_value, &zv);
	}

	if (APC_ITER_NUM_HITS & iterator->format) {
		ZVAL_LONG(&zv, entry->nhits);
		zend_hash_add_new(ht, apc_str_num_hits, &zv);
	}
	if (APC_ITER_MTIME & iterator->format) {
		ZVAL_LONG(&zv, entry->mtime);
		zend_hash_add_new(ht, apc_str_mtime, &zv);
	}
	if (APC_ITER_CTIME & iterator->format) {
		ZVAL_LONG(&zv, entry->ctime);
		zend_hash_add_new(ht, apc_str_creation_time, &zv);
	}
	if (APC_ITER_DTIME & iterator->format) {
		ZVAL_LONG(&zv, entry->dtime);
		zend_hash_add_new(ht, apc_str_deletion_time, &zv);
	}
	if (APC_ITER_ATIME & iterator->format) {
		ZVAL_LONG(&zv, entry->atime);
		zend_hash_add_new(ht, apc_str_access_time, &zv);
	}
	if (APC_ITER_REFCOUNT & iterator->format) {
		ZVAL_LONG(&zv, entry->ref_count);
		zend_hash_add_new(ht, apc_str_ref_count, &zv);
	}
	if (APC_ITER_MEM_SIZE & iterator->format) {
		ZVAL_LONG(&zv, entry->mem_size);
		zend_hash_add_new(ht, apc_str_mem_size, &zv);
	}
	if (APC_ITER_TTL & iterator->format) {
		ZVAL_LONG(&zv, entry->ttl);
		zend_hash_add_new(ht, apc_str_ttl, &zv);
	}

	return item;
}
/* }}} */

/* {{{ apc_iterator_item_dtor */
static void apc_iterator_item_dtor(apc_iterator_item_t *item) {
	zend_string_release(item->key);
	zval_ptr_dtor(&item->value);
	efree(item);
}
/* }}} */

/* {{{ acp_iterator_free */
static void apc_iterator_free(zend_object *object) {
	apc_iterator_t *iterator = apc_iterator_fetch_from(object);

	if (iterator->initialized == 0) {
		zend_object_std_dtor(object);
		return;
	}

	while (apc_stack_size(iterator->stack) > 0) {
		apc_iterator_item_dtor(apc_stack_pop(iterator->stack));
	}

	apc_stack_destroy(iterator->stack);

	if (iterator->regex) {
		zend_string_release(iterator->regex);
#if PHP_VERSION_ID >= 70300
		pcre2_match_data_free(iterator->re_match_data);
#endif
	}

	if (iterator->search_hash) {
		zend_hash_destroy(iterator->search_hash);
		efree(iterator->search_hash);
	}
	iterator->initialized = 0;

	zend_object_std_dtor(object);
}
/* }}} */

/* {{{ apc_iterator_create */
zend_object* apc_iterator_create(zend_class_entry *ce) {
	apc_iterator_t *iterator =
		(apc_iterator_t*) emalloc(sizeof(apc_iterator_t) + zend_object_properties_size(ce));

	zend_object_std_init(&iterator->obj, ce);
	object_properties_init(&iterator->obj, ce);

	iterator->initialized = 0;
	iterator->stack = NULL;
	iterator->regex = NULL;
	iterator->search_hash = NULL;
	iterator->obj.handlers = &apc_iterator_object_handlers;

	return &iterator->obj;
}
/* }}} */

/* {{{ apc_iterator_search_match
 *       Verify if the key matches our search parameters
 */
static int apc_iterator_search_match(apc_iterator_t *iterator, apc_cache_entry_t *entry) {
	int rval = 1;

	if (iterator->regex) {
#if PHP_VERSION_ID >= 70300
		rval = pcre2_match(
			php_pcre_pce_re(iterator->pce),
			(PCRE2_SPTR) ZSTR_VAL(entry->key), ZSTR_LEN(entry->key),
			0, 0, iterator->re_match_data, php_pcre_mctx()) >= 0;
#else
		rval = pcre_exec(
			iterator->pce->re, iterator->pce->extra,
			ZSTR_VAL(entry->key), ZSTR_LEN(entry->key),
			0, 0, NULL, 0) >= 0;
#endif
	}

	if (iterator->search_hash) {
		rval = zend_hash_exists(iterator->search_hash, entry->key);
	}

	return rval;
}
/* }}} */

/* {{{ apc_iterator_check_expiry */
static int apc_iterator_check_expiry(apc_cache_t* cache, apc_cache_entry_t *entry, time_t t)
{
	if (entry->ttl) {
		if ((time_t) (entry->ctime + entry->ttl) < t) {
			return 0;
		}
	}

	return 1;
}
/* }}} */

/* {{{ apc_iterator_fetch_active */
static int apc_iterator_fetch_active(apc_iterator_t *iterator) {
	int count = 0;
	apc_iterator_item_t *item;
	time_t t = apc_time();

	while (apc_stack_size(iterator->stack) > 0) {
		apc_iterator_item_dtor(apc_stack_pop(iterator->stack));
	}

	APC_RLOCK(apc_user_cache->header);
	php_apc_try {
		while (count <= iterator->chunk_size && iterator->slot_idx < apc_user_cache->nslots) {
			apc_cache_entry_t *entry = apc_user_cache->slots[iterator->slot_idx];
			while (entry) {
				if (apc_iterator_check_expiry(apc_user_cache, entry, t)) {
					if (apc_iterator_search_match(iterator, entry)) {
						count++;
						item = apc_iterator_item_ctor(iterator, entry);
						if (item) {
							apc_stack_push(iterator->stack, item);
						}
					}
				}
				entry = entry->next;
			}
			iterator->slot_idx++;
		}
	} php_apc_finally {
		iterator->stack_idx = 0;
		APC_RUNLOCK(apc_user_cache->header)
	} php_apc_end_try();

	return count;
}
/* }}} */

/* {{{ apc_iterator_fetch_deleted */
static int apc_iterator_fetch_deleted(apc_iterator_t *iterator) {
	int count = 0;
	apc_iterator_item_t *item;

	APC_RLOCK(apc_user_cache->header);
	php_apc_try {
		apc_cache_entry_t *entry = apc_user_cache->header->gc;
		while (entry && count <= iterator->slot_idx) {
			count++;
			entry = entry->next;
		}
		count = 0;
		while (entry && count < iterator->chunk_size) {
			if (apc_iterator_search_match(iterator, entry)) {
				count++;
				item = apc_iterator_item_ctor(iterator, entry);
				if (item) {
					apc_stack_push(iterator->stack, item);
				}
			}
			entry = entry->next;
		}
	} php_apc_finally {
		iterator->slot_idx += count;
		iterator->stack_idx = 0;
		APC_RUNLOCK(apc_user_cache->header);
	} php_apc_end_try();

	return count;
}
/* }}} */

/* {{{ apc_iterator_totals */
static void apc_iterator_totals(apc_iterator_t *iterator) {
	time_t t = apc_time();
	int i;

	APC_RLOCK(apc_user_cache->header);
	php_apc_try {
		for (i=0; i < apc_user_cache->nslots; i++) {
			apc_cache_entry_t *entry = apc_user_cache->slots[i];
			while (entry) {
				if (apc_iterator_check_expiry(apc_user_cache, entry, t)) {
					if (apc_iterator_search_match(iterator, entry)) {
						iterator->size += entry->mem_size;
						iterator->hits += entry->nhits;
						iterator->count++;
					}
				}
				entry = entry->next;
			}
		}
	} php_apc_finally {
		iterator->totals_flag = 1;
		APC_RUNLOCK(apc_user_cache->header);
	} php_apc_end_try();
}
/* }}} */

void apc_iterator_obj_init(apc_iterator_t *iterator, zval *search, zend_long format, zend_long chunk_size, zend_long list)
{
	if (!APCG(enabled)) {
		apc_error("APC must be enabled to use " APC_ITERATOR_NAME);
	}

	if (chunk_size < 0) {
		apc_error(APC_ITERATOR_NAME " chunk size must be 0 or greater");
		return;
	}

	if (format > APC_ITER_ALL) {
		apc_error(APC_ITERATOR_NAME " format is invalid");
		return;
	}

	if (list == APC_LIST_ACTIVE) {
		iterator->fetch = apc_iterator_fetch_active;
	} else if (list == APC_LIST_DELETED) {
		iterator->fetch = apc_iterator_fetch_deleted;
	} else {
		apc_warning(APC_ITERATOR_NAME " invalid list type");
		return;
	}

	iterator->slot_idx = 0;
	iterator->stack_idx = 0;
	iterator->key_idx = 0;
	iterator->chunk_size = chunk_size == 0 ? APC_DEFAULT_CHUNK_SIZE : chunk_size;
	iterator->stack = apc_stack_create(chunk_size);
	iterator->format = format;
	iterator->totals_flag = 0;
	iterator->count = 0;
	iterator->size = 0;
	iterator->hits = 0;
	iterator->regex = NULL;
	iterator->search_hash = NULL;
	if (search && Z_TYPE_P(search) == IS_STRING && Z_STRLEN_P(search)) {
		iterator->regex = zend_string_copy(Z_STR_P(search));
		iterator->pce = pcre_get_compiled_regex_cache(iterator->regex);

		if (!iterator->pce) {
			apc_error("Could not compile regular expression: %s", Z_STRVAL_P(search));
			zend_string_release(iterator->regex);
			iterator->regex = NULL;
		}

#if PHP_VERSION_ID >= 70300
		iterator->re_match_data = pcre2_match_data_create_from_pattern(
			php_pcre_pce_re(iterator->pce), php_pcre_gctx());
#endif
	} else if (search && Z_TYPE_P(search) == IS_ARRAY) {
		iterator->search_hash = apc_flip_hash(Z_ARRVAL_P(search));
	}
	iterator->initialized = 1;
}

/* {{{ proto object APCuIterator::__construct([ mixed search [, long format [, long chunk_size [, long list ]]]]) */
PHP_METHOD(apc_iterator, __construct) {
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());
	zend_long format = APC_ITER_ALL;
	zend_long chunk_size=0;
	zval *search = NULL;
	zend_long list = APC_LIST_ACTIVE;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|zlll", &search, &format, &chunk_size, &list) == FAILURE) {
		return;
	}

	apc_iterator_obj_init(iterator, search, format, chunk_size, list);
}
/* }}} */

/* {{{ proto APCuIterator::rewind() */
PHP_METHOD(apc_iterator, rewind) {
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0) {
		RETURN_FALSE;
	}

	iterator->slot_idx = 0;
	iterator->stack_idx = 0;
	iterator->key_idx = 0;
	iterator->fetch(iterator);
}
/* }}} */

/* {{{ proto boolean APCuIterator::valid() */
PHP_METHOD(apc_iterator, valid) {
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0) {
		RETURN_FALSE;
	}

	if (apc_stack_size(iterator->stack) == iterator->stack_idx) {
		iterator->fetch(iterator);
	}

	RETURN_BOOL(apc_stack_size(iterator->stack) == 0 ? 0 : 1);
}
/* }}} */

/* {{{ proto mixed APCuIterator::current() */
PHP_METHOD(apc_iterator, current) {
	apc_iterator_item_t *item;
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0) {
		RETURN_FALSE;
	}

	if (apc_stack_size(iterator->stack) == iterator->stack_idx) {
		if (iterator->fetch(iterator) == 0) {
			RETURN_FALSE;
		}
	}

	item = apc_stack_get(iterator->stack, iterator->stack_idx);
	ZVAL_COPY(return_value, &item->value);
}
/* }}} */

/* {{{ proto string APCuIterator::key() */
PHP_METHOD(apc_iterator, key) {
	apc_iterator_item_t *item;
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0 || apc_stack_size(iterator->stack) == 0) {
		RETURN_FALSE;
	}

	if (apc_stack_size(iterator->stack) == iterator->stack_idx) {
		if (iterator->fetch(iterator) == 0) {
			RETURN_FALSE;
		}
	}

	item = apc_stack_get(iterator->stack, iterator->stack_idx);

	if (item->key) {
		RETURN_STR_COPY(item->key);
	} else {
		RETURN_LONG(iterator->key_idx);
	}
}
/* }}} */

/* {{{ proto APCuIterator::next() */
PHP_METHOD(apc_iterator, next) {
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0 || apc_stack_size(iterator->stack) == 0) {
		RETURN_FALSE;
	}

	iterator->stack_idx++;
	iterator->key_idx++;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto long APCuIterator::getTotalHits() */
PHP_METHOD(apc_iterator, getTotalHits) {
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0) {
		RETURN_FALSE;
	}

	if (iterator->totals_flag == 0) {
		apc_iterator_totals(iterator);
	}

	RETURN_LONG(iterator->hits);
}
/* }}} */

/* {{{ proto long APCuIterator::getTotalSize() */
PHP_METHOD(apc_iterator, getTotalSize) {
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0) {
		RETURN_FALSE;
	}

	if (iterator->totals_flag == 0) {
		apc_iterator_totals(iterator);
	}

	RETURN_LONG(iterator->size);
}
/* }}} */

/* {{{ proto long APCuIterator::getTotalCount() */
PHP_METHOD(apc_iterator, getTotalCount) {
	apc_iterator_t *iterator = apc_iterator_fetch(getThis());

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (iterator->initialized == 0) {
		RETURN_FALSE;
	}

	if (iterator->totals_flag == 0) {
		apc_iterator_totals(iterator);
	}

	RETURN_LONG(iterator->count);
}
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_iterator___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, search)
	ZEND_ARG_INFO(0, format)
	ZEND_ARG_INFO(0, chunk_size)
	ZEND_ARG_INFO(0, list)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_iterator_void, 0, 0, 0)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ apc_iterator_functions */
static zend_function_entry apc_iterator_functions[] = {
	PHP_ME(apc_iterator, __construct, arginfo_apc_iterator___construct, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, rewind, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, current, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, key, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, next, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, valid, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, getTotalHits, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, getTotalSize, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_ME(apc_iterator, getTotalCount, arginfo_apc_iterator_void, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ apc_iterator_init */
int apc_iterator_init(int module_number) {
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, APC_ITERATOR_NAME, apc_iterator_functions);
	apc_iterator_ce = zend_register_internal_class(&ce);
	apc_iterator_ce->create_object = apc_iterator_create;
	zend_class_implements(apc_iterator_ce, 1, zend_ce_iterator);

	REGISTER_LONG_CONSTANT("APC_LIST_ACTIVE", APC_LIST_ACTIVE, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_LIST_DELETED", APC_LIST_DELETED, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_TYPE", APC_ITER_TYPE, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_KEY", APC_ITER_KEY, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_VALUE", APC_ITER_VALUE, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_NUM_HITS", APC_ITER_NUM_HITS, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_MTIME", APC_ITER_MTIME, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_CTIME", APC_ITER_CTIME, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_DTIME", APC_ITER_DTIME, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_ATIME", APC_ITER_ATIME, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_REFCOUNT", APC_ITER_REFCOUNT, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_MEM_SIZE", APC_ITER_MEM_SIZE, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_TTL", APC_ITER_TTL, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_NONE", APC_ITER_NONE, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("APC_ITER_ALL", APC_ITER_ALL, CONST_PERSISTENT | CONST_CS);

	memcpy(&apc_iterator_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	apc_iterator_object_handlers.clone_obj = NULL;
	apc_iterator_object_handlers.free_obj = apc_iterator_free;
	apc_iterator_object_handlers.offset = XtOffsetOf(apc_iterator_t, obj);

	return SUCCESS;
}
/* }}} */

int apc_iterator_shutdown(int module_number) {
	return SUCCESS;
}

/* {{{ apc_iterator_delete */
int apc_iterator_delete(zval *zobj) {
	apc_iterator_t *iterator;
	zend_class_entry *ce = Z_OBJCE_P(zobj);
	apc_iterator_item_t *item;

	if (!ce || !instanceof_function(ce, apc_iterator_ce)) {
		apc_error("apc_delete object argument must be instance of " APC_ITERATOR_NAME ".");
		return 0;
	}
	iterator = apc_iterator_fetch(zobj);

	if (iterator->initialized == 0) {
		return 0;
	}

	while (iterator->fetch(iterator)) {
		while (iterator->stack_idx < apc_stack_size(iterator->stack)) {
			item = apc_stack_get(iterator->stack, iterator->stack_idx++);
			apc_cache_delete(
				apc_user_cache, item->key);
		}
	}

	return 1;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: noexpandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4 sts=4
 */
