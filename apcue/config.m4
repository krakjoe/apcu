dnl $Id$
dnl config.m4 for extension apcue

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(apcue, for apcue support,
dnl Make sure that the comment is aligned:
dnl [  --with-apcue             Include apcue support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(apcue, whether to enable apcue support,
[  --enable-apcue           Enable apcue support])

if test "$PHP_APCUE" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-apcue -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/apcue.h"  # you most likely want to change this
  dnl if test -r $PHP_APCUE/$SEARCH_FOR; then # path given as parameter
  dnl   APCUE_DIR=$PHP_APCUE
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for apcue files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       APCUE_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$APCUE_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the apcue distribution])
  dnl fi

  dnl # --with-apcue -> add include path
  dnl PHP_ADD_INCLUDE($APCUE_DIR/include)

  dnl # --with-apcue -> check for lib and symbol presence
  dnl LIBNAME=apcue # you may want to change this
  dnl LIBSYMBOL=apcue # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $APCUE_DIR/lib, APCUE_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_APCUELIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong apcue lib version or lib not found])
  dnl ],[
  dnl   -L$APCUE_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(APCUE_SHARED_LIBADD)

  PHP_NEW_EXTENSION(apcue, apcue.c, $ext_shared)
fi
