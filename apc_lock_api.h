/*
  +----------------------------------------------------------------------+
  | APCu                                                                 |
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

#ifndef APC_LOCK_API_H
#define APC_LOCK_API_H

#ifndef _WIN32
# include "pthread.h"
typedef struct _apc_lock_t {
	pthread_mutex_t read;
	pthread_mutex_t write;
} apc_lock_t;
#else

#endif

/* {{{ functions */
extern int apc_lock_create(apc_lock_t *lock TSRMLS_DC);
extern int apc_lock_rlock(apc_lock_t *lock TSRMLS_DC);
extern int apc_lock_wlock(apc_lock_t *lock TSRMLS_DC);
extern int apc_lock_runlock(apc_lock_t *lock TSRMLS_DC);
extern int apc_lock_wunlock(apc_lock_t *lock TSRMLS_DC);
extern void apc_lock_destroy(apc_lock_t *lock TSRMLS_DC); /* }}} */

/* {{{ generic locking macros */
#define CREATE_LOCK(lock)     apc_lock_create(lock TSRMLS_CC)
#define DESTROY_LOCK(lock)    apc_lock_destroy(lock TSRMLS_CC)
#define WLOCK(lock)           apc_lock_wlock(lock TSRMLS_CC)
#define LOCK                  WLOCK
#define RLOCK(lock)           apc_lock_rlock(lock TSRMLS_CC)
#define WUNLOCK(lock)         apc_lock_wunlock(lock TSRMLS_CC)
#define UNLOCK                WUNLOCK
#define RUNLOCK(lock)         apc_lock_runlock(lock TSRMLS_CC)
/* }}} */

/* {{{ object locking macros */
#define APC_WLOCK(o)          WLOCK(&(o)->lock)
#define APC_LOCK              APC_WLOCK
#define APC_WUNLOCK(o)        WUNLOCK(&(o)->lock)
#define APC_UNLOCK            APC_WUNLOCK
#define APC_RLOCK(o)          RLOCK(&(o)->lock)
#define APC_RUNLOCK(o)        RUNLOCK(&(o)->lock) /* }}} */

/* {{{ simple atomic operations */
#define APC_ATOM_ADD(o, k, v) do {\
	APC_LOCK(o);\
	o->k += v;\
	APC_UNLOCK(o);\
} while(0)

#define APC_ATOM_SUB(o, k, v) do {\
	APC_LOCK(o);\
	o->k -= v;\
	APC_UNLOCK(o);\
while(0)

#define APC_ATOM_INC(o, k) do {\
	APC_LOCK(o);\
	o->k++;\
	APC_UNLOCK(o);\
} while(0)

#define APC_ATOM_DEC(o, k) do {\
	APC_LOCK(o);\
	o->h--;\
	APC_UNLOCK(o);\
} while(0)

#define APC_ATOM_SET(o, k, v) do {\
	APC_LOCK(o);\
	o->k = v;\
	APC_UNLOCK(o);\
} while(0)  

#define APC_ATOM_ZERO(o, k, s) do {\
	APC_LOCK(o);\
	memset(&o->k, 0, s);\
	APC_UNLOCK(o);\
} while(0) 

#define APC_ATOM_DUP(o, d) do {\
	APC_LOCK(o);\
	memcpy(d, o, sizeof(*o));\
	APC_UNLOCK(o);\
} while(0)

#define APC_ATOM_COPY(o, k, d) do {\
	APC_LOCK(o);\
	memcpy(d, &o->k, sizeof(*d));\
	APC_UNLOCK(o);\
} while(0) /* }}} */

#endif
