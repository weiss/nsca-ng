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

#ifndef SESSION_H
# define SESSION_H

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# include <stdarg.h>
# include <stdlib.h>
# include <syslog.h>

# include "system.h"

# define LOG_TARGET_SYSLOG 0x1
# define LOG_TARGET_STDERR 0x2
# define LOG_TARGET_SYSTEMD 0x4

enum {
	LOG_LEVEL_CRITICAL,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_NOTICE,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG
};

extern int log_level;

void log_set(int, int);
void vlog(int, const char *, va_list);
void log_close(void);

static inline void __attribute__((__format__(__printf__, 1, 2)))
debug(const char *format, ...)
{
	if (log_level >= LOG_LEVEL_DEBUG) {
		va_list ap;

		va_start(ap, format);
		vlog(LOG_DEBUG, format, ap);
		va_end(ap);
	}
}

static inline void __attribute__((__format__(__printf__, 1, 2)))
info(const char *format, ...)
{
	if (log_level >= LOG_LEVEL_INFO) {
		va_list ap;

		va_start(ap, format);
		vlog(LOG_INFO, format, ap);
		va_end(ap);
	}
}

static inline void __attribute__((__format__(__printf__, 1, 2)))
notice(const char *format, ...)
{
	if (log_level >= LOG_LEVEL_NOTICE) {
		va_list ap;

		va_start(ap, format);
		vlog(LOG_NOTICE, format, ap);
		va_end(ap);
	}
}

static inline void __attribute__((__format__(__printf__, 1, 2)))
warning(const char *format, ...)
{
	if (log_level >= LOG_LEVEL_WARNING) {
		va_list ap;

		va_start(ap, format);
		vlog(LOG_WARNING, format, ap);
		va_end(ap);
	}
}

static inline void __attribute__((__format__(__printf__, 1, 2)))
error(const char *format, ...)
{
	if (log_level >= LOG_LEVEL_ERROR) {
		va_list ap;

		va_start(ap, format);
		vlog(LOG_ERR, format, ap);
		va_end(ap);
	}
}

static inline void __attribute__((__format__(__printf__, 1, 2)))
critical(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vlog(LOG_CRIT, format, ap);
	va_end(ap);
}

static inline void __attribute__((__format__(__printf__, 1, 2), __noreturn__))
die(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vlog(LOG_CRIT, format, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

#endif

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
