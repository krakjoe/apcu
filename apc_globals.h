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

#ifndef APC_GLOBALS_H
#define APC_GLOBALS_H

#include "apc.h"

ZEND_BEGIN_MODULE_GLOBALS(apcu)
	/* configuration parameters */
	zend_bool enabled;      /* if true, apc is enabled (defaults to true) */
	zend_long shm_segments;      /* number of shared memory segments to use */
	zend_long shm_size;          /* size of each shared memory segment (in MB) */
	zend_long entries_hint;      /* hint at the number of entries expected */
	zend_long gc_ttl;            /* parameter to apc_cache_create */
	zend_long ttl;               /* parameter to apc_cache_create */
	zend_long smart;             /* smart value */

#if APC_MMAP
	char *mmap_file_mask;   /* mktemp-style file-mask to pass to mmap */
#endif

	/* module variables */
	zend_bool initialized;       /* true if module was initialized */
	zend_bool enable_cli;        /* Flag to override turning APC off for CLI */
	zend_bool slam_defense;      /* true for user cache slam defense */

	char *preload_path;          /* preload path */
	zend_bool coredump_unmap;    /* trap signals that coredump and unmap shared memory */
	zend_bool use_request_time;  /* use the SAPI request start time for TTL */
	time_t request_time;         /* cached request time */

	char *serializer_name;       /* the serializer config option */

	/* Nesting level of apcu_entry calls. */
	unsigned int entry_level;
ZEND_END_MODULE_GLOBALS(apcu)

/* (the following is defined in php_apc.c) */
ZEND_EXTERN_MODULE_GLOBALS(apcu)

#ifdef ZTS
# define APCG(v) TSRMG(apcu_globals_id, zend_apcu_globals *, v)
#else
# define APCG(v) (apcu_globals.v)
#endif

extern struct _apc_cache_t* apc_user_cache;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: noexpandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4 sts=4
 */
