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

# NSCA_LIB_SYSTEMD
# ----------------
# Check the availability of systemd.  If the systemd (or the old systemd-daemon)
# library is found, we define HAVE_SYSTEMD_SD_DAEMON_H to 1.  We then also set
# the output variables SYSTEMDCPPFLAGS, SYSTEMDLDFLAGS, and SYSTEMDLIBS to
# appropriate values.  (Why are there no underscores in these variable names?
# Because Automake treats names like "foo_CPPFLAGS" specially.)
AC_DEFUN([NSCA_LIB_SYSTEMD],
[
  nsca_save_CPPFLAGS=$CPPFLAGS
  nsca_save_LDFLAGS=$LDFLAGS
  nsca_systemd_dir=unknown
  AC_ARG_WITH([systemd],
    [AS_HELP_STRING([--with-systemd=PATH],
      [use the systemd library in PATH])],
    [nsca_with_systemd=$with_systemd],
    [nsca_with_systemd=maybe])
  AC_MSG_CHECKING([whether systemd is desired])
  AS_CASE([$nsca_with_systemd],
    [no],
      [AC_MSG_RESULT([no])],
    [yes|maybe],
      [AC_MSG_RESULT([$nsca_with_systemd])
       AC_MSG_CHECKING([for the location of systemd])
       for _nsca_systemd_dir in "$ac_pwd/lib/systemd" /usr /usr/local /usr/pkg
       do dnl Solaris 10 doesn't have "test -e".
         AS_IF([test -r "$_nsca_systemd_dir/include/systemd/sd-daemon.h"],
           [nsca_systemd_dir=$_nsca_systemd_dir
            break])
       done
       AS_IF([test "x$nsca_systemd_dir" != xunknown],
         [AC_MSG_RESULT([$nsca_systemd_dir])
          AS_IF([test "x$nsca_with_systemd" = xmaybe],
            [nsca_with_systemd=yes])],
         [AC_MSG_RESULT([not found])
          AS_IF([test "x$nsca_with_systemd" = xmaybe],
            [nsca_with_systemd=no])])],
    [AC_MSG_RESULT([yes])
     nsca_with_systemd=yes
     nsca_systemd_dir=$nsca_with_systemd])
  AS_IF([test "x$nsca_with_systemd" = xyes],
    [AS_IF([test "x$nsca_systemd_dir" != xunknown &&
      test "x$nsca_systemd_dir" != x/usr],
      [SYSTEMDCPPFLAGS="-I$nsca_systemd_dir/include"
       SYSTEMDLDFLAGS="-L$nsca_systemd_dir/lib"
       CPPFLAGS="$SYSTEMDCPPFLAGS $CPPFLAGS"
       LDFLAGS="$SYSTEMDLDFLAGS $LDFLAGS"])
     AC_CHECK_HEADERS([systemd/sd-daemon.h], [],
       [AC_MSG_ERROR([systemd header file not found])])
     AC_CHECK_LIB([systemd], [sd_notify],
       [SYSTEMDLIBS='-lsystemd'],
       [AC_CHECK_LIB([systemd-daemon], [sd_notify],
         [SYSTEMDLIBS='-lsystemd-daemon'],
         [AC_MSG_FAILURE([cannot link with systemd-daemon library])])])])
  AC_SUBST([SYSTEMDCPPFLAGS])
  AC_SUBST([SYSTEMDLDFLAGS])
  AC_SUBST([SYSTEMDLIBS])
  CPPFLAGS=$nsca_save_CPPFLAGS
  LDFLAGS=$nsca_save_LDFLAGS
])# NSCA_LIB_SYSTEMD

dnl vim:set joinspaces textwidth=80:
