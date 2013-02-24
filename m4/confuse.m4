# Copyright (c) 2013 Holger Weiss <holger@weiss.in-berlin.de>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# NSCA_LIB_CONFUSE
# ----------------
# Check the availability of libConfuse 2.6 or newer.  As this is a required
# dependency, we bail out if the user specified "--without-confuse", if we fail
# to link against the library, or if the confuse version we found is too old.
# On success, we set the output variables CONFUSECPPFLAGS, CONFUSELDFLAGS, and
# CONFUSELIBS to appropriate values.  (Why are there no underscores in these
# variable names?  Because Automake treats names like "foo_CPPFLAGS" specially.)
AC_DEFUN([NSCA_LIB_CONFUSE],
[
  nsca_save_CPPFLAGS=$CPPFLAGS
  nsca_save_LDFLAGS=$LDFLAGS
  AC_ARG_WITH([confuse],
    [AS_HELP_STRING([--with-confuse=PATH],
      [use the libConfuse library in PATH])],
    [nsca_confuse_dir=$with_confuse],
    [nsca_confuse_dir=yes])
  AC_MSG_CHECKING([whether libConfuse is desired])
  AS_CASE([$nsca_confuse_dir],
    [no],
      [AC_MSG_RESULT([no])
       AC_MSG_ERROR([building without libConfuse is not supported])],
    [yes],
      [AC_MSG_RESULT([yes])
       AC_MSG_CHECKING([for the location of libConfuse])
       nsca_confuse_dir=unknown
       for _nsca_confuse_dir in "$ac_pwd/lib/confuse" /usr /usr/local /usr/pkg
       do dnl Solaris 10 doesn't have "test -e".
         AS_IF([test -r "$_nsca_confuse_dir/include/confuse.h"],
           [nsca_confuse_dir=$_nsca_confuse_dir
            break])
       done
       AS_IF([test "x$nsca_confuse_dir" != xunknown],
         [AC_MSG_RESULT([$nsca_confuse_dir])],
         [AC_MSG_RESULT([not found, continuing anyway])])],
    [AC_MSG_RESULT([yes])])
  AS_IF([test "x$nsca_confuse_dir" != xunknown &&
    test "x$nsca_confuse_dir" != x/usr],
    [CONFUSECPPFLAGS="-I$nsca_confuse_dir/include"
     CONFUSELDFLAGS="-L$nsca_confuse_dir/lib"
     CPPFLAGS="$CONFUSECPPFLAGS $CPPFLAGS"
     LDFLAGS="$CONFUSELDFLAGS $LDFLAGS"])
  AC_CHECK_HEADER([confuse.h], [],
    [AC_MSG_ERROR([libConfuse header file not found])])
  AC_CHECK_LIB([confuse], [cfg_init],
    [CONFUSELIBS='-lconfuse'],
    [AC_CHECK_LIB([intl], [cfg_init],
      [CONFUSELIBS='-lconfuse -lintl'],
      [AC_CHECK_LIB([iconv], [cfg_init],
        [CONFUSELIBS='-lconfuse -lintl -liconv'],
        [AC_MSG_FAILURE([cannot link with libConfuse])],
        [-lconfuse -lintl -liconv])],
      [-lconfuse -lintl])])
  AC_CHECK_DECL([CFGF_NO_TITLE_DUPES], [],
    [AC_MSG_ERROR([libConfuse too old, version 2.6 or newer is required])],
    [[#include <confuse.h>]])
  AC_SUBST([CONFUSECPPFLAGS])
  AC_SUBST([CONFUSELDFLAGS])
  AC_SUBST([CONFUSELIBS])
  CPPFLAGS=$nsca_save_CPPFLAGS
  LDFLAGS=$nsca_save_LDFLAGS
])# NSCA_LIB_CONFUSE

dnl vim:set joinspaces textwidth=80:
