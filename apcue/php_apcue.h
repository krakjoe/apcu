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

/*
* This is an example of using the APCu API in a third party extension
*/

/* $Id$ */

#ifndef PHP_APCUE_H
#define PHP_APCUE_H

extern zend_module_entry apcue_module_entry;
#define phpext_apcue_ptr &apcue_module_entry

#ifdef PHP_WIN32
#	define PHP_APCUE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_APCUE_API __attribute__ ((visibility("default")))
#else
#	define PHP_APCUE_API
#endif

/*
* This is the only header needed
*/
#include "apcu/apc_api.h"

/*
* This is the only api declaration needed
*/
apc_sma_api_decl(apcue_sma);

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(apcue);
PHP_MSHUTDOWN_FUNCTION(apcue);
PHP_MINFO_FUNCTION(apcue);

PHP_FUNCTION(apcue_get);
PHP_FUNCTION(apcue_set);

#endif	/* PHP_APCUE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
