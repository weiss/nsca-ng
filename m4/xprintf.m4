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

# HW_HEADER_STDARG_H
# ------------------
# Define HAVE_STDARG_H to 1 if <stdarg.h> is available.
AC_DEFUN([HW_HEADER_STDARG_H],
[
  AC_PREREQ([2.60])dnl Older releases should work if AC_CHECK_HEADERS is used.
  AC_CHECK_HEADERS_ONCE([stdarg.h])
])# HW_HEADER_STDARG_H

# HW_HEADER_VARARGS_H
# -------------------
# Define HAVE_VARARGS_H to 1 if <varargs.h> is available.
AC_DEFUN([HW_HEADER_VARARGS_H],
[
  AC_PREREQ([2.60])dnl Older releases should work if AC_CHECK_HEADERS is used.
  AC_CHECK_HEADERS_ONCE([varargs.h])
])# HW_HEADER_VARARGS_H

# HW_FUNC_VA_COPY
# ---------------
# Set $hw_cv_func_va_copy to "yes" or "no".  Define HAVE_VA_COPY to 1 if
# $hw_cv_func_va_copy is set to "yes".  Note that it's "unspecified whether
# va_copy and va_end are macros or identifiers declared with external linkage."
# (C99: 7.15.1, 1)  Therefore, the presence of va_copy(3) cannot simply "be
# tested with #ifdef", as suggested by the Autoconf manual (5.5.1).
AC_DEFUN([HW_FUNC_VA_COPY],
[
  AC_REQUIRE([HW_HEADER_STDARG_H])dnl Our check evaluates HAVE_STDARG_H.
  AC_REQUIRE([HW_HEADER_VARARGS_H])dnl Our check evaluates HAVE_VARARGS_H.
  AC_CACHE_CHECK([for va_copy],
    [hw_cv_func_va_copy],
    [AC_RUN_IFELSE(
      [AC_LANG_PROGRAM(
        [[#if HAVE_STDARG_H
          #include <stdarg.h>
          #elif HAVE_VARARGS_H
          #include <varargs.h>
          #endif]],
        [[va_list ap, aq; va_copy(aq, ap);]])],
      [hw_cv_func_va_copy=yes],
      [hw_cv_func_va_copy=no],
      [hw_cv_func_va_copy=no])])
  AS_IF([test "$hw_cv_func_va_copy" = yes],
    [AC_DEFINE([HAVE_VA_COPY], [1],
      [Define to 1 if you have the `va_copy' function or macro.])])
])# HW_FUNC_VA_COPY

# HW_FUNC___VA_COPY
# -----------------
# Set $hw_cv_func___va_copy to "yes" or "no".  Define HAVE___VA_COPY to 1 if
# $hw_cv_func___va_copy is set to "yes".
AC_DEFUN([HW_FUNC___VA_COPY],
[
  AC_REQUIRE([HW_HEADER_STDARG_H])dnl Our check evaluates HAVE_STDARG_H.
  AC_REQUIRE([HW_HEADER_VARARGS_H])dnl Our check evaluates HAVE_VARARGS_H.
  AC_CACHE_CHECK([for __va_copy],
    [hw_cv_func___va_copy],
    [AC_RUN_IFELSE(
      [AC_LANG_PROGRAM(
        [[#if HAVE_STDARG_H
          #include <stdarg.h>
          #elif HAVE_VARARGS_H
          #include <varargs.h>
          #endif]],
        [[va_list ap, aq; __va_copy(aq, ap);]])],
      [hw_cv_func___va_copy=yes],
      [hw_cv_func___va_copy=no],
      [hw_cv_func___va_copy=no])])
  AS_IF([test "$hw_cv_func___va_copy" = yes],
    [AC_DEFINE([HAVE___VA_COPY], [1],
      [Define to 1 if you have the `__va_copy' function or macro.])])
])# HW_FUNC___VA_COPY

# HW_FUNC_VSNPRINTF
# -----------------
# Set $hw_cv_func_vsnprintf and $hw_cv_func_vsnprintf_c99 to "yes" or "no",
# respectively.  Define HAVE_VSNPRINTF to 1 only if $hw_cv_func_vsnprintf_c99
# is set to "yes".  Otherwise, define vsnprintf to rpl_vsnprintf and make sure
# the replacement function will be built.
AC_DEFUN([HW_FUNC_VSNPRINTF],
[
  AC_PREREQ([2.60])dnl 2.59 should work if some AC_TYPE_* macros are replaced.
  AC_REQUIRE([HW_HEADER_STDARG_H])dnl Our check evaluates HAVE_STDARG_H.
  AC_CHECK_FUNC([vsnprintf],
    [hw_cv_func_vsnprintf=yes],
    [hw_cv_func_vsnprintf=no])
  AS_IF([test "$hw_cv_func_vsnprintf" = yes],
    [AC_CACHE_CHECK([whether vsnprintf is C99-compliant],
      [hw_cv_func_vsnprintf_c99],
      [AC_RUN_IFELSE(
        [AC_LANG_PROGRAM(
          [[#if HAVE_STDARG_H
            #include <stdarg.h>
            #endif
            #include <stdio.h>
            static int testprintf(char *buf, size_t size, const char *fmt, ...)
            {
              int result;
              va_list ap;
              va_start(ap, fmt);
              result = vsnprintf(buf, size, fmt, ap);
              va_end(ap);
              return result;
            }]],
            [[char buf[43];
            if (testprintf(buf, 4, "The answer is %27.2g.", 42.0) != 42 ||
                testprintf(buf, 0, "No, it's %32zu.", (size_t)42) != 42 ||
                buf[0] != 'T' || buf[3] != '\0')
              return 1;]])],
        [hw_cv_func_vsnprintf_c99=yes],
        [hw_cv_func_vsnprintf_c99=no],
        [hw_cv_func_vsnprintf_c99=no])])],
    [hw_cv_func_snprintf_c99=no])
  AS_IF([test "$hw_cv_func_vsnprintf_c99" = yes],
    [AC_DEFINE([HAVE_VSNPRINTF], [1],
      [Define to 1 if you have a C99-compliant `vsnprintf' function.])],
    [AC_DEFINE([vsnprintf], [rpl_vsnprintf],
      [Define to rpl_vsnprintf if the replacement function should be used.])
    AC_CHECK_HEADERS([inttypes.h locale.h stddef.h stdint.h])
    AC_CHECK_MEMBERS([struct lconv.decimal_point, struct lconv.thousands_sep],
      [], [], [[#include <locale.h>]])
    AC_TYPE_LONG_DOUBLE
    AC_TYPE_LONG_LONG_INT
    AC_TYPE_UNSIGNED_LONG_LONG_INT
    AC_TYPE_SIZE_T
    AC_TYPE_INTMAX_T
    AC_TYPE_UINTMAX_T
    AC_TYPE_UINTPTR_T
    AC_CHECK_TYPES([ptrdiff_t])
    AC_CHECK_FUNCS([localeconv])
    _HW_FUNC_XPRINTF_REPLACE])
])# HW_FUNC_VSNPRINTF

# HW_FUNC_SNPRINTF
# ----------------
# Set $hw_cv_func_snprintf and $hw_cv_func_snprintf_c99 to "yes" or "no",
# respectively.  Define HAVE_SNPRINTF to 1 only if $hw_cv_func_snprintf_c99
# is set to "yes".  Otherwise, define snprintf to rpl_snprintf and make sure
# the replacement function will be built.
AC_DEFUN([HW_FUNC_SNPRINTF],
[
  AC_REQUIRE([HW_FUNC_VSNPRINTF])dnl Our snprintf(3) calls vsnprintf(3).
  AC_CHECK_FUNC([snprintf],
    [hw_cv_func_snprintf=yes],
    [hw_cv_func_snprintf=no])
  AS_IF([test "$hw_cv_func_snprintf" = yes],
    [AC_CACHE_CHECK([whether snprintf is C99-compliant],
      [hw_cv_func_snprintf_c99],
      [AC_RUN_IFELSE(
        [AC_LANG_PROGRAM([[#include <stdio.h>]],
          [[char buf[43];
          if (snprintf(buf, 4, "The answer is %27.2g.", 42.0) != 42 ||
              snprintf(buf, 0, "No, it's %32zu.", (size_t)42) != 42 ||
              buf[0] != 'T' || buf[3] != '\0')
            return 1;]])],
        [hw_cv_func_snprintf_c99=yes],
        [hw_cv_func_snprintf_c99=no],
        [hw_cv_func_snprintf_c99=no])])],
    [hw_cv_func_snprintf_c99=no])
  AS_IF([test "$hw_cv_func_snprintf_c99" = yes],
    [AC_DEFINE([HAVE_SNPRINTF], [1],
      [Define to 1 if you have a C99-compliant `snprintf' function.])],
    [AC_DEFINE([snprintf], [rpl_snprintf],
      [Define to rpl_snprintf if the replacement function should be used.])
    _HW_FUNC_XPRINTF_REPLACE])
])# HW_FUNC_SNPRINTF

# HW_FUNC_VASPRINTF
# -----------------
# Set $hw_cv_func_vasprintf to "yes" or "no".  Define HAVE_VASPRINTF to 1 if
# $hw_cv_func_vasprintf is set to "yes".  Otherwise, define vasprintf to
# rpl_vasprintf and make sure the replacement function will be built.
AC_DEFUN([HW_FUNC_VASPRINTF],
[
  AC_REQUIRE([HW_FUNC_VSNPRINTF])dnl Our vasprintf(3) calls vsnprintf(3).
  AC_CHECK_FUNCS([vasprintf],
    [hw_cv_func_vasprintf=yes],
    [hw_cv_func_vasprintf=no])
  AS_IF([test "$hw_cv_func_vasprintf" = no],
    [AC_DEFINE([vasprintf], [rpl_vasprintf],
      [Define to rpl_vasprintf if the replacement function should be used.])
    AC_CHECK_HEADERS([stdlib.h])
    HW_FUNC_VA_COPY
    AS_IF([test "$hw_cv_func_va_copy" = no],
      [HW_FUNC___VA_COPY])
    _HW_FUNC_XPRINTF_REPLACE])
])# HW_FUNC_VASPRINTF

# HW_FUNC_ASPRINTF
# ----------------
# Set $hw_cv_func_asprintf to "yes" or "no".  Define HAVE_ASPRINTF to 1 if
# $hw_cv_func_asprintf is set to "yes".  Otherwise, define asprintf to
# rpl_asprintf and make sure the replacement function will be built.
AC_DEFUN([HW_FUNC_ASPRINTF],
[
  AC_REQUIRE([HW_FUNC_VASPRINTF])dnl Our asprintf(3) calls vasprintf(3).
  AC_CHECK_FUNCS([asprintf],
    [hw_cv_func_asprintf=yes],
    [hw_cv_func_asprintf=no])
  AS_IF([test "$hw_cv_func_asprintf" = no],
    [AC_DEFINE([asprintf], [rpl_asprintf],
      [Define to rpl_asprintf if the replacement function should be used.])
    _HW_FUNC_XPRINTF_REPLACE])
])# HW_FUNC_ASPRINTF

# _HW_FUNC_XPRINTF_REPLACE
# ------------------------
# Arrange for building xprintf.c.  Must be called if one or more of the
# functions provided by xprintf.c are needed.
AC_DEFUN([_HW_FUNC_XPRINTF_REPLACE],
[
  AS_IF([test "x$_hw_cv_func_xprintf_replace_done" != xyes],
    [AC_C_CONST
    HW_HEADER_STDARG_H
    AC_LIBOBJ([xprintf])
    _hw_cv_func_xprintf_replace_done=yes])
])# _HW_FUNC_XPRINTF_REPLACE

dnl vim:set joinspaces textwidth=80:
