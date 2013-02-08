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

# NSCA_FUNC_PROGNAME
# ------------------
# Check the availability of BSD-style getprogname(3) and setprogname(3)
# functions.  If either of these functions isn't found, arrange for using the
# bundled implementation.
#
# Note that some Linux systems provide a crippled version of these functions in
# libbsd (they return the full pathname of the program, not just the final
# component).  On those systems, the functions are declared in <bsd/stdlib.h>.
# Using AC_CHECK_DECL instead of AC_CHECK_FUNC should make sure that they won't
# be used.
#
# Also note that glibc provides program_invocation_short_name(3) in <errno.h> if
# _GNU_SOURCE is defined, but there's not much point in using it.
AC_DEFUN([NSCA_FUNC_PROGNAME],
[
  AC_CHECK_DECL([getprogname],
    [nsca_func_getprogname=yes
     AC_DEFINE([HAVE_GETPROGNAME], [1],
       [Define to 1 if you have the `getprogname' function.])],
    [nsca_func_getprogname=no],
    [[#include <stdlib.h>]])
  AC_CHECK_DECL([setprogname],
    [nsca_func_setprogname=yes
     AC_DEFINE([HAVE_SETPROGNAME], [1],
       [Define to 1 if you have the `setprogname' function.])],
    [nsca_func_setprogname=no],
    [[#include <stdlib.h>]])
  AS_IF([test "x$nsca_func_getprogname" != xyes ||
    test "x$nsca_func_setprogname" != xyes],
    [AC_LIBOBJ([progname])])
])# NSCA_FUNC_PROGNAME

dnl vim:set joinspaces textwidth=80:
