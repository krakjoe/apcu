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
fi

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
               apc_mmap.c \
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
