/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_apcue.h"

/* If you declare any globals in php_apcue.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(apcue)
*/

/* True global resources - no need for thread safety here */
static int le_apcue;

/* Don't use staticness here, the SMA api needs access to this structure at expunge time */
apc_cache_t* apcue_cache = NULL;

/*
* Initializes your isolated sma, using your cache and the default expunge function
*/
apc_sma_api_impl(apcue_sma, &apcue_cache, apc_cache_default_expunge);

/* {{{ apcue_functions[]
 *
 * Every user visible function must have an entry in apcue_functions[].
 */
const zend_function_entry apcue_functions[] = {
	PHP_FE(apcue_get,	NULL)		/* For testing, remove later. */
	PHP_FE(apcue_set,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in apcue_functions[] */
};
/* }}} */

/* {{{ apcue_module_entry
 */
zend_module_entry apcue_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"apcue",
	apcue_functions,
	PHP_MINIT(apcue),
	PHP_MSHUTDOWN(apcue),
	PHP_RINIT(apcue),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(apcue),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(apcue),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_APCUE
ZEND_GET_MODULE(apcue)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("apcue.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_apcue_globals, apcue_globals)
    STD_PHP_INI_ENTRY("apcue.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_apcue_globals, apcue_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_apcue_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_apcue_init_globals(zend_apcue_globals *apcue_globals)
{
	apcue_globals->global_value = 0;
	apcue_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(apcue)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/

	/* initialize sma, use a sensible amount of memory !! */
	apcue_sma.init(1, 1024*1024*32, NULL TSRMLS_CC);
	
	/* create cache in shared memory */ 
	apcue_cache = apc_cache_create(
		&apcue_sma,
		10, 0L, 0L, 0L TSRMLS_CC
	);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(apcue)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	
	/* destroy cache */
	apc_cache_destroy(
		apcue_cache TSRMLS_CC);
	
	/* cleanup sma */
	apcue_sma.cleanup(TSRMLS_CC);

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(apcue)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(apcue)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(apcue)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "apcue support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* {{{ proto string apcue_get(string key)
   Return an entry from the cache */
PHP_FUNCTION(apcue_get) 
{
	char*      key = NULL;
	zend_uint  klen = 0L;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &klen) == FAILURE) {
		return;
	}

	{
		/* perform lookup */
		if (!apc_cache_fetch(apcue_cache, key, klen, time(0), &return_value TSRMLS_CC)) {
			/* entry not found */
		}
	}
}
/* }}} */

/* {{{ proto string apcue_set(string key, mixed value [, ttl])
   Set an entry in the cache */
PHP_FUNCTION(apcue_set)
{
	char*      key = NULL;
	zend_uint  klen = 0L;
    long       ttl = 0L;
    zval*      pzval = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &key, &klen, &pzval, &ttl) == FAILURE) {
		return;
	}

	/* perform store */
	ZVAL_BOOL(return_value, apc_cache_store(apcue_cache, key, klen, pzval, ttl, 0 TSRMLS_CC));
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
