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

# NSCA_LIB_OPENSSL
# ----------------
# Check the availability of OpenSSL 1.0.0 or newer.  As this is a required
# dependency, we bail out if the user specified "--without-openssl", if we fail
# to link against the library, or if the OpenSSL version we found is too old.
# On success, we set the output variables SSLCPPFLAGS, SSLLDFLAGS, and SSLLIBS
# to appropriate values.  (Why are there no underscores in these variable names?
# Because Automake treats names like "foo_CPPFLAGS" specially.)
AC_DEFUN([NSCA_LIB_OPENSSL],
[
  nsca_save_CPPFLAGS=$CPPFLAGS
  nsca_save_LDFLAGS=$LDFLAGS
  AC_ARG_WITH([openssl],
    [AS_HELP_STRING([--with-openssl=PATH],
      [use the OpenSSL library in PATH])],
    [nsca_ssl_dir=$with_openssl],
    [nsca_ssl_dir=yes])
  AC_MSG_CHECKING([whether OpenSSL is desired])
  AS_CASE([$nsca_ssl_dir],
    [no],
      [AC_MSG_RESULT([no])
       AC_MSG_ERROR([building without OpenSSL is not supported])],
    [yes],
      [AC_MSG_RESULT([yes])
       AC_MSG_CHECKING([for the location of OpenSSL])
       nsca_ssl_dir=unknown
       for _nsca_ssl_dir in "$ac_pwd/lib/ssl" /usr /usr/local /usr/local/ssl \
         /usr/pkg
       do dnl Solaris 10 doesn't have "test -e".
         AS_IF([test -r "$_nsca_ssl_dir/include/openssl/ssl.h"],
           [nsca_ssl_dir=$_nsca_ssl_dir
            break])
       done
      AS_IF([test "x$nsca_ssl_dir" != xunknown],
        [AC_MSG_RESULT([$nsca_ssl_dir])],
        [AC_MSG_RESULT([not found, continuing anyway])])],
    [AC_MSG_RESULT([yes])])
  AS_IF([test "x$nsca_ssl_dir" != xunknown && test "x$nsca_ssl_dir" != x/usr],
    [SSLCPPFLAGS="-I$nsca_ssl_dir/include"
     SSLLDFLAGS="-L$nsca_ssl_dir/lib"
     CPPFLAGS="$SSLCPPFLAGS $CPPFLAGS"
     LDFLAGS="$SSLLDFLAGS $LDFLAGS"])
  AC_CHECK_HEADER([openssl/ssl.h], [],
    [AC_MSG_ERROR([OpenSSL header files not found])])
  AC_CHECK_LIB([crypto], [BIO_new],
    [AC_CHECK_LIB([crypto], [DSO_load],
      [SSLLIBS='-lcrypto'],
      [AC_CHECK_LIB([dl], [DSO_load],
        [SSLLIBS='-lcrypto -ldl'],
        [AC_MSG_FAILURE([cannot link with OpenSSL])],
        [-lcrypto -ldl])])],
    [AC_MSG_FAILURE([cannot link with OpenSSL])])
  AC_CHECK_LIB([ssl], [SSL_library_init],
    [SSLLIBS="-lssl $SSLLIBS"],
    [AC_MSG_FAILURE([cannot link with OpenSSL])],
    [$SSLLIBS])
  AC_CHECK_LIB([ssl], [SSL_get_psk_identity], [:],
    [AC_MSG_ERROR([OpenSSL too old, version 1.0.0 or newer is required])],
    [$SSLLIBS])
  AC_SUBST([SSLCPPFLAGS])
  AC_SUBST([SSLLDFLAGS])
  AC_SUBST([SSLLIBS])
  CPPFLAGS=$nsca_save_CPPFLAGS
  LDFLAGS=$nsca_save_LDFLAGS
])# NSCA_LIB_OPENSSL

dnl vim:set joinspaces textwidth=80:
