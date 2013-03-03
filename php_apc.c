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
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: php_apc.c 328290 2012-11-09 03:30:09Z laruence $ */

#include "apc_cache.h"
#include "apc_iterator.h"
#include "apc_main.h"
#include "apc_sma.h"
#include "apc_lock.h"
#include "apc_bin.h"
#include "php_globals.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "ext/standard/flock_compat.h"
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#include "SAPI.h"
#include "rfc1867.h"
#include "php_apc.h"
#include "ext/standard/md5.h"

#if HAVE_SIGACTION
#include "apc_signal.h"
#endif

/* {{{ PHP_FUNCTION declarations */
PHP_FUNCTION(apc_cache_info);
PHP_FUNCTION(apc_clear_cache);
PHP_FUNCTION(apc_sma_info);
PHP_FUNCTION(apc_store);
PHP_FUNCTION(apc_fetch);
PHP_FUNCTION(apc_delete);
PHP_FUNCTION(apc_add);
PHP_FUNCTION(apc_inc);
PHP_FUNCTION(apc_dec);
PHP_FUNCTION(apc_cas);
PHP_FUNCTION(apc_exists);
#if !defined(_WIN32) && !defined(ZTS)
PHP_FUNCTION(apc_store_async);
#endif
PHP_FUNCTION(apc_bin_dump);
PHP_FUNCTION(apc_bin_load);
PHP_FUNCTION(apc_bin_dumpfile);
PHP_FUNCTION(apc_bin_loadfile);
/* }}} */

/* {{{ ZEND_DECLARE_MODULE_GLOBALS(apcu) */
ZEND_DECLARE_MODULE_GLOBALS(apcu)

/* True globals */
apc_cache_t* apc_user_cache = NULL;

static void php_apc_init_globals(zend_apcu_globals* apcu_globals TSRMLS_DC)
{
    apcu_globals->initialized = 0;
    apcu_globals->write_lock = 1;
    apcu_globals->slam_defense = 1;
#ifdef MULTIPART_EVENT_FORMDATA
    apcu_globals->rfc1867 = 0;
    memset(&(apcu_globals->rfc1867_data), 0, sizeof(apc_rfc1867_data));
#endif
    memset(&apcu_globals->copied_zvals, 0, sizeof(HashTable));
	apcu_globals->preload_path = NULL;
    apcu_globals->coredump_unmap = 0;
    apcu_globals->use_request_time = 1;
    apcu_globals->serializer_name = NULL;
    apcu_globals->serializer = NULL;
}

static void php_apc_shutdown_globals(zend_apcu_globals* apcu_globals TSRMLS_DC)
{
    /* the rest of the globals are cleaned up in apc_module_shutdown() */
}

static long apc_atol(const char *str, int str_len)
{
#if PHP_MAJOR_VERSION >= 6 || PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3
    return zend_atol(str, str_len);
#else
    /* Re-implement zend_atol() for 5.2.x */
    long retval;

    if (!str_len) {
        str_len = strlen(str);
    }

    retval = strtol(str, NULL, 0);

    if (str_len > 0) {
        switch (str[str_len - 1]) {
            case 'g':
            case 'G':
                retval *= 1024;
                /* break intentionally missing */
            case 'm':
            case 'M':
                retval *= 1024;
                /* break intentionally missing */
            case 'k':
            case 'K':
                retval *= 1024;
                break;
        }
    }

    return retval;
#endif
}

/* }}} */

/* {{{ PHP_INI */

static PHP_INI_MH(OnUpdateShmSegments) /* {{{ */
{
#if APC_MMAP
    if(zend_atoi(new_value, new_value_length)!=1) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "apc.shm_segments setting ignored in MMAP mode");
    }
    APCG(shm_segments) = 1;
#else
    APCG(shm_segments) = zend_atoi(new_value, new_value_length);
#endif
    return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateShmSize) /* {{{ */
{
    long s = apc_atol(new_value, new_value_length);

    if(s <= 0) {
        return FAILURE;
    }

    if(s < 1048576L) {
        /* if it's less than 1Mb, they are probably using the old syntax */
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "apc.shm_size now uses M/G suffixes, please update your ini files");
        s = s * 1048576L;
    }

    APCG(shm_size) = s;

    return SUCCESS;
}
/* }}} */

#ifdef MULTIPART_EVENT_FORMDATA
static PHP_INI_MH(OnUpdateRfc1867Freq) /* {{{ */
{
    int tmp;
    tmp = zend_atoi(new_value, new_value_length);
    if(tmp < 0) {
        apc_error("rfc1867_freq must be greater than or equal to zero." TSRMLS_CC);
        return FAILURE;
    }
    if(new_value[new_value_length-1] == '%') {
        if(tmp > 100) {
            apc_error("rfc1867_freq cannot be over 100%%" TSRMLS_CC);
            return FAILURE;
        }
        APCG(rfc1867_freq) = tmp / 100.0;
    } else {
        APCG(rfc1867_freq) = tmp;
    }
    return SUCCESS;
}
/* }}} */
#endif

PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN("apc.enabled",      "1",    PHP_INI_SYSTEM, OnUpdateBool,              enabled,         zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.shm_segments",   "1",    PHP_INI_SYSTEM, OnUpdateShmSegments,       shm_segments,    zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.shm_size",       "32M",  PHP_INI_SYSTEM, OnUpdateShmSize,           shm_size,        zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.user_entries_hint", "4096", PHP_INI_SYSTEM, OnUpdateLong,          user_entries_hint, zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.gc_ttl",         "3600", PHP_INI_SYSTEM, OnUpdateLong,            gc_ttl,           zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.ttl",            "0",    PHP_INI_SYSTEM, OnUpdateLong,            ttl,              zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.user_ttl",       "0",    PHP_INI_SYSTEM, OnUpdateLong,            user_ttl,         zend_apcu_globals, apcu_globals)
#if APC_MMAP
STD_PHP_INI_ENTRY("apc.mmap_file_mask",  NULL,  PHP_INI_SYSTEM, OnUpdateString,         mmap_file_mask,   zend_apcu_globals, apcu_globals)
#endif
STD_PHP_INI_BOOLEAN("apc.enable_cli", "0",      PHP_INI_SYSTEM, OnUpdateBool,           enable_cli,       zend_apcu_globals, apcu_globals)
STD_PHP_INI_BOOLEAN("apc.write_lock", "1",      PHP_INI_SYSTEM, OnUpdateBool,           write_lock,       zend_apcu_globals, apcu_globals)
STD_PHP_INI_BOOLEAN("apc.slam_defense", "1",    PHP_INI_SYSTEM, OnUpdateBool,           slam_defense,     zend_apcu_globals, apcu_globals)
#ifdef MULTIPART_EVENT_FORMDATA
STD_PHP_INI_BOOLEAN("apc.rfc1867", "0", PHP_INI_SYSTEM, OnUpdateBool, rfc1867, zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_prefix", "upload_", PHP_INI_SYSTEM, OnUpdateStringUnempty, rfc1867_prefix, zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_name", "APC_UPLOAD_PROGRESS", PHP_INI_SYSTEM, OnUpdateStringUnempty, rfc1867_name, zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_freq", "0", PHP_INI_SYSTEM, OnUpdateRfc1867Freq, rfc1867_freq, zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.rfc1867_ttl", "3600", PHP_INI_SYSTEM, OnUpdateLong, rfc1867_ttl, zend_apcu_globals, apcu_globals)
#endif
STD_PHP_INI_ENTRY("apc.preload_path", (char*)NULL,              PHP_INI_SYSTEM, OnUpdateString,       preload_path,  zend_apcu_globals, apcu_globals)
STD_PHP_INI_BOOLEAN("apc.coredump_unmap", "0", PHP_INI_SYSTEM, OnUpdateBool, coredump_unmap, zend_apcu_globals, apcu_globals)
STD_PHP_INI_BOOLEAN("apc.use_request_time", "1", PHP_INI_ALL, OnUpdateBool, use_request_time,  zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.serializer", "default", PHP_INI_SYSTEM, OnUpdateStringUnempty, serializer_name, zend_apcu_globals, apcu_globals)
PHP_INI_END()

/* }}} */

/* {{{ PHP_MINFO_FUNCTION(apcu) */
static PHP_MINFO_FUNCTION(apcu)
{
    apc_serializer_t *serializer = NULL;
    smart_str names = {0,};
    int i;

    php_info_print_table_start();
    php_info_print_table_header(2, "APCu Support", APCG(enabled) ? "enabled" : "disabled");
    php_info_print_table_row(2, "Version", PHP_APC_VERSION);
#ifdef __DEBUG_APC__
    php_info_print_table_row(2, "APCu Debugging", "Enabled");
#else
    php_info_print_table_row(2, "APCu Debugging", "Disabled");
#endif
#if APC_MMAP
    php_info_print_table_row(2, "MMAP Support", "Enabled");
    php_info_print_table_row(2, "MMAP File Mask", APCG(mmap_file_mask));
#else
    php_info_print_table_row(2, "MMAP Support", "Disabled");
#endif
    php_info_print_table_row(2, "Locking type", APC_LOCK_TYPE);

    for( i = 0, serializer = apc_get_serializers(TSRMLS_C); 
                serializer->name != NULL; 
                serializer++, i++) {
        if(i != 0) smart_str_appends(&names, ", ");
        smart_str_appends(&names, serializer->name);
    }

    if(names.c) {
        smart_str_0(&names);
        php_info_print_table_row(2, "Serialization Support", names.c);
        smart_str_free(&names);
    } else {
        php_info_print_table_row(2, "Serialization Support", "broken");
    }

    php_info_print_table_row(2, "Revision", "$Revision: 328290 $");
    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}
/* }}} */

#ifdef MULTIPART_EVENT_FORMDATA
extern int apc_rfc1867_progress(unsigned int event, void *event_data, void **extra TSRMLS_DC);
#endif

/* {{{ PHP_MINIT_FUNCTION(apcu) */
static PHP_MINIT_FUNCTION(apcu)
{
    ZEND_INIT_MODULE_GLOBALS(apcu, php_apc_init_globals, php_apc_shutdown_globals);

    REGISTER_INI_ENTRIES();

    /* Disable APC in cli mode unless overridden by apc.enable_cli */
    if(!APCG(enable_cli) && !strcmp(sapi_module.name, "cli")) {
        APCG(enabled) = 0;
    }

    if (APCG(enabled)) {
        if(!APCG(initialized)) {
			apc_module_init(module_number TSRMLS_CC);
#ifdef MULTIPART_EVENT_FORMDATA
            /* File upload progress tracking */
            if(APCG(rfc1867)) {
                php_rfc1867_callback = apc_rfc1867_progress;
            }
#endif
            apc_iterator_init(module_number TSRMLS_CC);
        }
    }

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION(apcu) */
static PHP_MSHUTDOWN_FUNCTION(apcu)
{
    if(APCG(enabled)) {
        apc_module_shutdown(TSRMLS_C);
#ifndef ZTS
        php_apc_shutdown_globals(&apcu_globals);
#endif
#if HAVE_SIGACTION
        apc_shutdown_signals(TSRMLS_C);
#endif
    }
#ifdef ZTS
    ts_free_id(apcu_globals_id);
#endif
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION(apcu) */
static PHP_RINIT_FUNCTION(apcu)
{
    if(APCG(enabled)) {
        apc_request_init(TSRMLS_C);

#if HAVE_SIGACTION
        apc_set_signals(TSRMLS_C);
#endif
    }
    return SUCCESS;
}
/* }}} */

/* {{{ proto void apc_clear_cache() */
PHP_FUNCTION(apc_clear_cache)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        return;
    }

    apc_cache_clear(
		apc_user_cache TSRMLS_CC);
    RETURN_TRUE;
}
/* }}} */

/* {{{ proto array apc_cache_info([bool limited]) */
PHP_FUNCTION(apc_cache_info)
{
    zval* info;
    zend_bool limited = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &limited) == FAILURE) {
        return;
    }

    info = apc_cache_info(apc_user_cache, limited TSRMLS_CC);

    if(!info) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file");
        RETURN_FALSE;
    }

    RETURN_ZVAL(info, 0, 1);

}
/* }}} */

/* {{{ proto array apc_sma_info([bool limited]) */
PHP_FUNCTION(apc_sma_info)
{
    apc_sma_info_t* info;
    zval* block_lists;
    int i;
    zend_bool limited = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &limited) == FAILURE) {
        return;
    }

    info = apc_sma_info(limited TSRMLS_CC);

    if(!info) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "No APC SMA info available.  Perhaps APC is disabled via apc.enabled?");
        RETURN_FALSE;
    }
    array_init(return_value);

    add_assoc_long(return_value, "num_seg", info->num_seg);
    add_assoc_double(return_value, "seg_size", (double)info->seg_size);
    add_assoc_double(return_value, "avail_mem", (double)apc_sma_get_avail_mem());

    if(limited) {
        apc_sma_free_info(info TSRMLS_CC);
        return;
    }

#if ALLOC_DISTRIBUTION
    {
        size_t *adist = apc_sma_get_alloc_distribution();
        zval* list;
        ALLOC_INIT_ZVAL(list);
        array_init(list);
        for(i=0; i<30; i++) {
            add_next_index_long(list, adist[i]);
        }
        add_assoc_zval(return_value, "adist", list);
    }
#endif
    ALLOC_INIT_ZVAL(block_lists);
    array_init(block_lists);

    for (i = 0; i < info->num_seg; i++) {
        apc_sma_link_t* p;
        zval* list;

        ALLOC_INIT_ZVAL(list);
        array_init(list);

        for (p = info->list[i]; p != NULL; p = p->next) {
            zval* link;

            ALLOC_INIT_ZVAL(link);
            array_init(link);

            add_assoc_long(link, "size", p->size);
            add_assoc_long(link, "offset", p->offset);
            add_next_index_zval(list, link);
        }
        add_next_index_zval(block_lists, list);
    }
    add_assoc_zval(return_value, "block_lists", block_lists);
    apc_sma_free_info(info TSRMLS_CC);
}
/* }}} */

/* {{{ _apc_update  */
int _apc_update(char *strkey, int strkey_len, apc_cache_updater_t updater, void* data TSRMLS_DC) 
{
    if(!APCG(enabled)) {
        return 0;
    }

    if (!APCG(serializer) && APCG(serializer_name)) {
        /* Avoid race conditions between MINIT of apc and serializer exts like igbinary */
        APCG(serializer) = apc_find_serializer(APCG(serializer_name) TSRMLS_CC);
    }

    HANDLE_BLOCK_INTERRUPTIONS();
    
    if (!_apc_cache_user_update(apc_user_cache, strkey, strkey_len + 1, updater, data TSRMLS_CC)) {
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return 0;
    }

    HANDLE_UNBLOCK_INTERRUPTIONS();

    return 1;
}
/* }}} */
    
/* {{{ _apc_store */
int _apc_store(char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC) {
    apc_cache_entry_t *entry;
    apc_cache_key_t key;
    time_t t;
    apc_context_t ctxt={0,};
    int ret = 1;

    t = apc_time();

    if(!APCG(enabled)) return 0;

    if (!APCG(serializer) && APCG(serializer_name)) {
        /* Avoid race conditions between MINIT of apc and serializer exts like igbinary */
        APCG(serializer) = apc_find_serializer(APCG(serializer_name) TSRMLS_CC);
    }

    HANDLE_BLOCK_INTERRUPTIONS();

    ctxt.pool = apc_pool_create(APC_SMALL_POOL, apc_sma_malloc, apc_sma_free, apc_sma_protect, apc_sma_unprotect TSRMLS_CC);
    if (!ctxt.pool) {
        apc_warning("Unable to allocate memory for pool." TSRMLS_CC);
        ret = 0;
        goto nocache;
    }

    ctxt.copy = APC_COPY_IN_USER;
    ctxt.force_update = 0;

    if (!apc_cache_make_user_key(&key, strkey, strkey_len, t)) {
        goto freepool;
    }

    if (apc_cache_is_last_key(apc_user_cache, &key, t TSRMLS_CC)) {
        goto freepool;
    }

    if (!(entry = apc_cache_make_user_entry(strkey, strkey_len, val, &ctxt, ttl TSRMLS_CC))) {
        goto freepool;
    }

    if (!apc_cache_user_insert(apc_user_cache, key, entry, &ctxt, t, exclusive TSRMLS_CC)) {
freepool:
        apc_pool_destroy(ctxt.pool TSRMLS_CC);
        ret = 0;
    }

nocache:
    HANDLE_UNBLOCK_INTERRUPTIONS();

    return ret;
}
/* }}} */

#if !defined(_WIN32) && !defined(ZTS)
/* {{{ _apc_async_store(apc_async_insert_t *data)
 Avoids incurring cache locking in the thread calling store by making the insert in a separate context to the generation of key/entry
 This may develop into more of a worker kind of functionality, right now, thread per set ...
*/
void* _apc_async_store(void *data) {
	/* grab the insert data */
	apc_async_insert_t *insert = (apc_async_insert_t*) data;
	
	if (insert) {
		
		HANDLE_BLOCK_INTERRUPTIONS();
		
		/* make the insert */
		if (!apc_cache_user_insert(insert->cache, insert->key, insert->entry, &insert->ctx, insert->ctime, 0 TSRMLS_CC)) {
			/* do something insertion failed */
		}

		/* temporary measure */
		apc_pool_destroy(insert->ctx.pool TSRMLS_CC);
		apc_efree(insert);
		
		HANDLE_UNBLOCK_INTERRUPTIONS();
	}

	/* no future, yet */
	pthread_exit(NULL);
} /* }}} */

/* {{{ _apc_store */
int _apc_store_async(char *strkey, int strkey_len, const zval *val, const unsigned int ttl, const int exclusive TSRMLS_DC) {
	int ret = 1;
	
    if(!APCG(enabled)) return 0;

    if (!APCG(serializer) && APCG(serializer_name)) {
        /* Avoid race conditions between MINIT of apc and serializer exts like igbinary */
        APCG(serializer) = apc_find_serializer(APCG(serializer_name) TSRMLS_CC);
    }

	HANDLE_BLOCK_INTERRUPTIONS();
	{
		apc_async_insert_t *insert = apc_cache_make_async_insert(strkey, strkey_len, val, ttl TSRMLS_CC);

		if (insert) {
			pthread_t pthread;
			if (pthread_create(&pthread, NULL, (apc_async_worker_t) _apc_async_store, insert) != SUCCESS) {
				ret = 0;
			}
		} else {
			ret = 0;		
		}
	}
	HANDLE_UNBLOCK_INTERRUPTIONS();

    return ret;
}
/* }}} */
#endif

/* {{{ apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const int exclusive)
 */
static void apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const int exclusive, const int async)
{
    zval *key = NULL;
    zval *val = NULL;
    long ttl = 0L;
    HashTable *hash;
    HashPosition hpos;
    zval **hentry;
    char *hkey=NULL;
    uint hkey_len;
    ulong hkey_idx;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zl", &key, &val, &ttl) == FAILURE) {
        return;
    }

    if (!key) RETURN_FALSE;

    if (Z_TYPE_P(key) == IS_ARRAY) {
        hash = Z_ARRVAL_P(key);
        array_init(return_value);
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            zend_hash_get_current_key_ex(hash, &hkey, &hkey_len, &hkey_idx, 0, &hpos);
            if (hkey) {
#if !defined(_WIN32) && !defined(ZTS)
                if (async) {
					if(!_apc_store_async(hkey, hkey_len, *hentry, (unsigned int)ttl, exclusive TSRMLS_CC)) {
		                add_assoc_long_ex(return_value, hkey, hkey_len, -1);  /* -1: insertion error */
		            }
				} else if(!_apc_store(hkey, hkey_len, *hentry, (unsigned int)ttl, exclusive TSRMLS_CC)) {
                    add_assoc_long_ex(return_value, hkey, hkey_len, -1);  /* -1: insertion error */
                }
#else
				if(!_apc_store(hkey, hkey_len, *hentry, (unsigned int)ttl, exclusive TSRMLS_CC)) {
                    add_assoc_long_ex(return_value, hkey, hkey_len, -1);  /* -1: insertion error */
                }
#endif
                hkey = NULL;
            } else {
                add_index_long(return_value, hkey_idx, -1);  /* -1: insertion error */
            }
            zend_hash_move_forward_ex(hash, &hpos);
        }
        return;
    } else if (Z_TYPE_P(key) == IS_STRING) {
        if (!val) RETURN_FALSE;
#if !defined(_WIN32) && !defined(ZTS)
		if (async) {
			if(_apc_store_async(Z_STRVAL_P(key), Z_STRLEN_P(key) + 1, val, (unsigned int)ttl, exclusive TSRMLS_CC))
            	RETURN_TRUE;
		} else {
			if(_apc_store(Z_STRVAL_P(key), Z_STRLEN_P(key) + 1, val, (unsigned int)ttl, exclusive TSRMLS_CC))
            	RETURN_TRUE;
		}
#else
		if(_apc_store(Z_STRVAL_P(key), Z_STRLEN_P(key) + 1, val, (unsigned int)ttl, exclusive TSRMLS_CC))
            RETURN_TRUE;
#endif
    } else {
        apc_warning("apc_store expects key parameter to be a string or an array of key/value pairs." TSRMLS_CC);
    }

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto int apc_store(mixed key, mixed var [, long ttl ])
 */
PHP_FUNCTION(apc_store) {
    apc_store_helper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 0);
}
/* }}} */

#if !defined(_WIN32) && !defined(ZTS)
/* {{{ proto int apc_store_async(mixed key, mixed var [, long ttl]) 
*/
PHP_FUNCTION(apc_store_async) {
	apc_store_helper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 1);
} /* }}} */
#endif

/* {{{ proto int apc_add(mixed key, mixed var [, long ttl ])
 */
PHP_FUNCTION(apc_add) {
    apc_store_helper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 0);
}
/* }}} */

/* {{{ inc_updater */

struct _inc_update_args {
    long step;
    long lval;
};

static int inc_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {

    struct _inc_update_args *args = (struct _inc_update_args*) data;
    
    zval* val = entry->data.val;

    if(Z_TYPE_P(val) == IS_LONG) {
        Z_LVAL_P(val) += args->step;
        args->lval = Z_LVAL_P(val);
        return 1;
    }

    return 0;
}
/* }}} */

/* {{{ proto long apc_inc(string key [, long step [, bool& success]])
 */
PHP_FUNCTION(apc_inc) {
    char *strkey;
    int strkey_len;
    struct _inc_update_args args = {1L, -1};
    zval *success = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lz", &strkey, &strkey_len, &(args.step), &success) == FAILURE) {
        return;
    }
    
	if (success) {
		zval_dtor(success);
	}

    if(_apc_update(strkey, strkey_len, inc_updater, &args TSRMLS_CC)) {
        if(success) ZVAL_TRUE(success);
        RETURN_LONG(args.lval);
    }
    
    if(success) ZVAL_FALSE(success);
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto long apc_dec(string key [, long step [, bool &success]])
 */
PHP_FUNCTION(apc_dec) {
    char *strkey;
    int strkey_len;
    struct _inc_update_args args = {1L, -1};
    zval *success = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lz", &strkey, &strkey_len, &(args.step), &success) == FAILURE) {
        return;
    }
    
	if (success) {
		zval_dtor(success);
	}

    args.step = args.step * -1;

    if(_apc_update(strkey, strkey_len, inc_updater, &args TSRMLS_CC)) {
        if(success) ZVAL_TRUE(success);
        RETURN_LONG(args.lval);
    }
    
    if(success) ZVAL_FALSE(success);
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ cas_updater */
static int cas_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {
    long* vals = ((long*)data);
    long old = vals[0];
    long new = vals[1];
    zval* val = entry->data.val;

    if(Z_TYPE_P(val) == IS_LONG) {
        if(Z_LVAL_P(val) == old) {
            Z_LVAL_P(val) = new;
            return 1;
        }
    }

    return 0;
}
/* }}} */

/* {{{ proto int apc_cas(string key, int old, int new)
 */
PHP_FUNCTION(apc_cas) {
    char *strkey;
    int strkey_len;
    long vals[2];

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &strkey, &strkey_len, &vals[0], &vals[1]) == FAILURE) {
        return;
    }

    if(_apc_update(strkey, strkey_len, cas_updater, &vals TSRMLS_CC)) RETURN_TRUE;
    RETURN_FALSE;
}
/* }}} */

void *apc_erealloc_wrapper(void *ptr, size_t size) {
    return _erealloc(ptr, size, 0 ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC);
}

/* {{{ proto mixed apc_fetch(mixed key[, bool &success])
 */
PHP_FUNCTION(apc_fetch) {
    zval *key;
    zval *success = NULL;
    HashTable *hash;
    HashPosition hpos;
    zval **hentry;
    zval *result;
    zval *result_entry;
    char *strkey;
    int strkey_len;
    apc_cache_entry_t* entry;
    time_t t;
    apc_context_t ctxt = {0,};

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &key, &success) == FAILURE) {
        return;
    }

    t = apc_time();

    if (success) {
        ZVAL_BOOL(success, 0);
    }

    ctxt.pool = apc_pool_create(APC_UNPOOL, apc_php_malloc, apc_php_free, NULL, NULL TSRMLS_CC);
    if (!ctxt.pool) {
        apc_warning("Unable to allocate memory for pool." TSRMLS_CC);
        RETURN_FALSE;
    }
    ctxt.copy = APC_COPY_OUT_USER;
    ctxt.force_update = 0;

    if(Z_TYPE_P(key) != IS_STRING && Z_TYPE_P(key) != IS_ARRAY) {
        convert_to_string(key);
    }

    if(Z_TYPE_P(key) == IS_STRING) {
        strkey = Z_STRVAL_P(key);
        strkey_len = Z_STRLEN_P(key);
        if(!strkey_len) RETURN_FALSE;
        entry = apc_cache_user_find(apc_user_cache, strkey, (strkey_len + 1), t TSRMLS_CC);
        if(entry) {
            /* deep-copy returned shm zval to emalloc'ed return_value */
            apc_cache_fetch_zval(return_value, entry->data.val, &ctxt TSRMLS_CC);
            apc_cache_release(apc_user_cache, entry TSRMLS_CC);
        } else {
            goto freepool;
        }
    } else if(Z_TYPE_P(key) == IS_ARRAY) {
        hash = Z_ARRVAL_P(key);
        MAKE_STD_ZVAL(result);
        array_init(result); 
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            if(Z_TYPE_PP(hentry) != IS_STRING) {
                apc_warning("apc_fetch() expects a string or array of strings." TSRMLS_CC);
                goto freepool;
            }
            entry = apc_cache_user_find(apc_user_cache, Z_STRVAL_PP(hentry), (Z_STRLEN_PP(hentry) + 1), t TSRMLS_CC);
            if(entry) {
                /* deep-copy returned shm zval to emalloc'ed return_value */
                MAKE_STD_ZVAL(result_entry);
                apc_cache_fetch_zval(result_entry, entry->data.val, &ctxt TSRMLS_CC);
                apc_cache_release(apc_user_cache, entry TSRMLS_CC);
                zend_hash_add(Z_ARRVAL_P(result), Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) +1, &result_entry, sizeof(zval*), NULL);
            } /* don't set values we didn't find */
            zend_hash_move_forward_ex(hash, &hpos);
        }
        RETVAL_ZVAL(result, 0, 1);
    } else {
        apc_warning("apc_fetch() expects a string or array of strings." TSRMLS_CC);
freepool:
        apc_pool_destroy(ctxt.pool TSRMLS_CC);
        RETURN_FALSE;
    }

    if (success) {
        ZVAL_BOOL(success, 1);
    }

    apc_pool_destroy(ctxt.pool TSRMLS_CC);
    return;
}
/* }}} */

/* {{{ proto mixed apc_exists(mixed key)
 */
PHP_FUNCTION(apc_exists) {
    zval *key;
    HashTable *hash;
    HashPosition hpos;
    zval **hentry;
    char *strkey;
    int strkey_len;
    apc_cache_entry_t* entry;
    zval *result;
    zval *result_entry;
    time_t t;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &key) == FAILURE) {
        return;
    }

    t = apc_time();

    if(Z_TYPE_P(key) != IS_STRING && Z_TYPE_P(key) != IS_ARRAY) {
        convert_to_string(key);
    }

    if(Z_TYPE_P(key) == IS_STRING) {
        strkey = Z_STRVAL_P(key);
        strkey_len = Z_STRLEN_P(key);
        if(!strkey_len) RETURN_FALSE;
        entry = apc_cache_user_exists(apc_user_cache, strkey, strkey_len + 1, t TSRMLS_CC);
        if(entry) {
            RETURN_TRUE;
        }
    } else if(Z_TYPE_P(key) == IS_ARRAY) {
        hash = Z_ARRVAL_P(key);
        MAKE_STD_ZVAL(result);
        array_init(result); 
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            if(Z_TYPE_PP(hentry) != IS_STRING) {
                apc_warning("apc_exists() expects a string or array of strings." TSRMLS_CC);
                RETURN_FALSE;
            }

            entry = apc_cache_user_exists(apc_user_cache, Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) + 1, t TSRMLS_CC);
            if(entry) {
                MAKE_STD_ZVAL(result_entry);
                ZVAL_BOOL(result_entry, 1);
                zend_hash_add(Z_ARRVAL_P(result), Z_STRVAL_PP(hentry), Z_STRLEN_PP(hentry) +1, &result_entry, sizeof(zval*), NULL);
            } /* don't set values we didn't find */
            zend_hash_move_forward_ex(hash, &hpos);
        }
        RETURN_ZVAL(result, 0, 1);
    } else {
        apc_warning("apc_exists() expects a string or array of strings." TSRMLS_CC);
    }

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto mixed apc_delete(mixed keys)
 */
PHP_FUNCTION(apc_delete) {
    zval *keys;

    if(!APCG(enabled)) RETURN_FALSE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &keys) == FAILURE) {
        return;
    }

    if (Z_TYPE_P(keys) == IS_STRING) {
        if (!Z_STRLEN_P(keys)) RETURN_FALSE;
        if(apc_cache_user_delete(apc_user_cache, Z_STRVAL_P(keys), (Z_STRLEN_P(keys) + 1) TSRMLS_CC)) {
            RETURN_TRUE;
        } else {
            RETURN_FALSE;
        }
    } else if (Z_TYPE_P(keys) == IS_ARRAY) {
        HashTable *hash = Z_ARRVAL_P(keys);
        HashPosition hpos;
        zval **hentry;
        array_init(return_value);
        zend_hash_internal_pointer_reset_ex(hash, &hpos);
        while(zend_hash_get_current_data_ex(hash, (void**)&hentry, &hpos) == SUCCESS) {
            if(Z_TYPE_PP(hentry) != IS_STRING) {
                apc_warning("apc_delete() expects a string, array of strings, or APCIterator instance." TSRMLS_CC);
                add_next_index_zval(return_value, *hentry);
                Z_ADDREF_PP(hentry);
            } else if(apc_cache_user_delete(apc_user_cache, Z_STRVAL_PP(hentry), (Z_STRLEN_PP(hentry) + 1) TSRMLS_CC) != 1) {
                add_next_index_zval(return_value, *hentry);
                Z_ADDREF_PP(hentry);
            }
            zend_hash_move_forward_ex(hash, &hpos);
        }
        return;
    } else if (Z_TYPE_P(keys) == IS_OBJECT) {
        if (apc_iterator_delete(keys TSRMLS_CC)) {
            RETURN_TRUE;
        } else {
            RETURN_FALSE;
        }
    } else {
        apc_warning("apc_delete() expects a string, array of strings, or APCIterator instance." TSRMLS_CC);
    }
}
/* }}} */

/* {{{ proto mixed apc_bin_dump([array vars])
    Returns a binary dump of the given user variables from the APC cache.
    A NULL for vars signals a dump of every entry, while array() will dump nothing.
 */
PHP_FUNCTION(apc_bin_dump) {

    zval *z_vars = NULL;
    HashTable  *h_vars;
    apc_bd_t *bd;

    if(!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_dump not available." TSRMLS_CC);
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!", &z_vars) == FAILURE) {
        return;
    }

    h_vars = z_vars ? Z_ARRVAL_P(z_vars) : NULL;
    bd = apc_bin_dump(NULL, h_vars TSRMLS_CC);
    if(bd) {
        RETVAL_STRINGL((char*)bd, bd->size-1, 0);
    } else {
        apc_error("Unknown error encountered during apc_bin_dump." TSRMLS_CC);
        RETVAL_NULL();
    }

    return;
}
/* }}} */


/* {{{ proto mixed apc_bin_dumpfile(array vars, string filename, [int flags [, resource context]])
    Output a binary dump of the given user variables from the APC cache to the named file.
 */
PHP_FUNCTION(apc_bin_dumpfile) {

    zval *z_vars = NULL;
    HashTable *h_vars;
    char *filename = NULL;
    int filename_len;
    long flags=0;
    zval *zcontext = NULL;
    php_stream_context *context = NULL;
    php_stream *stream;
    int numbytes = 0;
    apc_bd_t *bd;

    if(!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_dumpfile not available." TSRMLS_CC);
        RETURN_FALSE;
    }


    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a!s|lr!", &z_vars, &filename, &filename_len, &flags, &zcontext) == FAILURE) {
        return;
    }

    if(!filename_len) {
        apc_error("apc_bin_dumpfile filename argument must be a valid filename." TSRMLS_CC);
        RETURN_FALSE;
    }

    h_vars = z_vars ? Z_ARRVAL_P(z_vars) : NULL;
    bd = apc_bin_dump(NULL, h_vars TSRMLS_CC);
    if(!bd) {
        apc_error("Unknown error encountered during apc_bin_dumpfile." TSRMLS_CC);
        RETURN_FALSE;
    }


    /* Most of the following has been taken from the file_get/put_contents functions */

    context = php_stream_context_from_zval(zcontext, flags & PHP_FILE_NO_DEFAULT_CONTEXT);
    stream = php_stream_open_wrapper_ex(filename, (flags & PHP_FILE_APPEND) ? "ab" : "wb",
                                            ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL, context);
    if (stream == NULL) {
        efree(bd);
        apc_error("Unable to write to file in apc_bin_dumpfile." TSRMLS_CC);
        RETURN_FALSE;
    }

    if (flags & LOCK_EX && php_stream_lock(stream, LOCK_EX)) {
        php_stream_close(stream);
        efree(bd);
        apc_error("Unable to get a lock on file in apc_bin_dumpfile." TSRMLS_CC);
        RETURN_FALSE;
    }

    numbytes = php_stream_write(stream, (char*)bd, bd->size);
    if(numbytes != bd->size) {
        numbytes = -1;
    }

    php_stream_close(stream);
    efree(bd);

    if(numbytes < 0) {
        apc_error("Only %d of %d bytes written, possibly out of free disk space" TSRMLS_CC, numbytes, bd->size);
        RETURN_FALSE;
    }

    RETURN_LONG(numbytes);
}
/* }}} */

/* {{{ proto mixed apc_bin_load(string data, [int flags])
    Load the given binary dump into the APC file/user cache.
 */
PHP_FUNCTION(apc_bin_load) {

    int data_len;
    char *data;
    long flags = 0;

    if(!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_load not available." TSRMLS_CC);
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &data, &data_len, &flags) == FAILURE) {
        return;
    }

    if(!data_len || data_len != ((apc_bd_t*)data)->size -1) {
        apc_error("apc_bin_load string argument does not appear to be a valid APC binary dump due to size (%d vs expected %d)." TSRMLS_CC, data_len, ((apc_bd_t*)data)->size -1);
        RETURN_FALSE;
    }

    apc_bin_load((apc_bd_t*)data, (int)flags TSRMLS_CC);

    RETURN_TRUE;
}
/* }}} */

/* {{{ proto mixed apc_bin_loadfile(string filename, [resource context, [int flags]])
    Load the given binary dump from the named file into the APC file/user cache.
 */
PHP_FUNCTION(apc_bin_loadfile) {

    char *filename;
    int filename_len;
    zval *zcontext = NULL;
    long flags = 0;
    php_stream_context *context = NULL;
    php_stream *stream;
    char *data;
    int len;

    if(!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_loadfile not available." TSRMLS_CC);
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|r!l", &filename, &filename_len, &zcontext, &flags) == FAILURE) {
        return;
    }

    if(!filename_len) {
        apc_error("apc_bin_loadfile filename argument must be a valid filename." TSRMLS_CC);
        RETURN_FALSE;
    }

    context = php_stream_context_from_zval(zcontext, 0);
    stream = php_stream_open_wrapper_ex(filename, "rb",
            ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL, context);
    if (!stream) {
        apc_error("Unable to read from file in apc_bin_loadfile." TSRMLS_CC);
        RETURN_FALSE;
    }

    len = php_stream_copy_to_mem(stream, &data, PHP_STREAM_COPY_ALL, 0);
    if(len == 0) {
        apc_warning("File passed to apc_bin_loadfile was empty: %s." TSRMLS_CC, filename);
        RETURN_FALSE;
    } else if(len < 0) {
        apc_warning("Error reading file passed to apc_bin_loadfile: %s." TSRMLS_CC, filename);
        RETURN_FALSE;
    } else if(len != ((apc_bd_t*)data)->size) {
        apc_warning("file passed to apc_bin_loadfile does not appear to be valid due to size (%d vs expected %d)." TSRMLS_CC, len, ((apc_bd_t*)data)->size -1);
        RETURN_FALSE;
    }
    php_stream_close(stream);

    apc_bin_load((apc_bd_t*)data, (int)flags TSRMLS_CC);
    efree(data);

    RETURN_TRUE;
}
/* }}} */

/* {{{ arginfo */
#if (PHP_MAJOR_VERSION >= 6 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3))
# define PHP_APC_ARGINFO
#else
# define PHP_APC_ARGINFO static
#endif

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_store, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, var)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_cache_info, 0, 0, 0)
    ZEND_ARG_INFO(0, type)
    ZEND_ARG_INFO(0, limited)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_clear_cache, 0, 0, 0)
    ZEND_ARG_INFO(0, info)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_sma_info, 0, 0, 0)
    ZEND_ARG_INFO(0, limited)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apc_delete, 0)
    ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_fetch, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(1, success)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_inc, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, step)
    ZEND_ARG_INFO(1, success)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apc_cas, 0)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, old)
    ZEND_ARG_INFO(0, new)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apc_exists, 0)
    ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()


PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_dump, 0, 0, 0)
    ZEND_ARG_INFO(0, files)
    ZEND_ARG_INFO(0, user_vars)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_dumpfile, 0, 0, 3)
    ZEND_ARG_INFO(0, files)
    ZEND_ARG_INFO(0, user_vars)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, flags)
    ZEND_ARG_INFO(0, context)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_load, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apc_bin_loadfile, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, context)
    ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ apc_functions[] */
zend_function_entry apcu_functions[] = {
    PHP_FE(apc_cache_info,          arginfo_apc_cache_info)
    PHP_FE(apc_clear_cache,         arginfo_apc_clear_cache)
    PHP_FE(apc_sma_info,            arginfo_apc_sma_info)
    PHP_FE(apc_store,               arginfo_apc_store)
    PHP_FE(apc_fetch,               arginfo_apc_fetch)
    PHP_FE(apc_delete,              arginfo_apc_delete)
    PHP_FE(apc_add,                 arginfo_apc_store)
    PHP_FE(apc_inc,                 arginfo_apc_inc)
    PHP_FE(apc_dec,                 arginfo_apc_inc)
    PHP_FE(apc_cas,                 arginfo_apc_cas)
    PHP_FE(apc_exists,              arginfo_apc_exists)
#if !defined(_WIN32) && !defined(ZTS)
	PHP_FE(apc_store_async,			arginfo_apc_store)
#endif
    PHP_FE(apc_bin_dump,            arginfo_apc_bin_dump)
    PHP_FE(apc_bin_load,            arginfo_apc_bin_load)
    PHP_FE(apc_bin_dumpfile,        arginfo_apc_bin_dumpfile)
    PHP_FE(apc_bin_loadfile,        arginfo_apc_bin_loadfile)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ module definition structure */

zend_module_entry apcu_module_entry = {
    STANDARD_MODULE_HEADER,
    "apcu",
    apcu_functions,
    PHP_MINIT(apcu),
    PHP_MSHUTDOWN(apcu),
    PHP_RINIT(apcu),
    NULL,
    PHP_MINFO(apcu),
    PHP_APC_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_APCU
ZEND_GET_MODULE(apcu)
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
