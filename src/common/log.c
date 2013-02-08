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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "log.h"
#include "system.h"
#include "wrappers.h"

#ifndef LOG_BUFFER_SIZE
# define LOG_BUFFER_SIZE 768
#endif

int log_level = LOG_LEVEL_INFO;

static int log_target = LOG_TARGET_STDERR;
static bool log_opened = false;

static inline const char *level_to_string(int);

/*
 * Exported functions.
 */

void
log_set(int level, int flags)
{
	if (level != -1)
		log_level = level;
	if (flags != -1) {
		log_target = flags;
		if (flags & LOG_TARGET_SYSLOG && !log_opened) {
			openlog(getprogname(), LOG_PID, LOG_DAEMON);
			log_opened = true;
		}
	}
}

void
vlog(int level, const char *in_fmt, va_list ap)
{
	size_t in_pos, out_pos;
	char message[LOG_BUFFER_SIZE], out_fmt[512];
	enum {
		STATE_NORMAL,
		STATE_PERCENT
	} state = STATE_NORMAL;

	/*
	 * This loop handles `%m'.
	 *
	 * Why do we check for "out_pos < sizeof(out_fmt) - 2"?  We need one
	 * extra character when dealing with input such as `%s' (as we write
	 * both of these characters in one iteration), and another one for
	 * nul-termination.
	 */
	for (in_pos = out_pos = 0; in_fmt[in_pos] != '\0'
	    && out_pos < sizeof(out_fmt) - 2; in_pos++)
		switch (in_fmt[in_pos]) {
		case '%':
			if (state == STATE_PERCENT) {
				out_fmt[out_pos++] = '%';
				state = STATE_NORMAL;
			} else
				state = STATE_PERCENT;
			break;
		case 'm':
			if (state == STATE_PERCENT) {
				const char *err_str = strerror(errno);
				size_t err_pos;

				for (err_pos = 0; err_str[err_pos] != '\0'
				    && out_pos < sizeof(out_fmt) - 1; err_pos++)
					out_fmt[out_pos++] = err_str[err_pos];
				state = STATE_NORMAL;
				break;
			} /* Otherwise, fall through. */
		default:
			if (state == STATE_PERCENT)
				out_fmt[out_pos++] = '%';
			out_fmt[out_pos++] = in_fmt[in_pos];
			state = STATE_NORMAL;
		}

	out_fmt[out_pos] = '\0';

	if (vsnprintf(message, sizeof(message), out_fmt, ap)
	    > (int)sizeof(message) - 1)
		(void)memcpy(message + sizeof(message) - 7, " [...]", 7);

	if (log_target & LOG_TARGET_STDERR)
		(void)fprintf(stderr, "%s: [%s] %s\n", getprogname(),
		    level_to_string(level), message);
	if (log_target & LOG_TARGET_SYSLOG)
		syslog(level, "[%s] %s", level_to_string(level), message);
}

void
log_close(void)
{
	closelog();
}

/*
 * Static functions.
 */

static inline const char *
level_to_string(int level)
{
	switch (level) {
	case LOG_CRIT:
		return "FATAL";
	case LOG_ERR:
		return "ERROR";
	case LOG_WARNING:
		return "WARNING";
	case LOG_NOTICE:
		return "NOTICE";
	case LOG_INFO:
		return "INFO";
	case LOG_DEBUG:
		return "DEBUG";
	default:
		return "UNKNOWN";
	}
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
