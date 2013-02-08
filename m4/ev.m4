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

# NSCA_LIB_EV
# -----------
# Check the availability of libev 4.00 or newer.  If it's not found, or if the
# user specified "--with-ev=embedded", we use our internal copy of libev and set
# $nsca_lib_ev_embedded to "yes".  Otherwise, we set $nsca_lib_ev_embedded to
# "no".  Either way, we set the output variables EVCPPFLAGS, EVLDFLAGS, and
# EVLIBS to appropriate values.  (Why are there no underscores in these variable
# names?  Because Automake treats names like "foo_CPPFLAGS" specially.)
AC_DEFUN([NSCA_LIB_EV],
[
  nsca_save_CPPFLAGS=$CPPFLAGS
  nsca_save_LDFLAGS=$LDFLAGS
  nsca_lib_ev_embedded=maybe
  AC_ARG_WITH([ev],
    [AS_HELP_STRING([--with-ev=PATH],
      [use the libev library in PATH])],
    [nsca_ev_dir=$with_ev],
    [nsca_ev_dir=yes])
  AC_MSG_CHECKING([whether libev is desired])
  AS_CASE([$nsca_ev_dir],
    [no],
      [AC_MSG_RESULT([no])
       AC_MSG_ERROR([building without libev is not supported])],
    [yes|external],
      [AS_IF([test "x$nsca_ev_dir" = xexternal], [nsca_lib_ev_embedded=no])
       AC_MSG_RESULT([yes])
       AC_MSG_CHECKING([for the location of libev])
       nsca_ev_dir=unknown
       for _nsca_ev_dir in /usr /usr/local /usr/pkg
       do dnl Solaris 10 doesn't have "test -e".
         AS_IF([test -r "$_nsca_ev_dir/include/ev.h"],
           [nsca_ev_dir=$_nsca_ev_dir
            break])
       done
      AS_IF([test "x$nsca_ev_dir" != xunknown],
        [AC_MSG_RESULT([$nsca_ev_dir])],
        [AC_MSG_RESULT([not found, continuing anyway])])],
    [embedded],
      [AC_MSG_RESULT([yes, the bundled copy])
       _NSCA_LIB_EV_EMBEDDED
       nsca_lib_ev_embedded=yes],
    [AC_MSG_RESULT([yes])
     nsca_lib_ev_embedded=no])
  AS_IF([test "x$nsca_ev_dir" != xembedded &&
    test "x$nsca_ev_dir" != xunknown &&
    test "x$nsca_ev_dir" != x/usr],
    [EVCPPFLAGS="-I$nsca_ev_dir/include"
     EVLDFLAGS="-L$nsca_ev_dir/lib"
     CPPFLAGS="$EVCPPFLAGS $CPPFLAGS"
     LDFLAGS="$EVLDFLAGS $LDFLAGS"])
  AS_IF([test "x$nsca_lib_ev_embedded" != xyes],
    [AC_CHECK_HEADER([ev.h],
      [AC_CHECK_LIB([ev], [ev_version_major],
        [EVLIBS="-lev"
         AC_CHECK_LIB([ev], [ev_run], [:],
           [_NSCA_LIB_EV_ERROR([libev too old], [$nsca_lib_ev_embedded])
            nsca_lib_ev_embedded=yes])],
        [_NSCA_LIB_EV_ERROR([cannot link with libev], [$nsca_lib_ev_embedded])
         nsca_lib_ev_embedded=yes])],
      [_NSCA_LIB_EV_ERROR([libev header not found], [$nsca_lib_ev_embedded])
       nsca_lib_ev_embedded=yes])])
  AS_IF([test "x$nsca_lib_ev_embedded" = xyes],
    [unset EVCPPFLAGS EVLDFLAGS EVLIBS],
    [nsca_lib_ev_embedded=no])
  AC_SUBST([EVCPPFLAGS])
  AC_SUBST([EVLDFLAGS])
  AC_SUBST([EVLIBS])
  AC_DEFINE([EV_COMPAT3], [0],
    [Define to 1 if libev 3.x compatibility is desired.])
  CPPFLAGS=$nsca_save_CPPFLAGS
  LDFLAGS=$nsca_save_LDFLAGS
])# NSCA_LIB_EV

# _NSCA_LIB_EV_EMBEDDED
# ---------------------
# Arrange for using the bundled copy of libev.
AC_DEFUN([_NSCA_LIB_EV_EMBEDDED],
[
  dnl Why don't we use m4_include?  The difference is that m4_include warns
  dnl against multiple inclusions, while the builtin does not.  Multiple
  dnl inclusions yield unexpected results if the included file calls m4_define
  dnl without quoting the first argument.  However, lib/ev/libev.m4 doesn't do
  dnl that, so it should be safe to include it more than once.
  m4_builtin([include], [lib/ev/libev.m4])
  AC_DEFINE([EV_MULTIPLICITY], [0],
    [Define to 1 if the bundled libev should support multiple event loops.])
  AC_DEFINE([EV_MINPRI], [-1],
    [Define to the smallest allowed priority in the bundled libev.])
  AC_DEFINE([EV_MAXPRI], [1],
    [Define to the largest allowed priority in the bundled libev.])
  AC_DEFINE([EV_VERIFY], [0],
    [Define to the desired internal verification level of the bundled libev.])
  AC_DEFINE([ECB_AVOID_PTHREADS], [1], dnl Otherwise, building on AIX will fail.
    [Define to 1 if memory fences aren't needed for the bundled libev.])
])# _NSCA_LIB_EV_EMBEDDED

# _NSCA_LIB_EV_ERROR(MESSAGE, USE_EMBEDDED_EV)
# --------------------------------------------
# If USE_EMBEDDED_EV is set to "no", bail out with the specified error MESSAGE.
# Otherwise, spit out the specified MESSAGE and a note that we're going to use
# the included copy of libev, and then call _NSCA_LIB_EV_EMBEDDED.
AC_DEFUN([_NSCA_LIB_EV_ERROR],
[
  AS_IF([test "x$2" = xno],
    [AC_MSG_ERROR([$1])],
    [AC_MSG_NOTICE([$1, using bundled copy])
     _NSCA_LIB_EV_EMBEDDED])
])# _NSCA_LIB_EV_ERROR

dnl vim:set joinspaces textwidth=80:
