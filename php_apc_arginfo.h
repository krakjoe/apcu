/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 5282d856fd334278d5799581ad251977a3c6b18e */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_apcu_clear_cache, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_apcu_cache_info, 0, 0, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, limited, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_apcu_key_info, 0, 1, IS_ARRAY, 1)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_apcu_sma_info arginfo_apcu_cache_info

#define arginfo_apcu_enabled arginfo_apcu_clear_cache

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_apcu_store, 0, 1, MAY_BE_ARRAY|MAY_BE_BOOL)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, ttl, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

#define arginfo_apcu_add arginfo_apcu_store

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_apcu_inc, 0, 1, MAY_BE_LONG|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, step, IS_LONG, 0, "1")
	ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, success, "null")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, ttl, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

#define arginfo_apcu_dec arginfo_apcu_inc

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_apcu_cas, 0, 3, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, old, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, new, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_apcu_fetch, 0, 1, IS_MIXED, 0)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, success, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_apcu_exists, 0, 1, MAY_BE_ARRAY|MAY_BE_BOOL)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

#define arginfo_apcu_delete arginfo_apcu_exists

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_apcu_entry, 0, 2, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, ttl, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

#if defined(APC_DEBUG)
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_apcu_inc_request_time, 0, 0, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, by, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()
#endif


PHP_APCU_API ZEND_FUNCTION(apcu_clear_cache);
PHP_APCU_API ZEND_FUNCTION(apcu_cache_info);
PHP_APCU_API ZEND_FUNCTION(apcu_key_info);
PHP_APCU_API ZEND_FUNCTION(apcu_sma_info);
PHP_APCU_API ZEND_FUNCTION(apcu_enabled);
PHP_APCU_API ZEND_FUNCTION(apcu_store);
PHP_APCU_API ZEND_FUNCTION(apcu_add);
PHP_APCU_API ZEND_FUNCTION(apcu_inc);
PHP_APCU_API ZEND_FUNCTION(apcu_dec);
PHP_APCU_API ZEND_FUNCTION(apcu_cas);
PHP_APCU_API ZEND_FUNCTION(apcu_fetch);
PHP_APCU_API ZEND_FUNCTION(apcu_exists);
PHP_APCU_API ZEND_FUNCTION(apcu_delete);
PHP_APCU_API ZEND_FUNCTION(apcu_entry);
#if defined(APC_DEBUG)
PHP_APCU_API ZEND_FUNCTION(apcu_inc_request_time);
#endif


static const zend_function_entry ext_functions[] = {
	ZEND_FE(apcu_clear_cache, arginfo_apcu_clear_cache)
	ZEND_FE(apcu_cache_info, arginfo_apcu_cache_info)
	ZEND_FE(apcu_key_info, arginfo_apcu_key_info)
	ZEND_FE(apcu_sma_info, arginfo_apcu_sma_info)
	ZEND_FE(apcu_enabled, arginfo_apcu_enabled)
	ZEND_FE(apcu_store, arginfo_apcu_store)
	ZEND_FE(apcu_add, arginfo_apcu_add)
	ZEND_FE(apcu_inc, arginfo_apcu_inc)
	ZEND_FE(apcu_dec, arginfo_apcu_dec)
	ZEND_FE(apcu_cas, arginfo_apcu_cas)
	ZEND_FE(apcu_fetch, arginfo_apcu_fetch)
	ZEND_FE(apcu_exists, arginfo_apcu_exists)
	ZEND_FE(apcu_delete, arginfo_apcu_delete)
	ZEND_FE(apcu_entry, arginfo_apcu_entry)
#if defined(APC_DEBUG)
	ZEND_FE(apcu_inc_request_time, arginfo_apcu_inc_request_time)
#endif
	ZEND_FE_END
};
