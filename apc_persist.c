/*
  +----------------------------------------------------------------------+
  | APCu                                                                 |
  +----------------------------------------------------------------------+
  | Copyright (c) 2018 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Nikita Popov <nikic@php.net>                                 |
  +----------------------------------------------------------------------+
 */

#include "apc.h"
#include "apc_cache.h"

#if PHP_VERSION_ID < 70300
# define GC_SET_REFCOUNT(ref, rc) (GC_REFCOUNT(ref) = (rc))
# define GC_ADDREF(ref) GC_REFCOUNT(ref)++
# define GC_SET_PERSISTENT_TYPE(ref, type) (GC_TYPE_INFO(ref) = type)
#else
# define GC_SET_PERSISTENT_TYPE(ref, type) \
	(GC_TYPE_INFO(ref) = type | (GC_PERSISTENT << GC_FLAGS_SHIFT))
#endif

typedef struct _apc_persist_context_t {
	/* Serializer to use */
	apc_serializer_t *serializer;
	/* Computed size of the needed SMA allocation */
	size_t size;
	/* Whether or not we may have to memoize refcounted addresses */
	zend_bool memoization_needed;
	/* Whether to force serialization of the top-level value */
	zend_bool force_serialization;
	/* Serialized object/array string, in case there can only be one */
	unsigned char *serialized_str;
	size_t serialized_str_len;
	/* Whole SMA allocation */
	char *alloc;
	/* Current position in allocation */
	char *alloc_cur;
	/* HashTable storing refcounteds for which the size has already been counted. */
	HashTable already_counted;
	/* HashTable storing already allocated refcounteds. The entire zvals are stored. */
	HashTable already_allocated;
} apc_persist_context_t;

#define ADD_SIZE(sz) ctxt->size += ZEND_MM_ALIGNED_SIZE(sz)
#define ADD_SIZE_STR(len) ADD_SIZE(_ZSTR_STRUCT_SIZE(len))

#define ALLOC(sz) apc_persist_alloc(ctxt, sz)
#define COPY(val, sz) apc_persist_alloc_copy(ctxt, val, sz)

static zend_bool apc_persist_calc_zval(
		apc_persist_context_t *ctxt, const zval *zv, zend_bool top_level);
static void apc_persist_copy_zval(apc_persist_context_t *ctxt, zval *zv);

void apc_persist_init_context(apc_persist_context_t *ctxt, apc_serializer_t *serializer) {
	ctxt->serializer = serializer;
	ctxt->size = 0;
	ctxt->memoization_needed = 0;
	ctxt->force_serialization = 0;
	ctxt->serialized_str = NULL;
	ctxt->serialized_str_len = 0;
	ctxt->alloc = NULL;
	ctxt->alloc_cur = NULL;
}

void apc_persist_destroy_context(apc_persist_context_t *ctxt) {
	if (ctxt->memoization_needed) {
		zend_hash_destroy(&ctxt->already_counted);
		zend_hash_destroy(&ctxt->already_allocated);
	}
	if (ctxt->serialized_str) {
		efree(ctxt->serialized_str);
	}
}

static zend_bool apc_persist_calc_memoize(apc_persist_context_t *ctxt, void *ptr) {
	zval tmp;
	if (!ctxt->memoization_needed) {
		return 0;
	}

	if (zend_hash_index_exists(&ctxt->already_counted, (uintptr_t) ptr)) {
		return 1;
	}

	ZVAL_NULL(&tmp);
	zend_hash_index_add_new(&ctxt->already_counted, (uintptr_t) ptr, &tmp);
	return 0;
}

static zend_bool apc_persist_calc_ht(apc_persist_context_t *ctxt, const HashTable *ht) {
	uint32_t idx;

	ADD_SIZE(sizeof(HashTable));
	if (ht->nNumUsed == 0) {
		return 1;
	}

	/* TODO Too sparse hashtables could be compacted here */
	ADD_SIZE(HT_USED_SIZE(ht));
	for (idx = 0; idx < ht->nNumUsed; idx++) {
		Bucket *p = ht->arData + idx;
		if (Z_TYPE(p->val) == IS_UNDEF) continue;

		/* This can only happen if $GLOBALS is placed in the cache.
		 * Don't bother with this edge-case, fall back to serialization. */
		if (Z_TYPE(p->val) == IS_INDIRECT) {
			ctxt->force_serialization = 1;
			return 0;
		}

		/* TODO These strings can be reused
		if (p->key && !apc_persist_calc_is_handled(ctxt, (zend_refcounted *) p->key)) {
			ADD_SIZE_STR(ZSTR_LEN(p->key));
		}*/
		if (p->key) {
			ADD_SIZE_STR(ZSTR_LEN(p->key));
		}
		if (!apc_persist_calc_zval(ctxt, &p->val, 0)) {
			return 0;
		}
	}

	return 1;
}

static zend_bool apc_persist_calc_serialize(apc_persist_context_t *ctxt, const zval *zv) {
	unsigned char *buf = NULL;
	size_t buf_len = 0;

	apc_serialize_t serialize = APC_SERIALIZER_NAME(php);
	void *config = NULL;
	if (ctxt->serializer) {
		serialize = ctxt->serializer->serialize;
		config = ctxt->serializer->config;
	}

	if (!serialize(&buf, &buf_len, zv, config)) {
		return 0;
	}

	/* We only ever serialize the top-level value, memoization cannot be needed */
	ZEND_ASSERT(!ctxt->memoization_needed);
	ctxt->serialized_str = buf;
	ctxt->serialized_str_len = buf_len;

	ADD_SIZE_STR(buf_len);
	return 1;
}

static zend_bool apc_persist_calc_zval(
		apc_persist_context_t *ctxt, const zval *zv, zend_bool top_level) {
	if (Z_TYPE_P(zv) < IS_STRING) {
		/* No data apart from the zval itself */
		return 1;
	}

	if (ctxt->force_serialization) {
		return apc_persist_calc_serialize(ctxt, zv);
	}

	if (apc_persist_calc_memoize(ctxt, Z_COUNTED_P(zv))) {
		return 1;
	}

	switch (Z_TYPE_P(zv)) {
		case IS_STRING:
			ADD_SIZE_STR(Z_STRLEN_P(zv));
			return 1;
		case IS_ARRAY:
			if (!ctxt->serializer) {
				return apc_persist_calc_ht(ctxt, Z_ARRVAL_P(zv));
			}
			/* break missing intentionally */
		case IS_OBJECT:
			if (!top_level) {
				ctxt->force_serialization = 1;
				return 0;
			}
			return apc_persist_calc_serialize(ctxt, zv);
		case IS_REFERENCE:
			ADD_SIZE(sizeof(zend_reference));
			return apc_persist_calc_zval(ctxt, Z_REFVAL_P(zv), 0);
		case IS_RESOURCE:
			apc_warning("Cannot store resources in apcu cache");
			return 0;
		default:
			ZEND_ASSERT(0);
			return 0;
	}
}

static zend_bool apc_persist_calc(apc_persist_context_t *ctxt, const apc_cache_entry_t *entry) {
	ADD_SIZE(sizeof(apc_cache_entry_t));
	ADD_SIZE_STR(ZSTR_LEN(entry->key));
	return apc_persist_calc_zval(ctxt, &entry->val, 1);
}

static inline void *apc_persist_get_already_allocated(apc_persist_context_t *ctxt, void *ptr) {
	if (ctxt->memoization_needed) {
		return zend_hash_index_find_ptr(&ctxt->already_allocated, (uintptr_t) ptr);
	}
	return NULL;
}

static inline void apc_persist_add_already_allocated(
		apc_persist_context_t *ctxt, const void *old_ptr, void *new_ptr) {
	if (ctxt->memoization_needed) {
		zend_hash_index_add_new_ptr(&ctxt->already_allocated, (uintptr_t) old_ptr, new_ptr);
	}
}

static inline void *apc_persist_alloc(apc_persist_context_t *ctxt, size_t size) {
	void *ptr = ctxt->alloc_cur;
	ctxt->alloc_cur += ZEND_MM_ALIGNED_SIZE(size);
	ZEND_ASSERT(ctxt->alloc_cur <= ctxt->alloc + ctxt->size);
	return ptr;
}

static inline void *apc_persist_alloc_copy(
		apc_persist_context_t *ctxt, const void *val, size_t size) {
	void *ptr = apc_persist_alloc(ctxt, size);
	memcpy(ptr, val, size);
	return ptr;
}

static zend_string *apc_persist_copy_cstr(
		apc_persist_context_t *ctxt, const char *orig_buf, size_t buf_len) {
	zend_string *str = ALLOC(_ZSTR_STRUCT_SIZE(buf_len));

	GC_SET_REFCOUNT(str, 1);
	GC_SET_PERSISTENT_TYPE(str, IS_STRING);

	ZSTR_H(str) = 0;
	ZSTR_LEN(str) = buf_len;
	memcpy(ZSTR_VAL(str), orig_buf, buf_len);
	ZSTR_VAL(str)[buf_len] = '\0';
	zend_string_hash_val(str);

	return str;
}

static zend_string *apc_persist_copy_zstr(
		apc_persist_context_t *ctxt, const zend_string *orig_str) {
	zend_string *str = apc_persist_copy_cstr(ctxt, ZSTR_VAL(orig_str), ZSTR_LEN(orig_str));
	apc_persist_add_already_allocated(ctxt, orig_str, str);
	return str;
}

static zend_reference *apc_persist_copy_ref(
		apc_persist_context_t *ctxt, const zend_reference *orig_ref) {
	zend_reference *ref = ALLOC(sizeof(zend_reference));
	apc_persist_add_already_allocated(ctxt, orig_ref, ref);

	GC_SET_REFCOUNT(ref, 1);
	GC_SET_PERSISTENT_TYPE(ref, IS_REFERENCE);

	ZVAL_COPY_VALUE(&ref->val, &orig_ref->val);
	apc_persist_copy_zval(ctxt, &ref->val);

	return ref;
}

static const uint32_t uninitialized_bucket[-HT_MIN_MASK] = {HT_INVALID_IDX, HT_INVALID_IDX};

static zend_array *apc_persist_copy_ht(apc_persist_context_t *ctxt, const HashTable *orig_ht) {
	HashTable *ht = COPY(orig_ht, sizeof(HashTable));
	uint32_t idx;
	apc_persist_add_already_allocated(ctxt, orig_ht, ht);

	GC_SET_REFCOUNT(ht, 1);
	GC_SET_PERSISTENT_TYPE(ht, IS_ARRAY);

	/* Immutable arrays from opcache may lack a dtor and the apply protection flag. */
	ht->pDestructor = ZVAL_PTR_DTOR;
#if PHP_VERSION_ID < 70300
	ht->u.flags |= HASH_FLAG_APPLY_PROTECTION;
#endif

	ht->u.flags |= HASH_FLAG_STATIC_KEYS;
	if (ht->nNumUsed == 0) {
		ht->u.flags &= ~(HASH_FLAG_INITIALIZED|HASH_FLAG_PACKED);
		ht->nNextFreeElement = 0;
		ht->nTableMask = HT_MIN_MASK;
		HT_SET_DATA_ADDR(ht, &uninitialized_bucket);
		return ht;
	}

	ht->nNextFreeElement = 0;
	ht->nInternalPointer = HT_INVALID_IDX;
	HT_SET_DATA_ADDR(ht, COPY(HT_GET_DATA_ADDR(ht), HT_USED_SIZE(ht)));
	for (idx = 0; idx < ht->nNumUsed; idx++) {
		Bucket *p = ht->arData + idx;
		if (Z_TYPE(p->val) == IS_UNDEF) continue;

		if (ht->nInternalPointer == HT_INVALID_IDX) {
			ht->nInternalPointer = idx;
		}

		if (p->key) {
			p->key = apc_persist_copy_zstr(ctxt, p->key);
			ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;
		} else if ((zend_long) p->h >= (zend_long) ht->nNextFreeElement) {
			ht->nNextFreeElement = p->h + 1;
		}

		apc_persist_copy_zval(ctxt, &p->val);
	}

	return ht;
}

static void apc_persist_copy_serialize(
		apc_persist_context_t *ctxt, zval *zv) {
	zend_string *str;
	zend_uchar orig_type = Z_TYPE_P(zv);
	ZEND_ASSERT(orig_type == IS_ARRAY || orig_type == IS_OBJECT);

	ZEND_ASSERT(!ctxt->memoization_needed);
	ZEND_ASSERT(ctxt->serialized_str);
	str = apc_persist_copy_cstr(ctxt,
		(char *) ctxt->serialized_str, ctxt->serialized_str_len);

	/* Store string, but give it the type of an array/object */
	ZVAL_STR(zv, str);
	Z_TYPE_INFO_P(zv) = orig_type;
}

static void apc_persist_copy_zval(apc_persist_context_t *ctxt, zval *zv) {
	void *ptr;

	if (Z_TYPE_P(zv) < IS_STRING) {
		/* No data apart from the zval itself */
		return;
	}

	if (ctxt->force_serialization) {
		apc_persist_copy_serialize(ctxt, zv);
		return;
	}

	ptr = apc_persist_get_already_allocated(ctxt, Z_COUNTED_P(zv));
	switch (Z_TYPE_P(zv)) {
		case IS_STRING:
			if (!ptr) ptr = apc_persist_copy_zstr(ctxt, Z_STR_P(zv));
			ZVAL_STR(zv, ptr);
			return;
		case IS_ARRAY:
			if (!ctxt->serializer) {
				if (!ptr) ptr = apc_persist_copy_ht(ctxt, Z_ARRVAL_P(zv));
				ZVAL_ARR(zv, ptr);
				return;
			}
			/* break missing intentionally */
		case IS_OBJECT:
			apc_persist_copy_serialize(ctxt, zv);
			return;
		case IS_REFERENCE:
			if (!ptr) ptr = apc_persist_copy_ref(ctxt, Z_REF_P(zv));
			ZVAL_REF(zv, ptr);
			return;
		default:
			ZEND_ASSERT(0);
			return;
	}
}

static apc_cache_entry_t *apc_persist_copy(
		apc_persist_context_t *ctxt, const apc_cache_entry_t *orig_entry) {
	apc_cache_entry_t *entry = COPY(orig_entry, sizeof(apc_cache_entry_t));
	entry->key = apc_persist_copy_zstr(ctxt, entry->key);
	apc_persist_copy_zval(ctxt, &entry->val);
	return entry;
}

apc_cache_entry_t *apc_persist(
		apc_sma_t *sma, apc_serializer_t *serializer, const apc_cache_entry_t *orig_entry) {
	apc_persist_context_t ctxt;
	apc_cache_entry_t *entry;

	apc_persist_init_context(&ctxt, serializer);

	/* If we're serializing an array or reference using the default serializer, we will have
	 * to keep track of potentially repeated refcounted structures. */
	if (!serializer && (Z_TYPE(orig_entry->val) == IS_ARRAY ||
	                    Z_TYPE(orig_entry->val) == IS_REFERENCE)) {
		ctxt.memoization_needed = 1;
		zend_hash_init(&ctxt.already_counted, 0, NULL, ZVAL_PTR_DTOR, 0);
		zend_hash_init(&ctxt.already_allocated, 0, NULL, NULL, 0);
	}

	if (!apc_persist_calc(&ctxt, orig_entry)) {
		if (!ctxt.force_serialization) {
			apc_persist_destroy_context(&ctxt);
			return NULL;
		}

		/* Try again with forced serialization */
		apc_persist_destroy_context(&ctxt);
		apc_persist_init_context(&ctxt, serializer);
		ctxt.force_serialization = 1;
		if (!apc_persist_calc(&ctxt, orig_entry)) {
			apc_persist_destroy_context(&ctxt);
			return NULL;
		}
	}

	ctxt.alloc = ctxt.alloc_cur = sma->smalloc(ctxt.size);
	if (!ctxt.alloc) {
		apc_persist_destroy_context(&ctxt);
		return NULL;
	}

	entry = apc_persist_copy(&ctxt, orig_entry);
	ZEND_ASSERT(ctxt.alloc_cur == ctxt.alloc + ctxt.size);

	entry->mem_size = ctxt.size;

	apc_persist_destroy_context(&ctxt);
	return entry;
}
