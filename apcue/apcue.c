/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 2013 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Joe Watkins <joe.watkins@live.co.uk>                         |
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

/* Don't use staticness here, the sma needs access to this structure at expunge time */
apc_cache_t* apcue_cache = NULL;

/* {{{ override the expunge functionality of APCu */
static void apcue_cache_expunge(apc_cache_t* cache, size_t size TSRMLS_DC) {
	php_printf(
		"expunging apcue ...");

	/* just call the default expunge method, 
		reimplementation outside the scope of example */
	apc_cache_default_expunge(cache, size TSRMLS_CC);

} /* }}} */

/*
* Initializes your isolated sma, providing it a pointer to your cache and your expunge function
*
* Note: it does not matter that the cache is initialized after sma, but set to null the global
*		to avoid memory errors and or compilation warnings
*/
apc_sma_api_impl(apcue_sma, &apcue_cache, apcue_cache_expunge);

/* {{{ apcue_functions[]
 */
const zend_function_entry apcue_functions[] = {
	PHP_FE(apcue_get,	NULL)
	PHP_FE(apcue_set,	NULL)
	PHP_FE_END
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
	NULL,
	NULL,
	PHP_MINFO(apcue),
#if ZEND_MODULE_API_NO >= 20010901
	"0.0",
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_APCUE
ZEND_GET_MODULE(apcue)
#endif

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(apcue)
{
	/* take more care than this, do not initialize or create twice */

	/* initialize sma, use a sensible amount of memory !! */
	apcue_sma.init(1, 1024*1024*32, NULL TSRMLS_CC);
	
	/* create cache in shared memory */ 
	apcue_cache = apc_cache_create(
		&apcue_sma,
        NULL, /* default PHP serializer */
		10, 0L, 0L, 0L, 1 TSRMLS_CC
	);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(apcue)
{
	/* take more care than this, do not destroy and cleanup twice */	

	/* destroy cache */
	apc_cache_destroy(
		apcue_cache TSRMLS_CC);
	
	/* cleanup sma */
	apcue_sma.cleanup(TSRMLS_C);

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
}
/* }}} */

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
