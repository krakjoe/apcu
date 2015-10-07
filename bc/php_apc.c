/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2015 The PHP Group     	                             |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: krakjoe													 |
  +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "zend.h"
#include "zend_API.h"
#include "zend_compile.h"
#include "zend_hash.h"
#include "zend_extensions.h"

#include "php_apc.h"
#include "../php_apc.h"
#include "ext/standard/info.h"

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

/* {{{ PHP_FUNCTION declarations */
PHP_FUNCTION(apc_cache_info);
PHP_FUNCTION(apc_clear_cache);
/* }}} */

/* {{{ PHP_MINFO_FUNCTION(apc) */
static PHP_MINFO_FUNCTION(apc)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "APC Compatibility:", "Enabled");
    php_info_print_table_row(2, "Revision", "$Revision: 328290 $");
    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    php_info_print_table_end();
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION(apc) */
static PHP_RINIT_FUNCTION(apc)
{
#if defined(ZTS) && defined(COMPILE_DL_APC)
        ZEND_TSRMLS_CACHE_UPDATE();
#endif
    return SUCCESS;
}
/* }}} */

/* {{{ proto void apc_clear_cache(string cache) */
PHP_FUNCTION(apc_clear_cache) { 
	zend_string *ignored;
	zval proxy;

	if (zend_parse_paramters(ZEND_NUM_ARGS(), "S", &ignored) != SUCCESS) { 
		return;
	}
	
	ZVAL_STR(&proxy, 
		zend_string_init(ZEND_STRL("apcu_clear_cache"), 0));
	call_user_function(EG(function_table), NULL, &proxy, return_value, 0, NULL);
	zval_ptr_dtor(&proxy);
}
/* }}} */

/* {{{ proto array apc_cache_info(string cache [, bool limited = false]) */
PHP_FUNCTION(apc_cache_info) {
	zval *params[2] = {&EG(uninitialized_zval), &EG(uninitialized_zval)};
	zval  proxy;

	if (zend_parse_paramters(ZEND_NUM_ARGS(), "z|z", &params[0], &params[1]) != SUCCESS) {
		return;
	}

	ZVAL_STR(&proxy, 
		zend_string_init(ZEND_STRL("apcu_cache_info"), 0));
	call_user_function(EG(function_table), NULL, &proxy, return_value, 2, *params);
	zval_ptr_dtor(&proxy);
}
/* }}} */

/* {{{ apc_functions[] */
zend_function_entry apc_functions[] = {
    PHP_FE(apc_cache_info,         NULL)
    PHP_FE(apc_clear_cache,        NULL)
	PHP_FALIAS(apc_store, apcu_store, NULL)
	PHP_FALIAS(apc_fetch, apcu_fetch, NULL)
	PHP_FALIAS(apc_enabled, apcu_enabled, NULL)
	PHP_FALIAS(apc_delete, apcu_delete, NULL)
    PHP_FALIAS(apc_add, apcu_add,		NULL)
    PHP_FALIAS(apc_sma_info, apcu_sma_info, NULL)
    PHP_FALIAS(apc_inc, apcu_inc, NULL)
	PHP_FALIAS(apc_dec, apcu_dec, NULL)
	PHP_FALIAS(apc_cas, apcu_cas, NULL)
	PHP_FALIAS(apc_exists, apcu_exists, NULL)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ module definition structure */
zend_module_entry apc_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_APC_EXTNAME,
    apc_functions,
    NULL,
    NULL,
    PHP_RINIT(apc),
    NULL,
    PHP_MINFO(apc),
    PHP_APC_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_APC
ZEND_GET_MODULE(apc)
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE();
#endif
#endif
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
