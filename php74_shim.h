#ifndef PHP74_SHIM_H
#define PHP74_SHIM_H

#if PHP_VERSION_ID < 70400

#define ZEND_TRY_ASSIGN_NULL(zv) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_NULL(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_NULL(zv) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_NULL(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_FALSE(zv) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_FALSE(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_FALSE(zv) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_FALSE(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_TRUE(zv) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_TRUE(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_TRUE(zv) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_TRUE(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_BOOL(zv, bval) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_BOOL(zv, bval); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_BOOL(zv, bval) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_BOOL(zv, bval); \
} while (0)

#define ZEND_TRY_ASSIGN_LONG(zv, lval) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_LONG(zv, lval); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_LONG(zv, lval) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_LONG(zv, lval); \
} while (0)

#define ZEND_TRY_ASSIGN_DOUBLE(zv, dval) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_DOUBLE(zv, dval); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_DOUBLE(zv, dval) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_DOUBLE(zv, dval); \
} while (0)

#define ZEND_TRY_ASSIGN_EMPTY_STRING(zv) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_EMPTY_STRING(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_EMPTY_STRING(zv) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_EMPTY_STRING(zv); \
} while (0)

#define ZEND_TRY_ASSIGN_STR(zv, str) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_STR(zv, str); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_STR(zv, str) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_STR(zv, str); \
} while (0)

#define ZEND_TRY_ASSIGN_NEW_STR(zv, str) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_NEW_STR(zv, str); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_NEW_STR(zv, str) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_NEW_STR(zv, str); \
} while (0)

#define ZEND_TRY_ASSIGN_STRING(zv, string) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_STRING(zv, string); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_STRING(zv, string) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_STRING(zv, string); \
} while (0)

#define ZEND_TRY_ASSIGN_STRINGL(zv, string, len) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_STRINGL(zv, string, len); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_STRINGL(zv, string, len) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_STRINGL(zv, string, len); \
} while (0)

#define ZEND_TRY_ASSIGN_ARR(zv, arr) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_ARR(zv, arr); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_ARR(zv, arr) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_ARR(zv, arr); \
} while (0)

#define ZEND_TRY_ASSIGN_RES(zv, res) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_RES(zv, res); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_RES(zv, res) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_RES(zv, res); \
} while (0)

#define ZEND_TRY_ASSIGN_VALUE(zv, other_zv) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_COPY_VALUE(zv, other_zv); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_VALUE(zv, other_zv) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_VALUE(zv, other_zv); \
} while (0)

#define ZEND_TRY_ASSIGN_COPY(zv, other_zv) do { \
	ZVAL_DEREF(zv); \
	zval_ptr_dtor(zv); \
	ZVAL_COPY(zv, other_zv); \
} while (0)

#define ZEND_TRY_ASSIGN_REF_COPY(zv, other_zv) do { \
	ZEND_ASSERT(Z_ISREF_P(zv)); \
	ZEND_TRY_ASSIGN_COPY(zv, other_zv); \
} while (0)

/* Initializes a reference to an empty array and returns dereferenced zval,
 * or NULL if the initialization failed. */
static zend_always_inline zval *zend_try_array_init(zval *zv) {
	ZVAL_DEREF(zv);
	array_init(zv);
	return zv;
}

static zend_always_inline zval *zend_try_array_init_size(zval *zv, uint32_t size) {
	ZVAL_DEREF(zv);
	array_init_size(zv, size);
	return zv;
}

#endif

#endif
