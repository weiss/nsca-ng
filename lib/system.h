/*
 * Copyright (c) 2013 Holger Weiss <holger@weiss.in-berlin.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NSCA_NG_H
# define NSCA_NG_H

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# include <stdio.h> /* For size_t. */

/*
 * Turn __attribute__() into a no-op unless GCC is used.
 */
# ifndef __GNUC__
#  define __attribute__(x) /* Nothing. */
# endif

/*
 * Define the bool type.
 */
# if HAVE_STDBOOL_H
#  include <stdbool.h>
# else
#  if HAVE__BOOL
#   ifdef __cplusplus
typedef bool _Bool;
#   else
#    define _Bool signed char
#   endif
#  endif
#  define bool _Bool
#  define false 0
#  define true 1
#  define __bool_true_false_are_defined 1
# endif

/*
 * Make sure EWOULDBLOCK is defined.
 */
# ifndef EWOULDBLOCK
#  define EWOULDBLOCK EAGAIN
# endif

/*
 * Define a MIN() macro.
 */
# ifdef MIN
#  undef MIN
# endif
# define MIN(x, y) ((x) < (y) ? (x) : (y))

/*
 * For building without systemd(1) support.
 */
# ifndef SD_LISTEN_FDS_START
#  define SD_LISTEN_FDS_START 3
# endif
# ifndef HAVE_SYSTEMD_SD_DAEMON_H
#  define sd_notify(a, b) 0
#  define sd_listen_fds(a) 0
# endif

/*
 * Declare replacement functions (if necessary).
 */
# ifndef HAVE_DAEMON
int daemon(int, int);
# endif
# ifndef HAVE_GETPROGNAME
const char *getprogname(void);
# endif
# ifndef HAVE_SETPROGNAME
void setprogname(const char *);
# endif
# ifndef HAVE_STRDUP
char *strdup(const char *);
# endif
# ifndef HAVE_STRCASECMP
int strcasecmp(const char *, const char *);
# endif
# ifndef HAVE_STRNCASECMP
int strncasecmp(const char *, const char *, size_t);
# endif

# if !HAVE_SNPRINTF || !HAVE_VSNPRINTF || !HAVE_ASPRINTF || !HAVE_VASPRINTF
#  if HAVE_STDARG_H
#   include <stdarg.h>
#   if !HAVE_VSNPRINTF
int rpl_vsnprintf(char *, size_t, const char *, va_list);
#   endif
#   if !HAVE_SNPRINTF
int rpl_snprintf(char *, size_t, const char *, ...);
#   endif
#   if !HAVE_VASPRINTF
int rpl_vasprintf(char **, const char *, va_list);
#   endif
#   if !HAVE_ASPRINTF
int rpl_asprintf(char **, const char *, ...);
#   endif
#  endif
# endif

#endif

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
