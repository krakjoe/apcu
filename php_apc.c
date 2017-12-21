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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "apc_cache.h"
#include "apc_iterator.h"
#include "apc_sma.h"
#include "apc_lock.h"
#include "php_globals.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "ext/standard/flock_compat.h"
#include "ext/standard/md5.h"
#include "ext/standard/php_var.h"
#include "apc_arginfo.h"

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include "SAPI.h"
#include "php_apc.h"

#if HAVE_SIGACTION
#include "apc_signal.h"
#endif

/* {{{ PHP_FUNCTION declarations */
PHP_FUNCTION(apcu_cache_info);
PHP_FUNCTION(apcu_clear_cache);
PHP_FUNCTION(apcu_sma_info);
PHP_FUNCTION(apcu_key_info);
PHP_FUNCTION(apcu_store);
PHP_FUNCTION(apcu_fetch);
PHP_FUNCTION(apcu_delete);
PHP_FUNCTION(apcu_add);
PHP_FUNCTION(apcu_inc);
PHP_FUNCTION(apcu_dec);
PHP_FUNCTION(apcu_cas);
PHP_FUNCTION(apcu_exists);
/* }}} */

/* {{{ ZEND_DECLARE_MODULE_GLOBALS(apcu) */
ZEND_DECLARE_MODULE_GLOBALS(apcu)

/* True globals */
apc_cache_t* apc_user_cache = NULL;

/* External APC SMA */
apc_sma_api_extern(apc_sma);

/* Global init functions */
static void php_apc_init_globals(zend_apcu_globals* apcu_globals)
{
    apcu_globals->initialized = 0;
    apcu_globals->slam_defense = 1;
	apcu_globals->smart = 0;
	apcu_globals->preload_path = NULL;
    apcu_globals->coredump_unmap = 0;
    apcu_globals->use_request_time = 1;
    apcu_globals->serializer_name = NULL;
	apcu_globals->recursion = 0;
}
/* }}} */

/* {{{ PHP_INI */

static PHP_INI_MH(OnUpdateShmSegments) /* {{{ */
{
#if APC_MMAP
    if (zend_atoi(new_value->val, new_value->len)!=1) {
        php_error_docref(NULL, E_WARNING, "apc.shm_segments setting ignored in MMAP mode");
    }
    APCG(shm_segments) = 1;
#else
    APCG(shm_segments) = zend_atoi(new_value->val, new_value->len);
#endif
    return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateShmSize) /* {{{ */
{
    zend_long s = zend_atol(new_value->val, new_value->len);

    if (s <= 0) {
        return FAILURE;
    }

    if (s < Z_L(1048576)) {
        /* if it's less than 1Mb, they are probably using the old syntax */
        php_error_docref(	
			NULL, E_WARNING, "apc.shm_size now uses M/G suffixes, please update your ini files");
        s = s * Z_L(1048576);
    }

    APCG(shm_size) = s;

    return SUCCESS;
}
/* }}} */

PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN("apc.enabled",      "1",    PHP_INI_SYSTEM, OnUpdateBool,              enabled,          zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.shm_segments",   "1",    PHP_INI_SYSTEM, OnUpdateShmSegments,       shm_segments,     zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.shm_size",       "32M",  PHP_INI_SYSTEM, OnUpdateShmSize,           shm_size,         zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.entries_hint",   "4096", PHP_INI_SYSTEM, OnUpdateLong,              entries_hint,     zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.gc_ttl",         "3600", PHP_INI_SYSTEM, OnUpdateLong,              gc_ttl,           zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.ttl",            "0",    PHP_INI_SYSTEM, OnUpdateLong,              ttl,              zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.smart",          "0",    PHP_INI_SYSTEM, OnUpdateLong,              smart,            zend_apcu_globals, apcu_globals)
#if APC_MMAP
STD_PHP_INI_ENTRY("apc.mmap_file_mask",  NULL,  PHP_INI_SYSTEM, OnUpdateString,            mmap_file_mask,   zend_apcu_globals, apcu_globals)
#endif
STD_PHP_INI_BOOLEAN("apc.enable_cli",   "0",    PHP_INI_SYSTEM, OnUpdateBool,              enable_cli,       zend_apcu_globals, apcu_globals)
STD_PHP_INI_BOOLEAN("apc.slam_defense", "1",    PHP_INI_SYSTEM, OnUpdateBool,              slam_defense,     zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.preload_path", (char*)NULL,              PHP_INI_SYSTEM, OnUpdateString,       preload_path,  zend_apcu_globals, apcu_globals)
STD_PHP_INI_BOOLEAN("apc.coredump_unmap", "0", PHP_INI_SYSTEM, OnUpdateBool, coredump_unmap, zend_apcu_globals, apcu_globals)
STD_PHP_INI_BOOLEAN("apc.use_request_time", "1", PHP_INI_ALL, OnUpdateBool, use_request_time,  zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.serializer", "php", PHP_INI_SYSTEM, OnUpdateStringUnempty, serializer_name, zend_apcu_globals, apcu_globals)
STD_PHP_INI_ENTRY("apc.writable", "/tmp", PHP_INI_SYSTEM, OnUpdateStringUnempty, writable, zend_apcu_globals, apcu_globals)
PHP_INI_END()

/* }}} */

zend_bool apc_is_enabled(void)
{
	return APCG(enabled);
}

/* {{{ PHP_MINFO_FUNCTION(apcu) */
static PHP_MINFO_FUNCTION(apcu)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "APCu Support", APCG(enabled) ? "Enabled" : "Disabled");
    php_info_print_table_row(2, "Version", PHP_APCU_VERSION);
#ifdef APC_DEBUG
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

    if (APCG(enabled)) {
        apc_serializer_t *serializer = NULL;
        smart_str names = {0,};
        int i;
    
        for( i = 0, serializer = apc_get_serializers(); 
                    serializer->name != NULL; 
                    serializer++, i++) {
            if (i != 0) {
			    smart_str_appends(&names, ", ");
		    }
            smart_str_appends(&names, serializer->name);
        }
    
        if (names.s->val) {
            smart_str_0(&names);
            php_info_print_table_row(2, "Serialization Support", names.s->val);
            smart_str_free(&names);
        } else {
            php_info_print_table_row(2, "Serialization Support", "Broken");
        }
    } else {
        php_info_print_table_row(2, "Serialization Support", "Disabled");
    }

    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION(apcu) */
static PHP_MINIT_FUNCTION(apcu)
{
#if defined(ZTS) && defined(COMPILE_DL_APCU)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    ZEND_INIT_MODULE_GLOBALS(apcu, php_apc_init_globals, NULL);

    REGISTER_INI_ENTRIES();

	/* locks initialized regardless of settings */
	apc_lock_init();
	
    /* Disable APC in cli mode unless overridden by apc.enable_cli */
    if (!APCG(enable_cli) && !strcmp(sapi_module.name, "cli")) {
		APCG(enabled) = 0;
    }

	/* only run initialization if APC is enabled */
    if (APCG(enabled)) {

        if (!APCG(initialized)) {
			/* ensure this runs only once */
			APCG(initialized) = 1;
			
			/* initialize shared memory allocator */
#if APC_MMAP
			apc_sma.init(APCG(shm_segments), APCG(shm_size), APCG(mmap_file_mask));
#else
			apc_sma.init(APCG(shm_segments), APCG(shm_size), NULL);
#endif

			REGISTER_LONG_CONSTANT(APC_SERIALIZER_CONSTANT, (zend_long)&_apc_register_serializer, CONST_PERSISTENT | CONST_CS);

			/* register default serializer */
			_apc_register_serializer(
				"php", APC_SERIALIZER_NAME(php), APC_UNSERIALIZER_NAME(php), NULL);

			/* test out the constant function pointer */
			assert(apc_get_serializers()->name != NULL);

			/* create user cache */
			apc_user_cache = apc_cache_create(
				&apc_sma,
				apc_find_serializer(APCG(serializer_name)),
				APCG(entries_hint), APCG(gc_ttl), APCG(ttl), APCG(smart), APCG(slam_defense));
			
			/* initialize pooling */
			apc_pool_init();
			
			/* preload data from path specified in configuration */
			if (APCG(preload_path)) {
				apc_cache_preload(
					apc_user_cache, APCG(preload_path));
			}
        }
    }

	/* initialize iterator object */
    apc_iterator_init(module_number);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION(apcu) */
static PHP_MSHUTDOWN_FUNCTION(apcu)
{
	/* locks shutdown regardless of settings */
	apc_lock_cleanup();

	/* only shut down if APC is enabled */
    if (APCG(enabled)) {
        if (APCG(initialized)) {

			/* destroy cache pointer */
			apc_cache_destroy(apc_user_cache);
			/* cleanup shared memory */
			apc_sma.cleanup();

			APCG(initialized) = 0;
		}
		
#if HAVE_SIGACTION
        apc_shutdown_signals();
#endif
    }

    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
} /* }}} */

/* {{{ PHP_RINIT_FUNCTION(apcu) */
static PHP_RINIT_FUNCTION(apcu)
{
#if defined(ZTS) && defined(COMPILE_DL_APCU)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (APCG(enabled)) {
        if (APCG(serializer_name)) {
		    /* Avoid race conditions between MINIT of apc and serializer exts like igbinary */
			apc_cache_serializer(apc_user_cache, APCG(serializer_name));
		}

#if HAVE_SIGACTION
        apc_set_signals();
#endif
    }
    return SUCCESS;
}
/* }}} */

/* {{{ proto void apc_clear_cache() */
PHP_FUNCTION(apcu_clear_cache)
{
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    apc_cache_clear(
		apc_user_cache);
    RETURN_TRUE;
}
/* }}} */

/* {{{ proto array apc_cache_info([bool limited]) */
PHP_FUNCTION(apcu_cache_info)
{
    zval info;
    zend_bool limited = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &limited) == FAILURE) {
        return;
    }

    info = apc_cache_info(apc_user_cache, limited);

    if (Z_TYPE(info) != IS_ARRAY) {
        php_error_docref(NULL, E_WARNING, "No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file");
        RETURN_FALSE;
    }

    RETURN_ZVAL(&info, 0, 0);
}
/* }}} */

/* {{{ */
PHP_FUNCTION(apcu_key_info)
{
    zend_string *key;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &key) == FAILURE) {
        return;
    }
    
    apc_cache_stat(apc_user_cache, key, return_value);
} /* }}} */

/* {{{ proto array apc_sma_info([bool limited]) */
PHP_FUNCTION(apcu_sma_info)
{
    apc_sma_info_t* info;
    zval block_lists;
    int i;
    zend_bool limited = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &limited) == FAILURE) {
        return;
    }

    info = apc_sma.info(limited);

    if (!info) {
        php_error_docref(NULL, E_WARNING, "No APC SMA info available.  Perhaps APC is disabled via apc.enabled?");
        RETURN_FALSE;
    }
    array_init(return_value);

    add_assoc_long(return_value, "num_seg", info->num_seg);
    add_assoc_double(return_value, "seg_size", (double)info->seg_size);
    add_assoc_double(return_value, "avail_mem", (double)apc_sma.get_avail_mem());

    if (limited) {
        apc_sma.free_info(info);
        return;
    }

    array_init(&block_lists);

    for (i = 0; i < info->num_seg; i++) {
        apc_sma_link_t* p;
        zval list;

        array_init(&list);
        for (p = info->list[i]; p != NULL; p = p->next) {
            zval link;

            array_init(&link);

            add_assoc_long(&link, "size", p->size);
            add_assoc_long(&link, "offset", p->offset);
            add_next_index_zval(&list, &link);
        }
        add_next_index_zval(&block_lists, &list);
    }
    add_assoc_zval(return_value, "block_lists", &block_lists);
    apc_sma.free_info(info);
}
/* }}} */

/* {{{ php_apc_update  */
int php_apc_update(zend_string *key, apc_cache_updater_t updater, void* data) 
{
    if (!APCG(enabled)) {
        return 0;
    }

    if (APCG(serializer_name)) {
        /* Avoid race conditions between MINIT of apc and serializer exts like igbinary */
        apc_cache_serializer(apc_user_cache, APCG(serializer_name));
    }
    
    if (!apc_cache_update(apc_user_cache, key, updater, data)) {
        return 0;
    }

    return 1;
}
/* }}} */

/* {{{ apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const zend_bool exclusive)
 */
static void apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const zend_bool exclusive)
{
    zval *key = NULL;
    zval *val = NULL;
    zend_long ttl = 0L;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|zl", &key, &val, &ttl) == FAILURE) {
        return;
    }

    if (!key || !APCG(enabled)) {
        /* cannot work without key */
        RETURN_FALSE;
    }
    
	/* keep it tidy */
    {
		if (APCG(serializer_name)) {
        	/* Avoid race conditions between MINIT of apc and serializer exts like igbinary */
		    apc_cache_serializer(apc_user_cache, APCG(serializer_name));
		}

		if (Z_TYPE_P(key) == IS_ARRAY) {
            
            zval *hentry;
            zend_string *hkey;
            zend_ulong hkey_idx;

            HashPosition hpos;
            HashTable* hash = Z_ARRVAL_P(key);

            /* note: only indicative of error */
		    array_init(return_value);
		    zend_hash_internal_pointer_reset_ex(hash, &hpos);
		    while((hentry = zend_hash_get_current_data_ex(hash, &hpos))) {
		        if (zend_hash_get_current_key_ex(hash, &hkey, &hkey_idx, &hpos) == HASH_KEY_IS_STRING) {
		            if(!apc_cache_store(apc_user_cache, hkey, hentry, (uint32_t) ttl, exclusive)) {
		                add_assoc_long_ex(return_value, hkey->val, hkey->len, -1);  /* -1: insertion error */
		            }
		        } else {
		            add_index_long(return_value, hkey_idx, -1);  /* -1: insertion error */
		        }
		        zend_hash_move_forward_ex(hash, &hpos);
		    }
			return;
		} else {
            if (Z_TYPE_P(key) == IS_STRING) {
			    if (!val) {
                    /* nothing to store */
    	            RETURN_FALSE;
    	        }
                /* return true on success */
    			if(apc_cache_store(apc_user_cache, Z_STR_P(key), val, (uint32_t) ttl, exclusive)) {
    	            RETURN_TRUE;
                }
    		} else {
                apc_warning("apc_store expects key parameter to be a string or an array of key/value pairs.");
    		}
        }
	}
	
	/* default */
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool apcu_enabled(void)
    returns true when apcu is usable in the current environment */
PHP_FUNCTION(apcu_enabled) {
    RETURN_BOOL(APCG(enabled));
}  /* }}} */

/* {{{ proto int apc_store(mixed key, mixed var [, long ttl ])
 */
PHP_FUNCTION(apcu_store) {
    apc_store_helper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int apc_add(mixed key, mixed var [, long ttl ])
 */
PHP_FUNCTION(apcu_add) {
    apc_store_helper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ php_inc_updater */

struct php_inc_updater_args {
    zval step;
    zval rval;
};

static zend_bool php_inc_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {
    struct php_inc_updater_args *args = (struct php_inc_updater_args*) data;

    if (Z_TYPE(entry->val) == IS_LONG) {
        fast_long_add_function(&entry->val, &entry->val, &args->step);
        ZVAL_COPY_VALUE(&args->rval, &entry->val);
        return 1;
    }

    return 0;
}

/* {{{ proto long apc_inc(string key [, long step [, bool& success]])
 */
PHP_FUNCTION(apcu_inc) {
    zend_string *key;
    struct php_inc_updater_args args;
    zend_long step = 1;
    zval *success = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|lz", &key, &step, &success) == FAILURE) {
        return;
    }

	if (success) {
		ZVAL_DEREF(success);
		zval_ptr_dtor(success);
	}

	ZVAL_LONG(&args.step, step);

    if (php_apc_update(key, php_inc_updater, &args)) {
        if (success) {
			ZVAL_TRUE(success);
		}
        RETURN_ZVAL(&args.rval, 0, 0);
    }
    
    if (success) {
		ZVAL_FALSE(success);
	}
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto long apc_dec(string key [, long step [, bool &success]])
 */
PHP_FUNCTION(apcu_dec) {
    zend_string *key;
    struct php_inc_updater_args args;
    zend_long step = 1;
    zval *success = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|lz", &key, &step, &success) == FAILURE) {
        return;
    }

	if (success) {
		ZVAL_DEREF(success);
		zval_ptr_dtor(success);
	}

	ZVAL_LONG(&args.step, 0 - step);

    if (php_apc_update(key, php_inc_updater, &args)) {
        if (success) {
			ZVAL_TRUE(success);
		}

        RETURN_ZVAL(&args.rval, 0, 0);
    }
    
    if (success) {
		ZVAL_FALSE(success);
	}
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ php_cas_updater */
static zend_bool php_cas_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {
    zend_long* vals = ((zend_long*)data);
    zend_long old = vals[0];
    zend_long new = vals[1];

    if (Z_TYPE(entry->val) == IS_LONG) {
        if (Z_LVAL(entry->val) == old) {
            Z_LVAL(entry->val) = new;
            return 1;
        }
    }

    return 0;
}
/* }}} */

/* {{{ proto int apc_cas(string key, int old, int new)
 */
PHP_FUNCTION(apcu_cas) {
    zend_string *key;
    zend_long vals[2];

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sll", &key, &vals[0], &vals[1]) == FAILURE) {
        return;
    }

    if (php_apc_update(key, php_cas_updater, &vals)) {
		RETURN_TRUE;
	}

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto mixed apc_fetch(mixed key[, bool &success])
 */
PHP_FUNCTION(apcu_fetch) {
    zval *key;
    zval *success = NULL;
    time_t t;

    if (!APCG(enabled)) {
		RETURN_FALSE;
	}

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &key, &success) == FAILURE) {
        return;
    }

    t = apc_time();

    if (success) {
		ZVAL_DEREF(success);
        zval_ptr_dtor(success);
        ZVAL_FALSE(success);
    }

	if (Z_TYPE_P(key) != IS_STRING && Z_TYPE_P(key) != IS_ARRAY) {
	    convert_to_string(key);
	}

	if (Z_TYPE_P(key) == IS_ARRAY || (Z_TYPE_P(key) == IS_STRING && Z_STRLEN_P(key) > 0)) {
		if (Z_TYPE_P(key) == IS_STRING) {
			if (apc_cache_fetch(apc_user_cache, Z_STR_P(key), t, &return_value)) {
			    if (success) {
					ZVAL_TRUE(success);
				}
			} else { ZVAL_BOOL(return_value, 0); }
		} else if (Z_TYPE_P(key) == IS_ARRAY) {
			HashPosition hpos;
			zval *hentry;
			zval result;
            
			array_init(&result);
			zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(key), &hpos);
			while((hentry = zend_hash_get_current_data_ex(Z_ARRVAL_P(key), &hpos))) {
			    ZVAL_DEREF(hentry);
			    if (Z_TYPE_P(hentry) == IS_STRING) {
					zval result_entry,
						*iresult = &result_entry;
					ZVAL_UNDEF(iresult);

					if (apc_cache_fetch(apc_user_cache, Z_STR_P(hentry), t, &iresult)) {
					    add_assoc_zval(&result, Z_STRVAL_P(hentry), &result_entry);
					}
			    } else {
					apc_warning("apc_fetch() expects a string or array of strings.");
				}

			    zend_hash_move_forward_ex(Z_ARRVAL_P(key), &hpos);
			}

			RETVAL_ZVAL(&result, 0, 1);

			if (success) {
				ZVAL_TRUE(success);
			}
		}
	} else { 
		apc_warning("apc_fetch() expects a string or array of strings.");
		RETURN_FALSE; 
	}
    return;
}
/* }}} */

/* {{{ proto mixed apc_exists(mixed key)
 */
PHP_FUNCTION(apcu_exists) {
    zval *key;
    time_t t;

    if (!APCG(enabled)) {
		RETURN_FALSE;
	}

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE) {
        return;
    }

    t = apc_time();

    if (Z_TYPE_P(key) != IS_STRING && Z_TYPE_P(key) != IS_ARRAY) {
        convert_to_string(key);
    }

    if (Z_TYPE_P(key) == IS_STRING) {
        if (Z_STRLEN_P(key)) {
		    if (apc_cache_exists(apc_user_cache, Z_STR_P(key), t)) {
		        RETURN_TRUE;
		    } else {
				RETURN_FALSE;			
			}
		}
    } else if (Z_TYPE_P(key) == IS_ARRAY) {
    	HashPosition hpos;
		zval *hentry;

        array_init(return_value); 

        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(key), &hpos);
        while ((hentry = zend_hash_get_current_data_ex(Z_ARRVAL_P(key), &hpos))) {
            if (Z_TYPE_P(hentry) == IS_STRING) {
               if (apc_cache_exists(apc_user_cache, Z_STR_P(hentry), t)) {
	  			add_assoc_bool(return_value, Z_STRVAL_P(hentry), 1);
	       		}
            } else {
				apc_warning(
					"apc_exists() expects a string or array of strings.");
			}

			/* don't set values we didn't find */
            zend_hash_move_forward_ex(Z_ARRVAL_P(key), &hpos);
        }

		return;
    } else {
        apc_warning("apc_exists() expects a string or array of strings.");
    }

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto mixed apc_delete(mixed keys)
 */
PHP_FUNCTION(apcu_delete) {
    zval *keys;

    if (!APCG(enabled)) {
		RETURN_FALSE;
	}

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &keys) == FAILURE) {
        return;
    }

    if (Z_TYPE_P(keys) == IS_STRING) {
        if (!Z_STRLEN_P(keys)) {
			RETURN_FALSE;
		}

        if (apc_cache_delete(apc_user_cache, Z_STR_P(keys))) {
            RETURN_TRUE;
        } else {
            RETURN_FALSE;
        }

    } else if (Z_TYPE_P(keys) == IS_ARRAY) {
        HashPosition hpos;
        zval *hentry;

        array_init(return_value);
        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(keys), &hpos);

        while ((hentry = zend_hash_get_current_data_ex(Z_ARRVAL_P(keys), &hpos))) {
            if (Z_TYPE_P(hentry) != IS_STRING) {
                apc_warning("apc_delete() expects a string, array of strings, or APCIterator instance.");
                add_next_index_zval(return_value, hentry);
                Z_ADDREF_P(hentry);
            } else if (apc_cache_delete(apc_user_cache, Z_STR_P(hentry)) != 1) {
                add_next_index_zval(return_value, hentry);
                Z_ADDREF_P(hentry);
            }
            zend_hash_move_forward_ex(Z_ARRVAL_P(keys), &hpos);
        }
    } else if (Z_TYPE_P(keys) == IS_OBJECT) {

        if (apc_iterator_delete(keys)) {
            RETURN_TRUE;
        } else {
            RETURN_FALSE;
        }
    } else {
        apc_warning("apc_delete() expects a string, array of strings, or APCIterator instance.");
    }
}

PHP_FUNCTION(apcu_entry) {
	zval *key = NULL;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	zend_long ttl = 0L;
	zend_long now = apc_time();

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zf|l", &key, &fci, &fcc, &ttl) != SUCCESS) {
		return;
	}
	
	apc_cache_entry(apc_user_cache, key, &fci, &fcc, ttl, now, return_value);	
}
/* }}} */

/* {{{ apcu_functions[] */
zend_function_entry apcu_functions[] = {
    PHP_FE(apcu_cache_info,         arginfo_apcu_cache_info)
    PHP_FE(apcu_clear_cache,        arginfo_apcu_clear_cache)
    PHP_FE(apcu_sma_info,           arginfo_apcu_sma_info)
    PHP_FE(apcu_key_info,           arginfo_apcu_key_info)
    PHP_FE(apcu_enabled,            arginfo_apcu_enabled)
    PHP_FE(apcu_store,              arginfo_apcu_store)
    PHP_FE(apcu_fetch,              arginfo_apcu_fetch)
    PHP_FE(apcu_delete,             arginfo_apcu_delete)
    PHP_FE(apcu_add,                arginfo_apcu_store)
    PHP_FE(apcu_inc,                arginfo_apcu_inc)
    PHP_FE(apcu_dec,                arginfo_apcu_inc)
    PHP_FE(apcu_cas,                arginfo_apcu_cas)
    PHP_FE(apcu_exists,             arginfo_apcu_exists)
	PHP_FE(apcu_entry,				arginfo_apcu_entry)
    PHP_FE_END
};
/* }}} */

/* {{{ module definition structure */

zend_module_entry apcu_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_APCU_EXTNAME,
    apcu_functions,
    PHP_MINIT(apcu),
    PHP_MSHUTDOWN(apcu),
    PHP_RINIT(apcu),
    NULL,
    PHP_MINFO(apcu),
    PHP_APCU_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_APCU
ZEND_GET_MODULE(apcu)
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
