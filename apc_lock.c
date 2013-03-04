#ifndef HAVE_APC_LOCK
#define HAVE_APC_LOCK

#ifndef HAVE_APC_LOCK_H
# include "apc_lock.h"
#endif

/*
 APC locks are a generic implementation of a read/write lock using the most suitable primitives available on the operating system
*/
int apc_lock_create(apc_lock_t *lock TSRMLS_DC) {
	#ifndef _WIN32
	{
		/*
		 Initialize attributes suitable for APC locks to function in all environments
		 Locks must be suitable for sharing among processes and contain a read and write mutex
		*/
		pthread_mutexattr_t attr;
		
		switch (pthread_mutexattr_init(&attr)) {
			case SUCCESS: switch(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)) {
				case SUCCESS: {
					pthread_mutex_init(&lock->read, &attr);
					pthread_mutex_init(&lock->write, &attr);
					pthread_mutexattr_destroy(
						&attr);

					return 1;
				}
			}
			
			default:
				apc_error("pthread mutex error: APC failed to initialize lock attributes." TSRMLS_CC);
		}
	}
#else

#endif	

	return 0;
}

int apc_lock_rlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
	int rc = SUCCESS;
	switch ((rc = pthread_mutex_lock(&lock->read))) {
		/* this might seem crazy, it's sound */
		case EDEADLK:
		case SUCCESS:
			return 1;
		
		default:
			apc_error("pthread mutex error: APC failed to acquire read lock (%i).", rc TSRMLS_CC);
	}
#else
	
#endif
	return 0;
}
 
int apc_lock_wlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
	int rc = SUCCESS;
	switch ((rc = pthread_mutex_lock(&lock->read))) {
		case EDEADLK:
		case SUCCESS: switch((rc = pthread_mutex_lock(&lock->write))) {
			case SUCCESS:
			case EDEADLK: {
				return 1;
			} break;
		} break;

		default:
			apc_error("pthread mutex error: APC failed to acquire write lock (%i)", rc TSRMLS_CC);
	}
#else

#endif	
	return 0;
}

int apc_lock_wunlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
	int rc = SUCCESS;
	switch ((rc = pthread_mutex_unlock(&lock->write))) {		
		case SUCCESS: switch((rc = pthread_mutex_unlock(&lock->read))){
			case SUCCESS: {
				return 1;
			} break;
		} break;

		default:
			apc_error("pthread mutex error: APC failed to release write lock (%i)", rc TSRMLS_CC);
	}
#else

#endif
	return 0;
}

int apc_lock_runlock(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
	int rc = SUCCESS;
	switch ((rc = pthread_mutex_unlock(&lock->read))) {
		case SUCCESS: {
			return 1;
		} break;

		default:
			apc_error("pthread mutex error: APC failed to release read lock (%i)", rc TSRMLS_CC);
	}
#else

#endif
	return 0;
}

void apc_lock_destroy(apc_lock_t *lock TSRMLS_DC) {
#ifndef _WIN32
	/* this should be checked, and will be */
	//pthread_mutex_destroy(&lock->read);
	//pthread_mutex_destroy(&lock->write);
#else
	
#endif
} 
#endif

void apc_lock_test(TSRMLS_DC) {
#ifndef _WIN32

#else

#endif
}
