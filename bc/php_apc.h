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

#ifndef PHP_APC_H
#define PHP_APC_H

#define PHP_APC_VERSION PHP_APCU_VERSION
#define PHP_APC_EXTNAME "apc"

extern zend_module_entry apc_module_entry;
#define apc_module_ptr &apc_module_entry

#define phpext_apc_ptr apc_module_ptr

#if defined(ZTS) && defined(COMPILE_DL_APC)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

#endif /* PHP_APC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
