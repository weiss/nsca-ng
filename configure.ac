# Process this file with autoconf(1) to produce a configure script.

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

# Autotools initialization.
AC_PREREQ([2.60]) dnl But Autoconf 2.63 is required for AC_INIT's URL parameter.
AC_INIT([NSCA-ng],
  [1.5],
  [developers@nsca-ng.org],
  [nsca-ng],
  [http://www.nsca-ng.org/])
AC_CONFIG_SRCDIR([src/server/nsca-ng.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_TESTDIR([tests], [tests:src/client:src/server])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror check-news])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES]) dnl Automake 1.11 added this.

# Set the release date.
RELEASE_DATE='November 15, 2016'

AC_SUBST([RELEASE_DATE])

# Check whether to build the client.
AC_ARG_ENABLE([client],
  [AS_HELP_STRING([--enable-client],
    [build the NSCA-ng client [default: yes]])],
  [nsca_enable_client=$enable_client],
  [nsca_enable_client=yes])
AC_MSG_CHECKING([whether the NSCA-ng client should be built])
AS_IF([test "x$nsca_enable_client" = xyes],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])

# Check whether to build the server.
AC_ARG_ENABLE([server],
  [AS_HELP_STRING([--enable-server],
    [build the NSCA-ng server [default: no]])],
  [nsca_enable_server=$enable_server],
  [nsca_enable_server=no])
AC_MSG_CHECKING([whether the NSCA-ng server should be built])
AS_IF([test "x$nsca_enable_server" = xyes],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])

# Check whether to use the POSIX AIO API.
AC_ARG_ENABLE([posix-aio],
  [AS_HELP_STRING([--disable-posix-aio],
    [do not use the POSIX AIO API [default: auto-detect]])],
  [nsca_enable_posix_aio=$enable_posix_aio],
  [nsca_enable_posix_aio=yes])

# Bail out if neither the client nor the server should be built.
AS_IF([test "x$nsca_enable_client" = xno && test "x$nsca_enable_server" = xno],
  [AC_MSG_ERROR([Please specify --enable-client and/or --enable-server])])

# Tell Automake whether to build the client and/or server.
AM_CONDITIONAL([BUILD_CLIENT], [test "x$nsca_enable_client" = xyes])
AM_CONDITIONAL([BUILD_SERVER], [test "x$nsca_enable_server" = xyes])

# Check for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_MKDIR_P
AC_PROG_RANLIB

m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) dnl Automake 1.11.2 added this.

AM_CONDITIONAL([GCC], [test "x$GCC" = xyes])

# Check for libraries.
NSCA_LIB_NETWORKING
NSCA_LIB_EV
NSCA_LIB_OPENSSL
AS_IF([test "x$nsca_enable_server" = xyes],
  [NSCA_LIB_CONFUSE
   NSCA_LIB_SYSTEMD
   NSCA_LIB_PIDFILE
   AS_IF([test "x$nsca_enable_posix_aio" = xyes],
     [NSCA_LIB_AIO])])

# Tell Automake about some of our check results.
AM_CONDITIONAL([USE_EMBEDDED_EV],
  [test "x$nsca_lib_ev_embedded" = xyes])
AM_CONDITIONAL([USE_EMBEDDED_PIDFILE],
  [test "x$nsca_lib_pidfile_embedded" = xyes])
AM_CONDITIONAL([HAVE_FLOCK],
  [test "x$nsca_func_flock" = xyes])

# Check for header files.
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([inttypes.h])

# Check for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T
AS_IF([test "x$nsca_enable_server" = xyes],
  [AC_TYPE_SSIZE_T
   AC_TYPE_INTMAX_T
   AC_TYPE_PID_T])

# Check for library functions.
HW_FUNC_VSNPRINTF
HW_FUNC_SNPRINTF
HW_FUNC_VASPRINTF
HW_FUNC_ASPRINTF
NSCA_FUNC_PROGNAME
AC_REPLACE_FUNCS([strdup strcasecmp strncasecmp])
AS_IF([test "x$nsca_enable_client" = xyes],
  [AC_CHECK_FUNCS([nanosleep])])
AS_IF([test "x$nsca_enable_server" = xyes],
  [AC_CHECK_FUNCS([closefrom])
   NSCA_FUNC_DAEMON])

# Communicate the PIPE_BUF value to Autotest.
AC_COMPUTE_INT([nsca_pipe_buf], [PIPE_BUF], [[#include <limits.h>]],
  [nsca_pipe_buf=512])
AC_SUBST([nsca_pipe_buf])

# Spit out the results.
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
  build-aux/Makefile
  contrib/Makefile
  etc/Makefile
  lib/Makefile
  lib/ev/Makefile
  lib/pidfile/Makefile
  man/Makefile
  perl/Makefile
  python/Makefile
  src/Makefile
  src/client/Makefile
  src/common/Makefile
  src/server/Makefile
  tests/Makefile
  tests/atlocal])

AC_OUTPUT

AC_MSG_NOTICE([summary of build options:

  Build NSCA-ng client: $nsca_enable_client
  Build NSCA-ng server: $nsca_enable_server
  Use embedded libev:   $nsca_lib_ev_embedded
  Compiler:             $CC
])

dnl vim:set joinspaces textwidth=80 filetype=m4:
