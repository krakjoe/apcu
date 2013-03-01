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

/* $Id: apc_main.c 329498 2013-02-18 23:14:39Z gopalv $ */

#include "apc_php.h"
#include "apc_main.h"
#include "apc.h"
#include "apc_lock.h"
#include "apc_cache.h"
#include "apc_globals.h"
#include "apc_sma.h"
#include "apc_stack.h"
#include "apc_pool.h"
#include "apc_string.h"
#include "apc_serializer.h"
#include "SAPI.h"
#include "php_scandir.h"
#include "ext/standard/php_var.h"
#include "ext/standard/md5.h"

#define APC_MAX_SERIALIZERS 16

/* {{{ module variables */

/* pointer to the original Zend engine compile_file function */
static apc_serializer_t apc_serializers[APC_MAX_SERIALIZERS] = {{0,}};
/* }}} */

extern int _apc_store(char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC);

static zval* data_unserialize(const char *filename TSRMLS_DC)
{
    zval* retval;
    long len = 0;
    struct stat sb;
    char *contents, *tmp;
    FILE *fp;
    php_unserialize_data_t var_hash = {0,};

    if(VCWD_STAT(filename, &sb) == -1) {
        return NULL;
    }

    fp = fopen(filename, "rb");

    len = sizeof(char)*sb.st_size;

    tmp = contents = malloc(len);

    if(!contents) {
       return NULL;
    }

    if(fread(contents, 1, len, fp) < 1) {	
      free(contents);
      return NULL;
    }

    MAKE_STD_ZVAL(retval);

    PHP_VAR_UNSERIALIZE_INIT(var_hash);
    
    /* I wish I could use json */
    if(!php_var_unserialize(&retval, (const unsigned char**)&tmp, (const unsigned char*)(contents+len), &var_hash TSRMLS_CC)) {
        zval_ptr_dtor(&retval);
        return NULL;
    }

    PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

    free(contents);
    fclose(fp);

    return retval;
}

static int apc_load_data(const char *data_file TSRMLS_DC)
{
    char *p;
    char key[MAXPATHLEN] = {0,};
    unsigned int key_len;
    zval *data;

    p = strrchr(data_file, DEFAULT_SLASH);

    if(p && p[1]) {
        strlcpy(key, p+1, sizeof(key));
        p = strrchr(key, '.');

        if(p) {
            p[0] = '\0';
            key_len = strlen(key)+1;

            data = data_unserialize(data_file TSRMLS_CC);
            if(data) {
                _apc_store(key, key_len, data, 0, 1 TSRMLS_CC);
            }
            return 1;
        }
    }

    return 0;
}

#ifndef ZTS
static int apc_walk_dir(const char *path TSRMLS_DC)
{
    char file[MAXPATHLEN]={0,};
    int ndir, i;
    char *p = NULL;
    struct dirent **namelist = NULL;

    if ((ndir = php_scandir(path, &namelist, 0, php_alphasort)) > 0)
    {
        for (i = 0; i < ndir; i++)
        {
            /* check for extension */
            if (!(p = strrchr(namelist[i]->d_name, '.'))
                    || (p && strcmp(p, ".data")))
            {
                free(namelist[i]);
                continue;
            }
            snprintf(file, MAXPATHLEN, "%s%c%s",
                    path, DEFAULT_SLASH, namelist[i]->d_name);
            if(!apc_load_data(file TSRMLS_CC))
            {
                /* print error */
            }
            free(namelist[i]);
        }
        free(namelist);
    }

    return 1;
}
#endif

void apc_data_preload(TSRMLS_D)
{
    if(!APCG(preload_path)) return;
#ifndef ZTS
    apc_walk_dir(APCG(preload_path) TSRMLS_CC);
#else 
    apc_error("Cannot load data from apc.preload_path=%s in thread-safe mode" TSRMLS_CC, APCG(preload_path));
#endif

}
/* }}} */

/* {{{ apc_serializer hooks */
static int _apc_register_serializer(const char* name, apc_serialize_t serialize, 
                                    apc_unserialize_t unserialize,
                                    void *config TSRMLS_DC)
{
    int i;
    apc_serializer_t *serializer;

    for(i = 0; i < APC_MAX_SERIALIZERS; i++) {
        serializer = &apc_serializers[i];
        if(!serializer->name) {
            /* empty entry */
            serializer->name = name; /* assumed to be const */
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

apc_serializer_t* apc_find_serializer(const char* name TSRMLS_DC)
{
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

apc_serializer_t* apc_get_serializers(TSRMLS_D)
{
    return &(apc_serializers[0]);
}
/* }}} */

/* {{{ module init and shutdown */

int apc_module_init(int module_number TSRMLS_DC)
{
    /* apc initialization */
#if APC_MMAP
    apc_sma_init(APCG(shm_segments), APCG(shm_size), APCG(mmap_file_mask) TSRMLS_CC);
#else
    apc_sma_init(APCG(shm_segments), APCG(shm_size), NULL TSRMLS_CC);
#endif

    apc_user_cache = apc_cache_create(
		APCG(user_entries_hint), APCG(gc_ttl), APCG(user_ttl) TSRMLS_CC);

    apc_pool_init();

/*
 @TODO starting interned strings causes many things to fail right now
*/
#if 0
#ifdef ZEND_ENGINE_2_4
#ifndef ZTS
    apc_interned_strings_init(TSRMLS_C);
#endif
#endif
#endif

    apc_data_preload(TSRMLS_C);

    APCG(initialized) = 1;
    return 0;
}

int apc_module_shutdown(TSRMLS_D)
{
    if (!APCG(initialized))
        return 0;

#if 0
#ifdef ZEND_ENGINE_2_4
#ifndef ZTS
	apc_interned_strings_shutdown(TSRMLS_CC);
#endif
#endif
#endif

    apc_cache_destroy(apc_user_cache TSRMLS_CC);
    apc_sma_cleanup(TSRMLS_C);

    APCG(initialized) = 0;
    return 0;
}

/* }}} */

/* {{{ request init and shutdown */
int apc_request_init(TSRMLS_D)
{
    if (!APCG(serializer) && APCG(serializer_name)) {
        /* Avoid race conditions between MINIT of apc and serializer exts like igbinary */
        APCG(serializer) = apc_find_serializer(APCG(serializer_name) TSRMLS_CC);
    }

    return 0;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
