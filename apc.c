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
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  |          George Schlossnagle <george@omniti.com>                     |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  |          Arun C. Murthy <arunc@yahoo-inc.com>                        |
  |          Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

#include "apc.h"
#include "apc_cache.h"
#include "apc_globals.h"
#include "php.h"

/* {{{ console display functions */
#define APC_PRINT_FUNCTION(name, verbosity)					\
	void apc_##name(const char *format, ...)				\
	{									\
		va_list args;							\
										\
		va_start(args, format);						\
		php_verror(NULL, "", verbosity, format, args);			\
		va_end(args);							\
	}

APC_PRINT_FUNCTION(error, E_ERROR)
APC_PRINT_FUNCTION(warning, E_WARNING)
APC_PRINT_FUNCTION(notice, E_NOTICE)

#ifdef APC_DEBUG
APC_PRINT_FUNCTION(debug, E_NOTICE)
#else
void apc_debug(const char *format, ...) {}
#endif
/* }}} */

/* {{{ apc_flip_hash */
HashTable* apc_flip_hash(HashTable *hash) {
	zval data, *entry;
	HashTable *new_hash;

	if(hash == NULL) return hash;

	ZVAL_LONG(&data, 1);

	ALLOC_HASHTABLE(new_hash);
	zend_hash_init(new_hash, zend_hash_num_elements(hash), NULL, ZVAL_PTR_DTOR, 0);

	ZEND_HASH_FOREACH_VAL(hash, entry) {
		ZVAL_DEREF(entry);
		if (Z_TYPE_P(entry) == IS_STRING) {
			zend_hash_update(new_hash, Z_STR_P(entry), &data);
		} else {
			zend_hash_index_update(new_hash, Z_LVAL_P(entry), &data);
		}
	} ZEND_HASH_FOREACH_END();

	return new_hash;
}
/* }}} */

/*
* Serializer API
*/
#define APC_MAX_SERIALIZERS 16

/* pointer to the list of serializers */
static apc_serializer_t apc_serializers[APC_MAX_SERIALIZERS] = {{0,}};
/* }}} */

/* {{{ apc_register_serializer */
PHP_APCU_API int _apc_register_serializer(
        const char* name, apc_serialize_t serialize, apc_unserialize_t unserialize, void *config) {
	int i;
	apc_serializer_t *serializer;

	for(i = 0; i < APC_MAX_SERIALIZERS; i++) {
		serializer = &apc_serializers[i];
		if(!serializer->name) {
			/* empty entry */
			serializer->name = name;
			serializer->serialize = serialize;
			serializer->unserialize = unserialize;
			serializer->config = config;
			if (i < APC_MAX_SERIALIZERS - 1) {
				apc_serializers[i+1].name = NULL;
			}
			return 1;
		}
	}

	return 0;
} /* }}} */

/* {{{ apc_get_serializers */
PHP_APCU_API apc_serializer_t* apc_get_serializers()  {
	return &(apc_serializers[0]);
} /* }}} */

/* {{{ apc_find_serializer */
PHP_APCU_API apc_serializer_t* apc_find_serializer(const char* name) {
	int i;
	apc_serializer_t *serializer;

	for(i = 0; i < APC_MAX_SERIALIZERS; i++) {
		serializer = &apc_serializers[i];
		if(serializer->name && (strcmp(serializer->name, name) == 0)) {
			return serializer;
		}
	}
	return NULL;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: noexpandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4 sts=4
 */
