/*
  +----------------------------------------------------------------------+
  | APCu                                                                 |
  +----------------------------------------------------------------------+
  | Copyright (c) 2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Joe Watkins <joe.watkins@live.co.uk>                        |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_APC_LOCK
#define HAVE_APC_LOCK

#ifndef HAVE_APC_LOCK_H
# include "apc_lock.h"
#endif

#ifndef PHP_WIN32
# ifndef APC_SPIN_LOCK
#   ifndef APC_FCNTL_LOCK
#       ifdef APC_LOCK_RECURSIVE
#           define APC_LOCK_ROBUST 1
#       endif
#   endif
# endif
#endif



#ifdef APC_LOCK_ROBUST

#include "apc_stack.h"

/*
	rwlocks don't support robustness so we have to hack around it
*/
static
	pthread_key_t lock_key;

static
void release_all_locks(apc_stack_t *locks) {
	if (!locks) {
		return;
	}

	if (apc_stack_size(locks) > 0) {
		apc_lock_t *lock;

		while ((lock = (apc_lock_t *) apc_stack_pop(locks)) != NULL) {
			apc_lock_wunlock(lock);
		}
	}
	apc_stack_destroy(locks);
}

static
void release_all_locks_atexit() {
	apc_stack_t *stack = (apc_stack_t *) pthread_getspecific(lock_key);

	release_all_locks(stack);
	pthread_setspecific(lock_key, NULL);
}

static
void make_lock_key() {
	(void) pthread_key_create(&lock_key, release_all_locks);
}

static
void locked(apc_lock_t *lock) {
	apc_stack_t *stack = pthread_getspecific(lock_key);

	if (!stack) {
		stack = apc_stack_create(10);
		atexit(release_all_locks_atexit);
	}
	apc_stack_push(stack, lock);
	pthread_setspecific(lock_key, stack);
}

static
void unlocked(apc_lock_t *lock) {
	apc_stack_t *stack = (apc_stack_t *) pthread_getspecific(lock_key);
	apc_lock_t *stacked;

	if (!stack || apc_stack_size(stack) == 0) {
		return;
	}

	if (apc_stack_top(stack) == lock) {
		apc_stack_pop(stack);
		return;
	}
	else {
		/* are there any unpaired lock/unlocks? that's costly here */
		apc_stack_t *new_stack = apc_stack_create(10);
		apc_lock_t *l;

		while ((l = apc_stack_pop(stack)) != NULL) {
			if (l != lock) {
				apc_stack_push(new_stack, l);
			}
		}
		apc_stack_destroy(stack);
		stack = new_stack;
	}

	assert (stacked == lock);
	pthread_setspecific(lock_key, stack);
}

static
void lock_destroy(apc_lock_t *lock) {
	unlocked(lock);
}

#endif

/*
 APCu never checks the return value of a locking call, it assumes it should not fail
 and execution continues regardless, therefore it is pointless to check the return
 values of calls to locking functions, lets save ourselves the comparison.
*/

/* {{{ There's very little point in initializing a billion sets of attributes */
#ifndef PHP_WIN32
# ifndef APC_SPIN_LOCK
#   ifndef APC_FCNTL_LOCK
#       ifdef APC_LOCK_RECURSIVE
	        static pthread_mutexattr_t apc_lock_attr;
#       else
	        static pthread_rwlockattr_t apc_lock_attr;
#       endif
#   else
#       include <unistd.h>
#       include <fcntl.h>
    
        static int apc_fcntl_call(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
            int ret;
            struct flock lock;

            lock.l_type = type;
            lock.l_start = offset;
            lock.l_whence = whence;
            lock.l_len = len;
            lock.l_pid = 0;

            do { 
                ret = fcntl(fd, cmd, &lock) ; 
            } while(ret < 0 && errno == EINTR);
            
            return(ret);
        }
#   endif
# else
PHP_APCU_API int apc_lock_init(apc_lock_t* lock)
{
    lock->state = 0;
}

PHP_APCU_API int apc_lock_try(apc_lock_t* lock)
{
    int failed = 1;
    
    asm volatile
    (
        "xchgl %0, 0(%1)" :
        "=r" (failed) : "r" (&lock->state), 
        "0" (failed)    
    );
    
    return failed;   
}

PHP_APCU_API int apc_lock_get(apc_lock_t* lock)
{
    int failed = 1;
    
    do {
        failed = apc_lock_try(
            lock);
#ifdef APC_LOCK_NICE
        usleep(0);
#endif
    } while (failed);
    
    return failed;
}

PHP_APCU_API int apc_lock_release(apc_lock_t* lock)
{
    int released = 0;
    
    asm volatile (
        "xchg %0, 0(%1)" : "=r" (released) : "r" (&lock->state),
        "0" (released)
    );
    
    return !released;
}
# endif
static zend_bool apc_lock_ready = 0;
#endif /* }}} */

/* {{{ Initialize the global lock attributes */
PHP_APCU_API zend_bool apc_lock_init() {
#ifndef PHP_WIN32
	if (apc_lock_ready)
		return 1;

	/* once per process please */
	apc_lock_ready = 1;

#ifdef APC_LOCK_ROBUST
	make_lock_key();
#endif

#ifndef APC_SPIN_LOCK
# ifndef APC_FCNTL_LOCK
#   ifdef APC_LOCK_RECURSIVE
	    if (pthread_mutexattr_init(&apc_lock_attr) == SUCCESS) {
		    if (pthread_mutexattr_setpshared(&apc_lock_attr, PTHREAD_PROCESS_SHARED) == SUCCESS) {
			    pthread_mutexattr_settype(&apc_lock_attr, PTHREAD_MUTEX_RECURSIVE);
			    return 1;
		    }
	    }
#   else
	    if (pthread_rwlockattr_init(&apc_lock_attr) == SUCCESS) {
		    if (pthread_rwlockattr_setpshared(&apc_lock_attr, PTHREAD_PROCESS_SHARED) == SUCCESS) {
			    return 1;
		    }
	    }
#   endif
# endif
#endif
	return 0;
#else
	return 1;
#endif
} /* }}} */

/* {{{ Cleanup attributes and statics */
PHP_APCU_API void apc_lock_cleanup() {
#ifndef PHP_WIN32
	if (!apc_lock_ready)
		return;

#ifndef APC_SPIN_LOCK
# ifndef APC_FCNTL_LOCK
#   ifdef APC_LOCK_RECURSIVE
	    pthread_mutexattr_destroy(&apc_lock_attr);
#   else
	    pthread_rwlockattr_destroy(&apc_lock_attr);
#   endif
#   ifdef APC_LOCK_ROBUST
		pthread_key_delete(lock_key);
#   endif
# endif
#endif
#endif
	/* once per process please */
	apc_lock_ready = 0;
} /* }}} */

PHP_APCU_API zend_bool apc_lock_create(apc_lock_t *lock) {
#ifndef PHP_WIN32
# ifndef APC_SPIN_LOCK
#   ifndef APC_FCNTL_LOCK
#       ifdef APC_LOCK_RECURSIVE
	        {
		        pthread_mutex_init(lock, &apc_lock_attr);
		        return 1;
	        }
#       else
	        {
		        /* Native */
		        return (pthread_rwlock_init(lock, &apc_lock_attr)==SUCCESS);
	        }
#       endif
# else
    {
        /* FCNTL */
        char lock_path[] = "/tmp/.apc.XXXXXX";
        mktemp(
            lock_path);
        (*lock) = open(lock_path, O_RDWR|O_CREAT, 0666);
        if((*lock) > 0 ) {
            unlink(
                lock_path);
            return 1;
        } else {
            return 0;
        }
    }
# endif
#else
    {
        /* SPIN */
        lock->state = 0;
        return 1;
    }
    
#endif
#else
	lock = (apc_lock_t *)apc_windows_cs_create((apc_windows_cs_rwlock_t *)lock);

	return (NULL != lock);
#endif
}

PHP_APCU_API zend_bool apc_lock_rlock(apc_lock_t *lock) {
#ifndef PHP_WIN32
#ifndef APC_SPIN_LOCK
# ifndef APC_FCNTL_LOCK
#   ifdef APC_LOCK_RECURSIVE
	    pthread_mutex_lock(lock);
#   else
	    pthread_rwlock_rdlock(lock);
#   endif

#   ifdef APC_LOCK_ROBUST
	locked(lock);
#   endif

# else
    {
        /* FCNTL */
        apc_fcntl_call((*lock), F_SETLKW, F_RDLCK, 0, SEEK_SET, 0);
    }
# endif
#else
    {
        /* SPIN */
        apc_lock_get(lock);
    }
#endif
#else
	apc_windows_cs_rdlock((apc_windows_cs_rwlock_t *)lock);
#endif
	return 1;
}

PHP_APCU_API zend_bool apc_lock_wlock(apc_lock_t *lock) {
#ifndef PHP_WIN32
#ifndef APC_SPIN_LOCK
# ifndef APC_FCNTL_LOCK
#   ifdef APC_LOCK_RECURSIVE
	    pthread_mutex_lock(lock);
#   else	
	    pthread_rwlock_wrlock(lock);
#   endif

#   ifdef APC_LOCK_ROBUST
	locked(lock);
#   endif

# else
    {
        /* FCNTL */
        apc_fcntl_call((*lock), F_SETLKW, F_WRLCK, 0, SEEK_SET, 0);
    }
# endif
#else
    {
        /* SPIN */
        apc_lock_get(lock);
    }
#endif
#else
	apc_windows_cs_lock((apc_windows_cs_rwlock_t *)lock);
#endif
	return 1;
}

PHP_APCU_API zend_bool apc_lock_wunlock(apc_lock_t *lock) {
#ifndef PHP_WIN32
#ifndef APC_SPIN_LOCK
# ifndef APC_FCNTL_LOCK
#   ifdef APC_LOCK_RECURSIVE
	    pthread_mutex_unlock(lock);
#   else
	    pthread_rwlock_unlock(lock);
#   endif

#   ifdef APC_LOCK_ROBUST
	unlocked(lock);
#   endif

# else
    {
        /* FCNTL */
        apc_fcntl_call((*lock), F_SETLKW, F_UNLCK, 0, SEEK_SET, 0);
    }
# endif
#else
    {
        /* SPIN */
        apc_lock_release(lock);
    }
#endif
#else
	apc_windows_cs_unlock_wr((apc_windows_cs_rwlock_t *)lock);
#endif
	return 1;
}

PHP_APCU_API zend_bool apc_lock_runlock(apc_lock_t *lock) {
#ifndef PHP_WIN32
#ifndef APC_SPIN_LOCK
# ifndef APC_FCNTL_LOCK
#   ifdef APC_LOCK_RECURSIVE
	    pthread_mutex_unlock(lock);
#   else
	    pthread_rwlock_unlock(lock);
#   endif

#   ifdef APC_LOCK_ROBUST
	unlocked(lock);
#   endif


# else
    {
        /* FCNTL */
        apc_fcntl_call((*lock), F_SETLKW, F_UNLCK, 0, SEEK_SET, 0);
    }
# endif
#else
    {
        /* SPIN */
        apc_lock_release(lock);
    }
#endif
#else
	apc_windows_cs_unlock_rd((apc_windows_cs_rwlock_t *)lock);
#endif
	return 1;
}

PHP_APCU_API void apc_lock_destroy(apc_lock_t *lock) {
#ifndef PHP_WIN32
#ifndef APC_SPIN_LOCK
# ifndef APC_FCNTL_LOCK
#   ifdef APC_LOCK_RECURSIVE
	    /* nothing */
#   else
        /* nothing */
#   endif

#   ifdef APC_LOCK_ROBUST
	lock_destroy(lock);
#   endif

# else
    {
        /* FCNTL */
        close ((*lock));
    }
# endif
#endif
#else
	apc_windows_cs_destroy((apc_windows_cs_rwlock_t *)lock);
#endif
} 
#endif

