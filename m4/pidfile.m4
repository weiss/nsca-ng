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

# NSCA_LIB_PIDFILE
# ----------------
# Check the availability of FreeBSD's pidfile(3) API (which is also available on
# some Linux systems).  If it's found, we set $nsca_lib_pidfile_embedded to "no"
# and arrange for using the library which provides the API.  Otherwise, we set
# $nsca_lib_pidfile_embedded to "yes" and arrange for using the bundled
# replacement functions.
AC_DEFUN([NSCA_LIB_PIDFILE],
[
  # Minimize the number of dependencies of our Debian package.
  nsca_lib_pidfile_embedded=yes
  _NSCA_LIB_PIDFILE_EMBEDDED
])# NSCA_LIB_PIDFILE

# _NSCA_LIB_PIDFILE_EMBEDDED
# --------------------------
# Arrange for using the bundled pidfile functions.  If the flock(2) function is
# found, we set $nsca_func_flock to "yes"; otherwise, we set that variable to
# "no".
AC_DEFUN([_NSCA_LIB_PIDFILE_EMBEDDED],
[
  AC_CHECK_HEADERS([sys/file.h])
  AC_TYPE_MODE_T
  # At least on AIX 5.3, flock(2) is hidden in libbsd.
  AC_SEARCH_LIBS([flock], [bsd],
    [nsca_func_flock=yes
     AC_DEFINE([HAVE_FLOCK], [1],
       [Define to 1 if you have the `flock' function.])],
    [nsca_func_flock=no])
])# _NSCA_LIB_PIDFILE_EMBEDDED

dnl vim:set joinspaces textwidth=80:
