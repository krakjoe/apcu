dnl
dnl $Id: config.m4 327593 2012-09-10 11:50:58Z pajoye $
dnl

PHP_ARG_ENABLE(apcu, whether to enable APCu support,
[  --enable-apc           Enable APCu support])

AC_ARG_ENABLE(apcu-debug,
[  --enable-apc-debug     Enable APCu debugging], 
[
  PHP_APCU_DEBUG=$enableval
], 
[
  PHP_APCU_DEBUG=no
])

AC_MSG_CHECKING(whether we should use mmap)
AC_ARG_ENABLE(apcu-mmap,
[  --disable-apcu-mmap
                          Disable mmap support and use IPC shm instead],
[
  PHP_APCU_MMAP=$enableval
  AC_MSG_RESULT($enableval)
], [
  PHP_APCU_MMAP=yes
  AC_MSG_RESULT(yes)
])

AC_MSG_CHECKING(whether we should use semaphore locking instead of fcntl)
AC_ARG_ENABLE(apcu-sem,
[  --enable-apcu-sem
                          Enable semaphore locks instead of fcntl],
[
  PHP_APCU_SEM=$enableval
  AC_MSG_RESULT($enableval)
], [
  PHP_APCU_SEM=no
  AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(whether we should use pthread mutex locking)
AC_ARG_ENABLE(apcu-pthreadmutex,
[  --disable-apcu-pthreadmutex
                          Disable pthread mutex locking ],
[
  PHP_APCU_PTHREADMUTEX=$enableval
  AC_MSG_RESULT($enableval)
],
[
  PHP_APCU_PTHREADMUTEX=yes
  AC_MSG_RESULT(yes)
])

if test "$PHP_APCU_PTHREADMUTEX" != "no"; then
	orig_LIBS="$LIBS"
	LIBS="$LIBS -lpthread"
	AC_TRY_RUN(
			[
				#include <sys/types.h>
				#include <pthread.h>
                                main() {
				pthread_mutex_t mutex;
				pthread_mutexattr_t attr;	

				if(pthread_mutexattr_init(&attr)) { 
					puts("Unable to initialize pthread attributes (pthread_mutexattr_init).");
					return -1; 
				}
				if(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)) { 
					puts("Unable to set PTHREAD_PROCESS_SHARED (pthread_mutexattr_setpshared), your system may not support shared mutex's.");
					return -1; 
				}	
				if(pthread_mutex_init(&mutex, &attr)) { 
					puts("Unable to initialize the mutex (pthread_mutex_init).");
					return -1; 
				}
				if(pthread_mutexattr_destroy(&attr)) { 
					puts("Unable to destroy mutex attributes (pthread_mutexattr_destroy).");
					return -1; 
				}
				if(pthread_mutex_destroy(&mutex)) { 
					puts("Unable to destroy mutex (pthread_mutex_destroy).");
					return -1; 
				}

				puts("pthread mutexs are supported!");
				return 0;
                                }
			],
			[ dnl -Success-
				PHP_ADD_LIBRARY(pthread)
			],
			[ dnl -Failure-
				AC_MSG_WARN([It doesn't appear that pthread mutexes are supported on your system])
  			PHP_APCU_PTHREADMUTEX=no
			],
			[
				PHP_ADD_LIBRARY(pthread)
			]
	)
	LIBS="$orig_LIBS"
fi

AC_MSG_CHECKING(whether we should use pthread read/write locking)
AC_ARG_ENABLE(apcu-pthreadrwlocks,
[  --enable-apcu-pthreadrwlocks
                          Enable pthread read/write locking ],
[
  PHP_APCU_PTHREADRWLOCK=$enableval
  AC_MSG_RESULT($enableval)
],
[
  PHP_APCU_PTHREADRWLOCK=no
  AC_MSG_RESULT(no)
])

if test "$PHP_APCU_PTHREADRWLOCK" != "no"; then
	orig_LIBS="$LIBS"
	LIBS="$LIBS -lpthread"
	AC_TRY_RUN(
			[
				#include <sys/types.h>
				#include <pthread.h>
                                main() {
				pthread_rwlock_t rwlock;
				pthread_rwlockattr_t attr;	

				if(pthread_rwlockattr_init(&attr)) { 
					puts("Unable to initialize pthread attributes (pthread_rwlockattr_init).");
					return -1; 
				}
				if(pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)) { 
					puts("Unable to set PTHREAD_PROCESS_SHARED (pthread_rwlockattr_setpshared), your system may not support shared rwlock's.");
					return -1; 
				}	
				if(pthread_rwlock_init(&rwlock, &attr)) { 
					puts("Unable to initialize the rwlock (pthread_rwlock_init).");
					return -1; 
				}
				if(pthread_rwlockattr_destroy(&attr)) { 
					puts("Unable to destroy rwlock attributes (pthread_rwlockattr_destroy).");
					return -1; 
				}
				if(pthread_rwlock_destroy(&rwlock)) { 
					puts("Unable to destroy rwlock (pthread_rwlock_destroy).");
					return -1; 
				}

				puts("pthread rwlocks are supported!");
				return 0;
                                }
			],
			[ dnl -Success-
				PHP_ADD_LIBRARY(pthread)
				APC_CFLAGS="-D_GNU_SOURCE"
			],
			[ dnl -Failure-
				AC_MSG_WARN([It doesn't appear that pthread rwlocks are supported on your system])
  				PHP_APCU_PTHREADRWLOCK=no
			],
			[
				PHP_ADD_LIBRARY(pthread)
			]
	)
	LIBS="$orig_LIBS"
fi

	AC_CACHE_CHECK([whether the target compiler supports builtin atomics], PHP_cv_APC_GCC_ATOMICS, [

			AC_TRY_LINK([],[
					int foo = 0;
					__sync_fetch_and_add(&foo, 1);
					__sync_bool_compare_and_swap(&foo, 0, 1);
					return __sync_fetch_and_add(&foo, 1);
				],
				[PHP_cv_APC_GCC_ATOMICS=yes],
				[PHP_cv_APC_GCC_ATOMICS=no])
		])

	if test "x${PHP_cv_APC_GCC_ATOMICS}" != "xno"; then
			AC_DEFINE(HAVE_ATOMIC_OPERATIONS, 1,
				[Define this if your target compiler supports builtin atomics])
		else
			if test "$PHP_APCU_PTHREADRWLOCK" != "no"; then
				AC_MSG_WARN([Disabling pthread rwlocks, because of missing atomic operations])
				dnl - fall back would most likely be pthread mutexes 
				PHP_APCU_PTHREADRWLOCK=no
			fi
	fi

AC_MSG_CHECKING(whether we should use spin locks)
AC_ARG_ENABLE(apc-spinlocks,
[  --enable-apcu-spinlocks
                          Enable spin locks  EXPERIMENTAL ],
[
  PHP_APCU_SPINLOCKS=$enableval
  AC_MSG_RESULT($enableval)
],
[
  PHP_APCU_SPINLOCKS=no
  AC_MSG_RESULT(no)
])


AC_MSG_CHECKING(whether we should enable memory protection)
AC_ARG_ENABLE(apcu-memprotect,
[  --enable-apcu-memprotect
                          Enable mmap/shm memory protection],
[
  PHP_APCU_MEMPROTECT=$enableval
  AC_MSG_RESULT($enableval)
], [
  PHP_APCU_MEMPROTECT=no
  AC_MSG_RESULT(no)
])

if test "$PHP_APCU" != "no"; then
  test "$PHP_APCU_MMAP" != "no" && AC_DEFINE(APC_MMAP, 1, [ ])

	if test "$PHP_APCU_DEBUG" != "no"; then
		AC_DEFINE(__DEBUG_APC__, 1, [ ])
	fi

	if test "$PHP_APCU_SEM" != "no"; then
		AC_DEFINE(APC_SEM_LOCKS, 1, [ ])
	elif test "$PHP_APCU_SPINLOCKS" != "no"; then
		AC_DEFINE(APC_SPIN_LOCKS, 1, [ ]) 
	elif test "$PHP_APCU_PTHREADRWLOCK" != "no"; then
		AC_DEFINE(APC_PTHREADRW_LOCKS, 1, [ ]) 
	elif test "$PHP_APCU_PTHREADMUTEX" != "no"; then 
		AC_DEFINE(APC_PTHREADMUTEX_LOCKS, 1, [ ])
	else 
		AC_DEFINE(APC_FCNTL_LOCKS, 1, [ ])
	fi
  
	if test "$PHP_APCU_MEMPROTECT" != "no"; then
		AC_DEFINE(APC_MEMPROTECT, 1, [ shm/mmap memory protection ])
	fi

  AC_CHECK_FUNCS(sigaction)
  AC_CACHE_CHECK(for union semun, php_cv_semun,
  [
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
    ], [union semun x; x.val=1], [
      php_cv_semun=yes
    ],[
      php_cv_semun=no
    ])
  ])
  if test "$php_cv_semun" = "yes"; then
    AC_DEFINE(HAVE_SEMUN, 1, [ ])
  else
    AC_DEFINE(HAVE_SEMUN, 0, [ ])
  fi

  AC_MSG_CHECKING(whether we should enable valgrind support)
  AC_ARG_ENABLE(valgrind-checks,
  [  --disable-valgrind-checks
                          Disable valgrind based memory checks],
  [
    PHP_APCU_VALGRIND=$enableval
    AC_MSG_RESULT($enableval)
  ], [
    PHP_APCU_VALGRIND=yes
    AC_MSG_RESULT(yes)
    AC_CHECK_HEADER(valgrind/memcheck.h, 
  		[AC_DEFINE([HAVE_VALGRIND_MEMCHECK_H],1, [enable valgrind memchecks])])
  ])

  apc_sources="apc.c apc_lock.c php_apc.c \
               apc_cache.c \
               apc_debug.c \
               apc_main.c \
               apc_mmap.c \
               apc_sem.c \
               apc_shm.c \
               apc_sma.c \
               apc_stack.c \
               apc_rfc1867.c \
               apc_signal.c \
               apc_pool.c \
               apc_iterator.c \
							 apc_bin.c "

  PHP_CHECK_LIBRARY(rt, shm_open, [PHP_ADD_LIBRARY(rt,,APC_SHARED_LIBADD)])
  PHP_NEW_EXTENSION(apcu, $apc_sources, $ext_shared,, \\$(APC_CFLAGS))
  PHP_SUBST(APC_SHARED_LIBADD)
  PHP_SUBST(APC_CFLAGS)
  PHP_INSTALL_HEADERS(ext/apcu, [apc_serializer.h])
  AC_DEFINE(HAVE_APCU, 1, [ ])
fi

PHP_ARG_ENABLE(coverage,  whether to include code coverage symbols,
[  --enable-coverage           DEVELOPERS ONLY!!], no, no)

if test "$PHP_COVERAGE" = "yes"; then

  if test "$GCC" != "yes"; then
    AC_MSG_ERROR([GCC is required for --enable-coverage])
  fi
  
  dnl Check if ccache is being used
  case `$php_shtool path $CC` in
    *ccache*[)] gcc_ccache=yes;;
    *[)] gcc_ccache=no;;
  esac

  if test "$gcc_ccache" = "yes" && (test -z "$CCACHE_DISABLE" || test "$CCACHE_DISABLE" != "1"); then
    AC_MSG_ERROR([ccache must be disabled when --enable-coverage option is used. You can disable ccache by setting environment variable CCACHE_DISABLE=1.])
  fi
  
  lcov_version_list="1.5 1.6 1.7 1.9"

  AC_CHECK_PROG(LCOV, lcov, lcov)
  AC_CHECK_PROG(GENHTML, genhtml, genhtml)
  PHP_SUBST(LCOV)
  PHP_SUBST(GENHTML)

  if test "$LCOV"; then
    AC_CACHE_CHECK([for lcov version], php_cv_lcov_version, [
      php_cv_lcov_version=invalid
      lcov_version=`$LCOV -v 2>/dev/null | $SED -e 's/^.* //'` #'
      for lcov_check_version in $lcov_version_list; do
        if test "$lcov_version" = "$lcov_check_version"; then
          php_cv_lcov_version="$lcov_check_version (ok)"
        fi
      done
    ])
  else
    lcov_msg="To enable code coverage reporting you must have one of the following LCOV versions installed: $lcov_version_list"      
    AC_MSG_ERROR([$lcov_msg])
  fi

  case $php_cv_lcov_version in
    ""|invalid[)]
      lcov_msg="You must have one of the following versions of LCOV: $lcov_version_list (found: $lcov_version)."
      AC_MSG_ERROR([$lcov_msg])
      LCOV="exit 0;"
      ;;
  esac

  if test -z "$GENHTML"; then
    AC_MSG_ERROR([Could not find genhtml from the LCOV package])
  fi

  PHP_ADD_MAKEFILE_FRAGMENT

  dnl Remove all optimization flags from CFLAGS
  changequote({,})
  CFLAGS=`echo "$CFLAGS" | $SED -e 's/-O[0-9s]*//g'`
  CXXFLAGS=`echo "$CXXFLAGS" | $SED -e 's/-O[0-9s]*//g'`
  changequote([,])

  dnl Add the special gcc flags
  CFLAGS="$CFLAGS -O0 -ggdb -fprofile-arcs -ftest-coverage"
  CXXFLAGS="$CXXFLAGS -ggdb -O0 -fprofile-arcs -ftest-coverage"
fi
dnl vim: set ts=2 
