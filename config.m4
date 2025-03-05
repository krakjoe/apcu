PHP_ARG_ENABLE([apcu],
  [whether to enable APCu support],
  [AS_HELP_STRING([--enable-apcu],
    [Enable APCu support])])

PHP_ARG_ENABLE([apcu-rwlocks],
  [if APCu should be allowed to use rwlocks],
  [AS_HELP_STRING([--disable-apcu-rwlocks],
    [Disable rwlocks in APCu])],
  [yes],
  [no])

PHP_ARG_ENABLE([apcu-debug],
  [if APCu should be built in debug mode],
  [AS_HELP_STRING([--enable-apcu-debug],
    [Enable APCu debugging])],
  [no],
  [no])

PHP_ARG_ENABLE([apcu-clear-signal],
  [if APCu should clear on SIGUSR1],
  [AS_HELP_STRING([--enable-apcu-clear-signal],
    [Enable SIGUSR1 clearing handler])],
  [no],
  [no])

PHP_ARG_ENABLE([apcu-mmap],
  [if APCu should use mmap instead of shm],
  [AS_HELP_STRING([--disable-apcu-mmap],
    [Disable mmap, falls back on shm])],
  [yes],
  [no])

PHP_ARG_ENABLE([apcu-spinlocks],
  [if APCu should utilize spinlocks before flocks],
  [AS_HELP_STRING([--enable-apcu-spinlocks],
    [Use spinlocks before flocks])],
  [no],
  [no])

PHP_ARG_ENABLE([valgrind-checks],
  [whether to enable Valgrind-based memory checks],
  [AS_HELP_STRING([--disable-valgrind-checks],
    [Disable Valgrind-based memory checks])],
  [yes],
  [no])

PHP_ARG_ENABLE([coverage],
  [whether to include code coverage symbols],
  [AS_HELP_STRING([--enable-coverage],
    [Include code coverage symbols (DEVELOPERS ONLY!!)])],
  [no],
  [no])

if test "$PHP_APCU" != "no"; then
  AS_VAR_IF([PHP_APCU_DEBUG], [no], [],
    [AC_DEFINE([APC_DEBUG], [1],
      [Define to 1 if APCu debugging mode is enabled.])])

  AS_VAR_IF([PHP_APCU_CLEAR_SIGNAL], [no], [],
    [AC_DEFINE([APC_CLEAR_SIGNAL], [1],
      [Define to 1 if SIGUSR1 clearing handler is enabled.])])

  AS_VAR_IF([PHP_APCU_MMAP], [no], [],
    [AC_DEFINE([APC_MMAP], [1],
      [Define to 1 if APCu uses mmap instead of shm.])])

  AS_VAR_IF([PHP_VALGRIND_CHECKS], [no], [],
    [AC_CHECK_HEADERS([valgrind/memcheck.h], [],
      [AC_MSG_NOTICE([Valgrind-based memory checks are disabled.])])])

  AS_VAR_IF([PHP_APCU_RWLOCKS], [no], [], [
    AC_CACHE_CHECK([whether the target compiler supports builtin atomics],
      [PHP_cv_APCU_GCC_ATOMICS],
      [AC_LINK_IFELSE([AC_LANG_PROGRAM([], [
        int foo = 0;
        __sync_add_and_fetch(&foo, 1);
        __sync_sub_and_fetch(&foo, 1);
        return 0;
      ])], [PHP_cv_APCU_GCC_ATOMICS=yes], [PHP_cv_APCU_GCC_ATOMICS=no])])
    AS_VAR_IF([PHP_cv_APCU_GCC_ATOMICS], [no],
      [AC_MSG_FAILURE([Compiler does not support atomics])])
  ])

  if test "$PHP_APCU_RWLOCKS" != "no"; then
      orig_LIBS="$LIBS"
      LIBS="$LIBS -lpthread"
        AC_MSG_CHECKING([for pthread rwlocks])
      AC_RUN_IFELSE([AC_LANG_SOURCE([[
          #include <sys/types.h>
          #include <pthread.h>
                #include <stdio.h>
          int main() {
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

            return 0;
          }
        ]])],[ dnl -Success-
          APCU_CFLAGS="-D_GNU_SOURCE -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1"
          PHP_ADD_LIBRARY(pthread)
          PHP_LDFLAGS="$PHP_LDFLAGS -lpthread"
          AC_DEFINE(APC_NATIVE_RWLOCK, 1, [ ])
          AC_MSG_RESULT([yes])
        ],[ dnl -Failure-
          AC_MSG_RESULT([no])
          PHP_APCU_RWLOCKS=no
        ],[
          APCU_CFLAGS="-D_GNU_SOURCE -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1"
          PHP_ADD_LIBRARY(pthread)
          PHP_LDFLAGS="$PHP_LDFLAGS -lpthread"
    ])
    LIBS="$orig_LIBS"
  fi

  if test "$PHP_APCU" != "no"; then
    orig_LIBS="$LIBS"
    LIBS="$LIBS -lpthread"
      AC_MSG_CHECKING([for pthread mutexes])
    AC_RUN_IFELSE([AC_LANG_SOURCE([[
          #include <sys/types.h>
          #include <pthread.h>
                  #include <stdio.h>
          int main() {
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
            return 0;
        }
        ]])],[ dnl -Success-
          APCU_CFLAGS="-D_GNU_SOURCE -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1"
          PHP_ADD_LIBRARY(pthread)
          PHP_LDFLAGS="$PHP_LDFLAGS -lpthread"
          AC_MSG_RESULT([yes])
          AC_DEFINE(APC_HAS_PTHREAD_MUTEX, 1, [ ])
        ],[ dnl -Failure-
          AC_MSG_RESULT([no])
          PHP_APCU_MUTEX=no
        ],[
          APCU_CFLAGS="-D_GNU_SOURCE -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1"
          PHP_ADD_LIBRARY(pthread)
          PHP_LDFLAGS="$PHP_LDFLAGS -lpthread"
    ])
    LIBS="$orig_LIBS"
  fi

  if test "$PHP_APCU_RWLOCKS" = "no"; then
   if test "$PHP_APCU_MUTEX" = "no"; then
      AS_VAR_IF([PHP_APCU_SPINLOCKS], [no], [
        AC_DEFINE([APC_FCNTL_LOCK], [1],
          [Define to 1 if APCu file locking is enabled.])
        AC_MSG_WARN([APCu file locking enabled])
      ], [
        AC_DEFINE([APC_SPIN_LOCK], [1],
          [Define to 1 if APCu spin locking is enabled.])
        AC_MSG_WARN([APCu spin locking enabled])
      ])
   fi
  fi

  AC_CHECK_FUNCS(sigaction)

  for i in -Wall -Wextra -Wno-clobbered -Wno-unused-parameter; do
    AX_CHECK_COMPILE_FLAG([$i], [APCU_CFLAGS="$APCU_CFLAGS $i"])
  done

  apc_sources="apc.c apc_lock.c apc_mutex.c php_apc.c \
                 apc_cache.c \
                 apc_mmap.c \
                 apc_shm.c \
                 apc_sma.c \
                 apc_stack.c \
                 apc_signal.c \
                 apc_time.c \
                 apc_iterator.c \
                 apc_persist.c"

  LIBS_SAVED=$LIBS; LIBS=
  AC_SEARCH_LIBS([shm_open], [rt])
  LIBS=$LIBS_SAVED
  AS_CASE([$ac_cv_search_shm_open], ["none required"|no], [],
    [PHP_EVAL_LIBLINE([$ac_cv_search_shm_open], [APCU_SHARED_LIBADD])])

  PHP_NEW_EXTENSION([apcu], [$apc_sources], [$ext_shared],, [$APCU_CFLAGS])
  PHP_SUBST(APCU_SHARED_LIBADD)
  PHP_SUBST(PHP_LDFLAGS)
  PHP_INSTALL_HEADERS(ext/apcu, [php_apc.h apc.h apc_api.h apc_cache.h apc_globals.h apc_iterator.h apc_lock.h apc_mutex.h apc_sma.h apc_serializer.h apc_stack.h apc_arginfo.h php_apc_legacy_arginfo.h])
  AC_DEFINE(HAVE_APCU, 1, [ ])
fi

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
    AC_MSG_ERROR([ccache must be disabled when --enable-coverage option is used. You can disable ccache by setting environment variable
CCACHE_DISABLE=1.])
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
