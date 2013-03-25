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
  | Authors: Lucas Nealan <lucas@php.net>                                |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Facebook Inc. in 2007.

   Future revisions and derivatives of this source code must acknowledge
   Facebook Inc. as the original contributor of this module by leaving
   this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.
 */

 /* $Id: apc_signal.c 327066 2012-08-12 07:48:48Z laruence $ */

 /* Allows apc to install signal handlers and maintain signalling
    to already registered handlers. Registers all signals that
    coredump by default and unmaps the shared memory segment
    before the coredump. Note: PHP module init is called before 
    signals are set by Apache and thus apc_set_signals should
    be called in request init (RINIT)
  */

#include "apc.h"

#if HAVE_SIGACTION
#include <signal.h>
#include "apc_globals.h"
#include "apc_sma.h"
#include "apc_signal.h"

static apc_signal_info_t apc_signal_info = {0};

static int apc_register_signal(int signo, void (*handler)(int, siginfo_t*, void*) TSRMLS_DC);
static void apc_rehandle_signal(int signo, siginfo_t *siginfo, void *context);
static void apc_core_unmap(int signo, siginfo_t *siginfo, void *context);
#if defined(SIGUSR1) && defined(APC_SIGNAL_CLEAR)
static void apc_clear_cache(int signo, siginfo_t *siginfo, void *context);
#endif

extern apc_cache_t* apc_user_cache;

/* {{{ apc_core_unmap 
 *  Coredump signal handler, unmaps shm and calls previously installed handlers 
 */
static void apc_core_unmap(int signo, siginfo_t *siginfo, void *context) 
{
    TSRMLS_FETCH();
	
    apc_sma_cleanup(TSRMLS_C);
    apc_rehandle_signal(signo, siginfo, context);

#if !defined(WIN32) && !defined(NETWARE)
    kill(getpid(), signo);
#else
    raise(signo);
#endif
} /* }}} */


#if defined(SIGUSR1) && defined(APC_SIGNAL_CLEAR)
/* {{{ apc_reload_cache */
static void apc_clear_cache(int signo, siginfo_t *siginfo, void *context) {
	TSRMLS_FETCH();
	
	if (apc_user_cache) {	
		apc_cache_clear(
			apc_user_cache TSRMLS_CC);
	}
	
	apc_rehandle_signal(signo, siginfo, context);
	
#if !defined(WIN32) && !defined(NETWARE)
    kill(getpid(), signo);
#else
    raise(signo);
#endif
} /* }}} */
#endif

/* {{{ apc_rehandle_signal
 *  Call the previously registered handler for a signal
 */
static void apc_rehandle_signal(int signo, siginfo_t *siginfo, void *context)
{
    int i;
    apc_signal_entry_t p_sig = {0};

    for (i=0;  (i < apc_signal_info.installed && p_sig.signo != signo);  i++) {
        p_sig = *apc_signal_info.prev[i];
        if (p_sig.signo == signo) {
            if (p_sig.siginfo) {
                (*(void (*)(int, siginfo_t*, void*))p_sig.handler)(signo, siginfo, context);
            } else {
                (*(void (*)(int))p_sig.handler)(signo);
            }
        }
    }

} /* }}} */

/* {{{ apc_register_signal
 *  Set a handler for a previously installed signal and save so we can 
 *  callback when handled 
 */
static int apc_register_signal(int signo, void (*handler)(int, siginfo_t*, void*) TSRMLS_DC)
{
    struct sigaction sa = {{0}};
    apc_signal_entry_t p_sig = {0};

    if (sigaction(signo, NULL, &sa) == 0) {
        if ((void*)sa.sa_handler == (void*)handler) {
            return SUCCESS;
        }

        if (sa.sa_handler != SIG_ERR && sa.sa_handler != SIG_DFL && sa.sa_handler != SIG_IGN) {
            p_sig.signo = signo;
            p_sig.siginfo = ((sa.sa_flags & SA_SIGINFO) == SA_SIGINFO);
            p_sig.handler = (void *)sa.sa_handler;

            apc_signal_info.prev = (apc_signal_entry_t **)apc_erealloc(apc_signal_info.prev, (apc_signal_info.installed+1)*sizeof(apc_signal_entry_t *) TSRMLS_CC);
            apc_signal_info.prev[apc_signal_info.installed] = (apc_signal_entry_t *)apc_emalloc(sizeof(apc_signal_entry_t) TSRMLS_CC);
            *apc_signal_info.prev[apc_signal_info.installed++] = p_sig;
        } else {
            /* inherit flags and mask if already set */
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sa.sa_flags |= SA_SIGINFO; /* we'll use a siginfo handler */
#if defined(SA_ONESHOT)
            sa.sa_flags = SA_ONESHOT;
#elif defined(SA_RESETHAND)
            sa.sa_flags = SA_RESETHAND;
#endif
        }
        sa.sa_handler = (void*)handler;

        if (sigaction(signo, &sa, NULL) < 0) {
            apc_warning("Error installing apc signal handler for %d" TSRMLS_CC, signo);
        }

        return SUCCESS;
    }
    return FAILURE;
} /* }}} */

/* {{{ apc_set_signals
 *  Install our signal handlers */
void apc_set_signals(TSRMLS_D) 
{
	if (apc_signal_info.installed == 0) {
#if defined(SIGUSR1) && defined(APC_SIGNAL_CLEAR)
		apc_register_signal(SIGUSR1, apc_clear_cache TSRMLS_CC);
#endif
		if (APCG(coredump_unmap)) {
		    /* ISO C standard signals that coredump */
		    apc_register_signal(SIGSEGV, apc_core_unmap TSRMLS_CC);
		    apc_register_signal(SIGABRT, apc_core_unmap TSRMLS_CC);
		    apc_register_signal(SIGFPE, apc_core_unmap TSRMLS_CC);
		    apc_register_signal(SIGILL, apc_core_unmap TSRMLS_CC);
/* extended signals that coredump */
#ifdef SIGBUS
			apc_register_signal(SIGBUS, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGABORT
			apc_register_signal(SIGABORT, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGEMT
			apc_register_signal(SIGEMT, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGIOT
			apc_register_signal(SIGIOT, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGQUIT
			apc_register_signal(SIGQUIT, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGSYS
			apc_register_signal(SIGSYS, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGTRAP
			apc_register_signal(SIGTRAP, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGXCPU
			apc_register_signal(SIGXCPU, apc_core_unmap TSRMLS_CC);
#endif
#ifdef SIGXFSZ
			apc_register_signal(SIGXFSZ, apc_core_unmap TSRMLS_CC);
#endif
    	}
	}
} /* }}} */

/* {{{ apc_set_signals
 *  cleanup signals for shutdown */
void apc_shutdown_signals(TSRMLS_D) 
{
    int i=0;
    if (apc_signal_info.installed > 0) {
        for (i=0;  (i < apc_signal_info.installed);  i++) {
            apc_efree(apc_signal_info.prev[i] TSRMLS_CC);
        }
        apc_efree(apc_signal_info.prev TSRMLS_CC);
        apc_signal_info.installed = 0; /* just in case */
	}
}
/* }}} */

#endif  /* HAVE_SIGACTION */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
