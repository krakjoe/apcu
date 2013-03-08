/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Gopal Vijayaraghavan <gopalv@php.net>                       |
  +----------------------------------------------------------------------+

 */

/* $Id: $ */

#ifndef APC_SERIALIZER_H
#define APC_SERIALIZER_H

/* this is a shipped .h file, do not include any other header in this file */
#define APC_SERIALIZER_NAME(module) module##_apc_serializer
#define APC_SERIALIZER_EXTERN(module) extern apc_serialize_t module##_apc_serializer
#define APC_UNSERIALIZER_NAME(module) module##_apc_unserializer
#define APC_UNSERIALIZER_EXTERN(module) extern apc_unserialize_t module##_apc_unserializer

#define APC_SERIALIZER_ARGS unsigned char **buf, size_t *buf_len, const zval *value, void *config TSRMLS_DC
#define APC_UNSERIALIZER_ARGS zval **value, unsigned char *buf, size_t buf_len, void *config TSRMLS_DC

typedef int (*apc_serialize_t)(APC_SERIALIZER_ARGS);
typedef int (*apc_unserialize_t)(APC_UNSERIALIZER_ARGS);

/* {{{ struct definition: apc_serializer_t */
typedef struct apc_serializer_t {
    const char*        name;
    apc_serialize_t    serialize;
    apc_unserialize_t  unserialize;
    void*              config;
} apc_serializer_t;
/* }}} */

#define APC_MAX_SERIALIZERS 16

/* pointer to the list of serializers */
static apc_serializer_t apc_serializers[APC_MAX_SERIALIZERS] = {{0,}};
/* }}} */

#if !defined(APC_UNUSED)
# if defined(__GNUC__)
#  define APC_UNUSED __attribute__((unused))
# else
# define APC_UNUSED
# endif
#endif

static zend_bool apc_register_serializer(const char* name, 
                                         apc_serialize_t serialize, 
                                         apc_unserialize_t unserialize,
                                         void *config TSRMLS_DC) {
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
}

static apc_serializer_t* apc_get_serializers(TSRMLS_D)  {
	return &(apc_serializers[0]);
}

static apc_serializer_t* apc_find_serializer(const char* name TSRMLS_DC) {
	int i;
    apc_serializer_t *serializer;

    for(i = 0; i < APC_MAX_SERIALIZERS; i++) {
        serializer = &apc_serializers[i];
        if(serializer->name && (strcmp(serializer->name, name) == 0)) {
            return serializer;
        }
    }
    return NULL;
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
