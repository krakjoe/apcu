dnl
dnl $Id: config.m4 327593 2012-09-10 11:50:58Z pajoye $
dnl

PHP_ARG_ENABLE(apcu, whether to enable APCu support,
[  --enable-apcu          Enable APCu support])

AC_MSG_CHECKING(if APCu should be allowed to use rwlocks)
AC_ARG_ENABLE(apcu-rwlocks,
[  --disable-apcu-rwlocks Disable rwlocks in APCu],
[
  PHP_APCU_RWLOCKS=no
  AC_MSG_RESULT(no)
],
[
  PHP_APCU_RWLOCKS=yes
  AC_MSG_RESULT(yes)
])

AC_MSG_CHECKING(if APCu should be built in debug mode)
AC_ARG_ENABLE(apcu-debug,
[  --enable-apcu-debug    Enable APCu debugging], 
[
  PHP_APCU_DEBUG=yes
	AC_MSG_RESULT(yes)
], 
[
  PHP_APCU_DEBUG=no
  AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(if APCu should clear on SIGUSR1)
AC_ARG_ENABLE(apcu-clear-signal,
[  --enable-apcu-clear-signal  Enable SIGUSR1 clearing handler],
[
  AC_DEFINE(APC_CLEAR_SIGNAL, 1, [ ])
  AC_MSG_RESULT(yes)
],
[
  AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(if APCu will use mmap or shm)
AC_ARG_ENABLE(apcu-mmap,
[  --disable-apcu-mmap		Disable mmap, falls back on shm],
[
  PHP_APCU_MMAP=no
  AC_MSG_RESULT(shm)
], [
  PHP_APCU_MMAP=yes
  AC_MSG_RESULT(mmap)
])

if test "$PHP_APCU" != "no"; then
	if test "$PHP_APCU_DEBUG" != "no"; then
		AC_DEFINE(APC_DEBUG, 1, [ ])
	fi
  
	if test "$PHP_APCU_MMAP" != "no"; then
		AC_DEFINE(APC_MMAP, 1, [ ])
	fi

  if test "$PHP_APCU_RWLOCKS" != "no"; then
    PHP_CHECK_LIBRARY(pthread, pthread_rwlock_init,
    [
      PHP_ADD_LIBRARY(pthread,,APCU_SHARED_LIBADD)
		  AC_DEFINE(APC_NATIVE_RWLOCK, 1, [ ])
    ])
  fi
	
  AC_CHECK_FUNCS(sigaction)
  AC_CACHE_CHECK(for union semun, php_cv_semun,
  [
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
    ], [union semun x; x.val=1], [
      hp_cv_semun=yesp
    ],[
      php_cv_semun=no
    ])
  ])
  if test "$php_cv_semun" = "yes"; then
    AC_DEFINE(HAVE_SEMUN, 1, [ ])
  else
    AC_DEFINE(HAVE_SEMUN, 0, [ ])
  fi

  AC_ARG_ENABLE(valgrind-checks,
  [  --disable-valgrind-checks
                          Disable valgrind based memory checks],
  [
    PHP_APCU_VALGRIND=no
  ], [
    PHP_APCU_VALGRIND=yes
    AC_CHECK_HEADER(valgrind/memcheck.h, 
  		[AC_DEFINE([HAVE_VALGRIND_MEMCHECK_H],1, [enable valgrind memchecks])])
  ])

  apc_sources="apc.c apc_lock.c php_apc.c \
               apc_cache.c \
               apc_mmap.c \
               apc_shm.c \
               apc_sma.c \
               apc_stack.c \
               apc_rfc1867.c \
               apc_signal.c \
               apc_pool.c \
               apc_iterator.c \
							 apc_bin.c "

  PHP_CHECK_LIBRARY(rt, shm_open, [PHP_ADD_LIBRARY(rt,,APCU_SHARED_LIBADD)])
  PHP_NEW_EXTENSION(apcu, $apc_sources, $ext_shared,, \\$(APCU_CFLAGS))
  PHP_SUBST(APCU_SHARED_LIBADD)
  PHP_SUBST(APCU_CFLAGS)
  PHP_INSTALL_HEADERS(ext/apcu, [apc.h apc_api.h apc_cache_api.h apc_lock_api.h apc_pool_api.h apc_sma_api.h apc_bin_api.h])
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
