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

/* $Id: apc_globals.h 328290 2012-11-09 03:30:09Z laruence $ */

#ifndef APC_GLOBALS_H
#define APC_GLOBALS_H

#include "apc.h"
#include "apc_cache.h"
#include "apc_stack.h"
#include "apc_php.h"

/* {{{ struct apc_rfc1867_data */

typedef struct _apc_rfc1867_data apc_rfc1867_data;

struct _apc_rfc1867_data {
    char tracking_key[64];
    int  key_length;
    size_t content_length;
    char filename[128];
    char name[64];
    char *temp_filename;
    int cancel_upload;
    double start_time;
    size_t bytes_processed;
    size_t prev_bytes_processed;
    int update_freq;
    double rate;
    int started;
};
/* }}} */


ZEND_BEGIN_MODULE_GLOBALS(apcu)
    /* configuration parameters */
    zend_bool enabled;      /* if true, apc is enabled (defaults to true) */
    long shm_segments;      /* number of shared memory segments to use */
    long shm_size;          /* size of each shared memory segment (in MB) */
    long entries_hint;      /* hint at the number of entries expected */
    long gc_ttl;            /* parameter to apc_cache_create */
    long ttl;               /* parameter to apc_cache_create */
	long smart;             /* smart value */

#if APC_MMAP
    char *mmap_file_mask;   /* mktemp-style file-mask to pass to mmap */
#endif

    /* module variables */
    zend_bool initialized;       /* true if module was initialized */
    zend_bool enable_cli;        /* Flag to override turning APC off for CLI */
    zend_bool slam_defense;      /* true for user cache slam defense */ 

#ifdef MULTIPART_EVENT_FORMDATA
    zend_bool rfc1867;            /* Flag to enable rfc1867 handler */
    char* rfc1867_prefix;         /* Key prefix */
    char* rfc1867_name;           /* Name of hidden field to activate upload progress/key suffix */
    double rfc1867_freq;          /* Update frequency as percentage or bytes */
    long rfc1867_ttl;             /* TTL for rfc1867 entries */
    apc_rfc1867_data rfc1867_data;/* Per-request data */
#endif

	void *apc_bd_alloc_ptr;      /* bindump alloc() ptr */
    void *apc_bd_alloc_ubptr;    /* bindump alloc() upper bound ptr */
    HashTable apc_bd_alloc_list; /* bindump alloc() ptr list */
	char *preload_path;          /* preload path */
    zend_bool coredump_unmap;    /* trap signals that coredump and unmap shared memory */
    zend_bool use_request_time;  /* use the SAPI request start time for TTL */

    char *serializer_name;       /* the serializer config option */
    char *writable;              /* writable path for general use */
ZEND_END_MODULE_GLOBALS(apcu)

/* (the following declaration is defined in php_apc.c) */
ZEND_EXTERN_MODULE_GLOBALS(apcu)

#ifdef ZTS
# define APCG(v) TSRMG(apcu_globals_id, zend_apcu_globals *, v)
#else
# define APCG(v) (apcu_globals.v)
#endif

/* True globals */
extern apc_cache_t* apc_user_cache;  /* the global cache */
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
