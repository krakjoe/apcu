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
#include "apc_bin.h"
#include "php_globals.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "ext/standard/flock_compat.h"
#include "ext/standard/md5.h"
#include "ext/standard/php_var.h"

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

PHP_FUNCTION(apcu_bin_dump);
PHP_FUNCTION(apcu_bin_load);
PHP_FUNCTION(apcu_bin_dumpfile);
PHP_FUNCTION(apcu_bin_loadfile);

#ifdef APC_FULL_BC
PHP_FUNCTION(apc_bin_dumpfile);
PHP_FUNCTION(apc_bin_dump);
#endif
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
    long s = zend_atol(new_value->val, new_value->len);

    if (s <= 0) {
        return FAILURE;
    }

    if (s < 1048576L) {
        /* if it's less than 1Mb, they are probably using the old syntax */
        php_error_docref(	
			NULL, E_WARNING, "apc.shm_size now uses M/G suffixes, please update your ini files");
        s = s * 1048576L;
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

/* {{{ PHP_MINFO_FUNCTION(apcu) */
static PHP_MINFO_FUNCTION(apcu)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "APCu Support", APCG(enabled) ? "Enabled" : "Disabled");
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

    php_info_print_table_row(2, "Revision", "$Revision: 328290 $");
    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}
/* }}} */

#ifdef APC_FULL_BC
static void apc_init(INIT_FUNC_ARGS);
#endif

/* {{{ PHP_MINIT_FUNCTION(apcu) */
static PHP_MINIT_FUNCTION(apcu)
{
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

/* XXX pack this into macros when there are more hooks to handle */
#if defined(PHP_WIN32) && defined(_WIN64)
			do {
				 char buf[65];

				 if (!_i64toa_s((__int64)_apc_register_serializer, buf, 65, 10)) {
					REGISTER_STRING_CONSTANT(APC_SERIALIZER_CONSTANT, buf, CONST_PERSISTENT | CONST_CS);
				 } else {
					/* subsequent apc_register_serializer() calls will be void */
					php_error_docref(NULL, E_WARNING, "Serializer hook init failed");
				 }
			} while (0);
#else
			REGISTER_LONG_CONSTANT(APC_SERIALIZER_CONSTANT, (long)&_apc_register_serializer, CONST_PERSISTENT | CONST_CS);
#endif

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

			/* initialize iterator object */
            apc_iterator_init(module_number);
        }

        REGISTER_LONG_CONSTANT("APC_BIN_VERIFY_MD5", APC_BIN_VERIFY_MD5, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("APC_BIN_VERIFY_CRC32", APC_BIN_VERIFY_CRC32, CONST_CS | CONST_PERSISTENT);
    }

#ifndef REGISTER_BOOL_CONSTANT
    {
        zend_constant apc_bc;
        Z_TYPE(apc_bc.value) = IS_BOOL;
#if defined(APC_FULL_BC) && APC_FULL_BC
        Z_LVAL(apc_bc.value) = 1;
#else
        Z_LVAL(apc_bc.value) = 0;
#endif
        apc_bc.flags = (CONST_CS | CONST_PERSISTENT);
        apc_bc.name = zend_strndup(ZEND_STRL("APCU_APC_FULL_BC"));
        apc_bc.name_len = sizeof("APCU_APC_FULL_BC");
        apc_bc.module_number = module_number;
        zend_register_constant(&apc_bc);
    }
#else
#if defined(APC_FULL_BC) && APC_FULL_BC
    REGISTER_BOOL_CONSTANT("APCU_APC_FULL_BC", 1, CONST_CS | CONST_PERSISTENT);
#else
    REGISTER_BOOL_CONSTANT("APCU_APC_FULL_BC", 0, CONST_CS | CONST_PERSISTENT);
#endif
#endif

#ifdef APC_FULL_BC
	apc_init(INIT_FUNC_ARGS_PASSTHRU);
#endif

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

#ifdef ZTS
    ts_free_id(apcu_globals_id);
#endif

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

#ifdef APC_FULL_BC
/* {{{ proto void apc_clear_cache([string cache]) */
PHP_FUNCTION(apcu_clear_cache)
{
    char *ignored;
    int ignlen = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &ignored, &ignlen) == FAILURE) {
        return;
    }

    if (0 == ignlen || APC_CACHE_IS_USER(ignored, ignlen)) {
        apc_cache_clear(apc_user_cache);
    }

    RETURN_TRUE;
}
/* }}} */

/* {{{ proto array apc_cache_info(string cache_type, [bool limited]) */
PHP_FUNCTION(apcu_cache_info)
{
    zval info;
    zend_bool limited = 0;
    char *ct;
    ulong ctlen;
    
    if (ZEND_NUM_ARGS()) {
        if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|b", &ct, &ctlen, &limited) == FAILURE) {
            return;
        }
    }

    info = apc_cache_info(apc_user_cache, limited);

    if (!Z_ISARRAY(info)) {
        php_error_docref(NULL, E_WARNING, "No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file");
        RETURN_FALSE;
    }

    RETURN_ZVAL(&info, 0, 0);
}
/* }}} */
#else
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
    zval* info;
    zend_bool limited = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &limited) == FAILURE) {
        return;
    }

    info = apc_cache_info(apc_user_cache, limited);

    if (!info) {
        php_error_docref(NULL, E_WARNING, "No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file");
        RETURN_FALSE;
    }

    RETURN_ZVAL(info, 0, 1);

}
/* }}} */
#endif

PHP_FUNCTION(apcu_key_info)
{
    zend_string *key;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &key) == FAILURE) {
        return;
    }
    
    apc_cache_stat(apc_user_cache, key, return_value);
}

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

    HANDLE_BLOCK_INTERRUPTIONS();
    
    if (!apc_cache_update(apc_user_cache, key, updater, data)) {
        HANDLE_UNBLOCK_INTERRUPTIONS();
        return 0;
    }

    HANDLE_UNBLOCK_INTERRUPTIONS();

    return 1;
}
/* }}} */

/* {{{ apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const zend_bool exclusive)
 */
static void apc_store_helper(INTERNAL_FUNCTION_PARAMETERS, const zend_bool exclusive)
{
    zval *key = NULL;
    zval *val = NULL;
    long ttl = 0L;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|zl", &key, &val, &ttl) == FAILURE) {
        return;
    }

    if (!key || !APCG(enabled)) {
        /* cannot work without key */
        RETURN_FALSE;
    }

    HANDLE_BLOCK_INTERRUPTIONS();
    
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
    	            HANDLE_UNBLOCK_INTERRUPTIONS();
    	            RETURN_FALSE;
    	        }
                /* return true on success */
    			if(apc_cache_store(apc_user_cache, Z_STR_P(key), val, (uint32_t) ttl, exclusive)) {
			        HANDLE_UNBLOCK_INTERRUPTIONS();
    	            RETURN_TRUE;
                }
    		} else {
                apc_warning("apc_store expects key parameter to be a string or an array of key/value pairs.");
    		}
        }
	}
	
	HANDLE_UNBLOCK_INTERRUPTIONS();
	
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
    long step;
    long lval;
};

static zend_bool php_inc_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {
    struct php_inc_updater_args *args = (struct php_inc_updater_args*) data;

    if (Z_TYPE(entry->val) == IS_LONG) {
        Z_LVAL(entry->val) += args->step;
        args->lval = Z_LVAL(entry->val);
        return 1;
    }

    return 0;
}
/* }}} */

/* {{{ proto long apc_inc(string key [, long step [, bool& success]])
 */
PHP_FUNCTION(apcu_inc) {
    zend_string *key;
    struct php_inc_updater_args args = {1L, -1};
    zval *success = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|lz", &key, &(args.step), &success) == FAILURE) {
        return;
    }
    
	if (success) {
		zval_dtor(success);
	}

    if (php_apc_update(key, php_inc_updater, &args)) {
        if (success) {
			ZVAL_TRUE(success);
		}
        RETURN_LONG(args.lval);
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
    struct php_inc_updater_args args = {1L, -1};
    zval *success = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|lz", &key, &(args.step), &success) == FAILURE) {
        return;
    }
    
	if (success) {
		zval_dtor(success);
	}

    args.step = args.step * -1;

    if (php_apc_update(key, php_inc_updater, &args)) {
        if (success) ZVAL_TRUE(success);
        RETURN_LONG(args.lval);
    }
    
    if (success) {
		ZVAL_FALSE(success);
	}
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ php_cas_updater */
static zend_bool php_cas_updater(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {
    long* vals = ((long*)data);
    long old = vals[0];
    long new = vals[1];

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
    long vals[2];

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
    apc_cache_entry_t* entry;
    time_t t;
    apc_context_t ctxt = {0,};

    if (!APCG(enabled)) {
		RETURN_FALSE;
	}

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &key, &success) == FAILURE) {
        return;
    }

    t = apc_time();

    if (success) {
		ZVAL_DEREF(success);
        ZVAL_FALSE(success);
    }

	if (Z_TYPE_P(key) != IS_STRING && Z_TYPE_P(key) != IS_ARRAY) {
	    convert_to_string(key);
	}
	
	/* check for a string, or array of strings */
	if (Z_TYPE_P(key) == IS_ARRAY || (Z_TYPE_P(key) == IS_STRING && Z_STRLEN_P(key) > 0)) {
		
		/* initialize a context */
		if (apc_cache_make_context(apc_user_cache, &ctxt, APC_CONTEXT_NOSHARE, APC_UNPOOL, APC_COPY_OUT, 0)) {
			if (Z_TYPE_P(key) == IS_STRING) {
				/* do find using string as key */
				if ((entry = apc_cache_find(apc_user_cache, Z_STR_P(key), t))) {
				    /* deep-copy returned shm zval to return_value on stack */
				    apc_cache_fetch_zval(&ctxt, return_value, &entry->val);
					/* decrement refcount of entry */
				    apc_cache_release(apc_user_cache, entry);
					/* set success */
					if (success) {
						ZVAL_TRUE(success);
					}
				} else { ZVAL_BOOL(return_value, 0); }

			} else if (Z_TYPE_P(key) == IS_ARRAY) {

				/* do find using key as array of strings */
				HashPosition hpos;
				zval *hentry;
				zval result;
                
				array_init(&result);
				
				zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(key), &hpos);
				while((hentry = zend_hash_get_current_data_ex(Z_ARRVAL_P(key), &hpos))) {

				    if (Z_TYPE_P(hentry) == IS_STRING) {

				        /* perform find using this index as key */
						if ((entry = apc_cache_find(apc_user_cache, Z_STR_P(hentry), t))) {
						    zval result_entry;

						    /* deep-copy returned shm zval to emalloc'ed return_value */
						    apc_cache_fetch_zval(
								&ctxt, &result_entry, &entry->val);
							/* decrement refcount of entry */
						    apc_cache_release(
								apc_user_cache, entry);
							/* add the emalloced value to return array */
						    add_assoc_zval(&result, Z_STRVAL_P(hentry), &result_entry);
						}
				    } else {

						/* we do not break loop, we just skip the key */
						apc_warning(
							"apc_fetch() expects a string or array of strings.");
					}

					/* don't set values we didn't find */
				    zend_hash_move_forward_ex(Z_ARRVAL_P(key), &hpos);
				}

				RETVAL_ZVAL(&result, 0, 1);

				if (success) {
					ZVAL_TRUE(success);
				}
			}

			apc_cache_destroy_context(&ctxt );	
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
/* }}} */

/* {{{ proto mixed apcu_bin_dump([array vars])
    Returns a binary dump of the given user variables from the APC cache.
    A NULL for vars signals a dump of every entry, while array() will dump nothing.
 */
PHP_FUNCTION(apcu_bin_dump) {

    zval *z_vars = NULL;
    HashTable  *h_vars;
    apc_bd_t *bd;

    if (!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_dump not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a!", &z_vars) == FAILURE) {
        return;
    }

    h_vars = z_vars ? Z_ARRVAL_P(z_vars) : NULL;
    bd = apc_bin_dump(apc_user_cache, h_vars);
    if (bd) {
        RETVAL_STRINGL((char*)bd, bd->size-1);
    } else {
        apc_error("Unknown error encountered during apc_bin_dump.");
        RETVAL_NULL();
    }

    return;
}
/* }}} */

#ifdef APC_FULL_BC
/* {{{ proto mixed apc_bin_dump([array files [, array user_vars]])
    Compatibility mode for old APC
 */
PHP_FUNCTION(apc_bin_dump) {

    zval *z_files = NULL, *z_user_vars = NULL;
    HashTable *h_user_vars;
    apc_bd_t *bd;

    if(!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_dump not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a!a!", &z_files, &z_user_vars) == FAILURE) {
        return;
    }

    h_user_vars = z_user_vars ? Z_ARRVAL_P(z_user_vars) : NULL;
    bd = apc_bin_dump(apc_user_cache, h_user_vars);
    if(bd) {
        RETVAL_STRINGL((char*)bd, bd->size-1);
    } else {
        apc_error("Unknown error encountered during apc_bin_dump.");
        RETVAL_NULL();
    }

    return;
}
/* }}} */
#endif

/* {{{ proto mixed apcu_bin_dumpfile(array vars, string filename, [int flags [, resource context]])
    Output a binary dump of the given user variables from the APC cache to the named file.
 */
PHP_FUNCTION(apcu_bin_dumpfile) {

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

    if (!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_dumpfile not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "a!s|lr!", &z_vars, &filename, &filename_len, &flags, &zcontext) == FAILURE) {
        return;
    }

    if (!filename_len) {
        apc_error("apc_bin_dumpfile filename argument must be a valid filename.");
        RETURN_FALSE;
    }

    h_vars = z_vars ? Z_ARRVAL_P(z_vars) : NULL;
    bd = apc_bin_dump(apc_user_cache, h_vars);
    if (!bd) {
        apc_error("Unknown error encountered during apc_bin_dumpfile.");
        RETURN_FALSE;
    }


    /* Most of the following has been taken from the file_get/put_contents functions */

    context = php_stream_context_from_zval(zcontext, flags & PHP_FILE_NO_DEFAULT_CONTEXT);
    stream = php_stream_open_wrapper_ex(filename, (flags & PHP_FILE_APPEND) ? "ab" : "wb",
                                            REPORT_ERRORS, NULL, context);
    if (stream == NULL) {
        efree(bd);
        apc_error("Unable to write to file in apc_bin_dumpfile.");
        RETURN_FALSE;
    }

    if (flags & LOCK_EX && php_stream_lock(stream, LOCK_EX)) {
        php_stream_close(stream);
        efree(bd);
        apc_error("Unable to get a lock on file in apc_bin_dumpfile.");
        RETURN_FALSE;
    }

    numbytes = php_stream_write(stream, (char*)bd, bd->size);
    if (numbytes != bd->size) {
        numbytes = -1;
    }

    php_stream_close(stream);
    efree(bd);

    if (numbytes < 0) {
        apc_error("Only %d of %d bytes written, possibly out of free disk space", numbytes, bd->size);
        RETURN_FALSE;
    }

    RETURN_LONG(numbytes);
}
/* }}} */

#ifdef APC_FULL_BC
/* {{{ proto mixed apc_bin_dumpfile(array files, array user_vars, string filename, [int flags [, resource context]])
    Compatibility mode for old APC
 */
PHP_FUNCTION(apc_bin_dumpfile) {

    zval *z_files = NULL, *z_user_vars = NULL;
    HashTable *h_user_vars;
    char *filename = NULL;
    int filename_len;
    long flags=0;
    zval *zcontext = NULL;
    php_stream_context *context = NULL;
    php_stream *stream;
    int numbytes = 0;
    apc_bd_t *bd;

    if(!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_dumpfile not available.");
        RETURN_FALSE;
    }


    if (zend_parse_parameters(ZEND_NUM_ARGS(), "a!a!s|lr!", &z_files, &z_user_vars, &filename, &filename_len, &flags, &zcontext) == 
FAILURE) {
        return;
    }

    if(!filename_len) {
        apc_error("apc_bin_dumpfile filename argument must be a valid filename.");
        RETURN_FALSE;
    }

    h_user_vars = z_user_vars ? Z_ARRVAL_P(z_user_vars) : NULL;
    bd = apc_bin_dump(apc_user_cache, h_user_vars);
    if(!bd) {
        apc_error("Unknown error encountered during apc_bin_dumpfile.");
        RETURN_FALSE;
    }


    /* Most of the following has been taken from the file_get/put_contents functions */

    context = php_stream_context_from_zval(zcontext, flags & PHP_FILE_NO_DEFAULT_CONTEXT);
    stream = php_stream_open_wrapper_ex(filename, (flags & PHP_FILE_APPEND) ? "ab" : "wb",
                                            REPORT_ERRORS, NULL, context);
    if (stream == NULL) {
        efree(bd);
        apc_error("Unable to write to file in apc_bin_dumpfile.");
        RETURN_FALSE;
    }

    if (flags & LOCK_EX && php_stream_lock(stream, LOCK_EX)) {
        php_stream_close(stream);
        efree(bd);
        apc_error("Unable to get a lock on file in apc_bin_dumpfile.");
        RETURN_FALSE;
    }

    numbytes = php_stream_write(stream, (char*)bd, bd->size);
    if(numbytes != bd->size) {
        numbytes = -1;
    }

    php_stream_close(stream);
    efree(bd);

    if(numbytes < 0) {
        apc_error("Only %d of %d bytes written, possibly out of free disk space", numbytes, bd->size);
        RETURN_FALSE;
    }

    RETURN_LONG(numbytes);
}
/* }}} */
#endif

/* {{{ proto mixed apcu_bin_load(string data, [int flags])
    Load the given binary dump into the APC file/user cache.
 */
PHP_FUNCTION(apcu_bin_load) {

    int data_len;
    char *data;
    long flags = 0;

    if (!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_load not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &data, &data_len, &flags) == FAILURE) {
        return;
    }

    if (!data_len || data_len != ((apc_bd_t*)data)->size -1) {
        apc_error("apc_bin_load string argument does not appear to be a valid APC binary dump due to size (%d vs expected %d).", data_len, 
((apc_bd_t*)data)->size -1);
        RETURN_FALSE;
    }

    apc_bin_load(apc_user_cache, (apc_bd_t*)data, (int)flags);

    RETURN_TRUE;
}
/* }}} */

/* {{{ proto mixed apc_bin_loadfile(string filename, [resource context, [int flags]])
    Load the given binary dump from the named file into the APC file/user cache.
 */
PHP_FUNCTION(apcu_bin_loadfile) {

    char *filename;
    int filename_len;
    zval *zcontext = NULL;
    long flags = 0;
    php_stream_context *context = NULL;
    php_stream *stream;
    zend_string *data;

    if (!APCG(enabled)) {
        apc_warning("APC is not enabled, apc_bin_loadfile not available.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|r!l", &filename, &filename_len, &zcontext, &flags) == FAILURE) {
        return;
    }

    if (!filename_len) {
        apc_error("apc_bin_loadfile filename argument must be a valid filename.");
        RETURN_FALSE;
    }

    context = php_stream_context_from_zval(zcontext, 0);
    stream = php_stream_open_wrapper_ex(filename, "rb",
            REPORT_ERRORS, NULL, context);
    if (!stream) {
        apc_error("Unable to read from file in apc_bin_loadfile.");
        RETURN_FALSE;
    }

    data = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
    if (data->len == 0) {
        apc_warning("File passed to apc_bin_loadfile was empty: %s.", filename);
        RETURN_FALSE;
    } else if (data->len < 0) {
        apc_warning("Error reading file passed to apc_bin_loadfile: %s.", filename);
        RETURN_FALSE;
    } else if (data->len != ((apc_bd_t*)data)->size) {
        apc_warning("file passed to apc_bin_loadfile does not appear to be valid due to size (%d vs expected %d).", data->len, 
((apc_bd_t*)data)->size -1);
        RETURN_FALSE;
    }
    php_stream_close(stream);

    apc_bin_load(apc_user_cache, (apc_bd_t*)data->val, (int)flags);
    zend_string_release(data);

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
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_store, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, var)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_enabled, 0, 0, 0)
ZEND_END_ARG_INFO()

#ifdef APC_FULL_BC
PHP_APC_ARGINFO
/* this will generate different reflection but retains functional compatibility */
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_cache_info, 0, 0, 0)
    ZEND_ARG_INFO(0, type)
    ZEND_ARG_INFO(0, limited)
ZEND_END_ARG_INFO()
PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_clear_cache, 0, 0, 0)
    ZEND_ARG_INFO(0, cache)
ZEND_END_ARG_INFO()
#else
PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_cache_info, 0, 0, 0)
    ZEND_ARG_INFO(0, limited)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_clear_cache, 0, 0, 0)
ZEND_END_ARG_INFO()
#endif

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_key_info, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_sma_info, 0, 0, 0)
    ZEND_ARG_INFO(0, limited)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apcu_delete, 0)
    ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_fetch, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(1, success)
ZEND_END_ARG_INFO()


PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_inc, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, step)
    ZEND_ARG_INFO(1, success)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apcu_cas, 0)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, old)
    ZEND_ARG_INFO(0, new)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO(arginfo_apcu_exists, 0)
    ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_bin_dump, 0, 0, 0)
    ZEND_ARG_INFO(0, user_vars)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_bin_dumpfile, 0, 0, 2)
    ZEND_ARG_INFO(0, user_vars)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, flags)
    ZEND_ARG_INFO(0, context)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_bin_load, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

PHP_APC_ARGINFO
ZEND_BEGIN_ARG_INFO_EX(arginfo_apcu_bin_loadfile, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, context)
    ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()


#ifdef APC_FULL_BC
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
#endif
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
    PHP_FE(apcu_bin_dump,           arginfo_apcu_bin_dump)
    PHP_FE(apcu_bin_load,           arginfo_apcu_bin_load)
    PHP_FE(apcu_bin_dumpfile,       arginfo_apcu_bin_dumpfile)
    PHP_FE(apcu_bin_loadfile,       arginfo_apcu_bin_loadfile)
    {NULL, NULL, NULL}
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

#ifdef APC_FULL_BC

PHP_MINFO_FUNCTION(apc)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "APC support", "Emulated");
	php_info_print_table_end();
}

/* {{{ apc_functions[] */
zend_function_entry apc_functions[] = {
    PHP_FALIAS(apc_cache_info,   apcu_cache_info,   arginfo_apcu_cache_info)
    PHP_FALIAS(apc_clear_cache,  apcu_clear_cache,  arginfo_apcu_clear_cache)
    PHP_FALIAS(apc_sma_info,     apcu_sma_info,     arginfo_apcu_sma_info)
    PHP_FALIAS(apc_store,        apcu_store,        arginfo_apcu_store)
    PHP_FALIAS(apc_fetch,        apcu_fetch,        arginfo_apcu_fetch)
    PHP_FALIAS(apc_delete,       apcu_delete,       arginfo_apcu_delete)
    PHP_FALIAS(apc_add,          apcu_add,          arginfo_apcu_store)
    PHP_FALIAS(apc_inc,          apcu_inc,          arginfo_apcu_inc)
    PHP_FALIAS(apc_dec,          apcu_dec,          arginfo_apcu_inc)
    PHP_FALIAS(apc_cas,          apcu_cas,          arginfo_apcu_cas)
    PHP_FALIAS(apc_exists,       apcu_exists,       arginfo_apcu_exists)
    PHP_FE(apc_bin_dump,                            arginfo_apc_bin_dump)
    PHP_FE(apc_bin_dumpfile,                        arginfo_apc_bin_dumpfile)
    PHP_FALIAS(apc_bin_load,     apcu_bin_load,     arginfo_apcu_bin_load)
    PHP_FALIAS(apc_bin_loadfile, apcu_bin_loadfile, arginfo_apcu_bin_loadfile)
    {NULL, NULL, NULL}
};

zend_module_entry apc_module_entry = {
	STANDARD_MODULE_HEADER,
	"apc",
	apc_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	PHP_MINFO(apc),
	PHP_APCU_VERSION,
	STANDARD_MODULE_PROPERTIES,
};

static void apc_init(INIT_FUNC_ARGS)
{
	zend_register_internal_module(&apc_module_entry);
}

#endif


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
