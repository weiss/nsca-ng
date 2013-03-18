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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ev.h>
#include <openssl/crypto.h>

#include "log.h"
#include "system.h"
#include "util.h"
#include "wrappers.h"

static const char *get_ev_backend(EV_P);
static const char *get_openssl_version(void);

/*
 * Exported functions.
 */

char *
concat(const char *str1, const char *str2)
{
	size_t len1 = strlen(str1);
	size_t len2 = strlen(str2);
	size_t size;
	char *catenated;

	if (((size = len1 + len2) <= len1 && len2 != 0) || ++size == 0)
		die("String concatenation would overflow");

	catenated = xmalloc(size);
	(void)memcpy(catenated, str1, len1);
	(void)memcpy(catenated + len1, str2, len2);
	catenated[len1 + len2] = '\0';

	return catenated;
}

bool
parse_line(char * restrict line, char ** restrict args, int n_args)
{
	char *p;
	int i;

	for (i = 0, p = strtok(line, " \t"); p != NULL;
	    i++, p = strtok(NULL, " \t"))
		if (i < n_args)
			args[i] = p;
		else {
			if (i > 0)
				debug("%s message has more than %d argument(s)",
				    args[0], n_args - 1);
			return false;
		}

	if (i > 0)
		debug("%s message has %d argument(s)", args[0], i - 1);

	return (bool)(i == n_args);
}

char *
skip_newlines(const char *string)
{
	const char *p = string;

	while (*p == '\r' || *p == '\n')
		p++;

	return (char *)p;
}

char *
skip_whitespace(const char *string)
{
	const char *p = string;

	while (*p == ' ' || *p == '\t')
		p++;

	return (char *)p;
}

void
chomp(char *string)
{
	size_t len;

	if ((len = strlen(string)) > 0 && string[len - 1] == '\n')
		string[len - 1] = '\0';
}

const char *
nsca_version(void)
{
	static char version_string[128];

	(void)snprintf(version_string, sizeof(version_string),
	    "%s %s (%s, libev %d.%d with %s)",
	    getprogname(),
	    NSCA_VERSION,
	    get_openssl_version(),
	    ev_version_major(),
	    ev_version_minor(),
	    get_ev_backend(EV_DEFAULT_UC));

	return version_string;
}

/*
 * Static functions.
 */

static const char *
get_ev_backend(EV_P)
{
	switch (ev_backend(EV_A)) {
	case EVBACKEND_SELECT:
		return "select";
	case EVBACKEND_POLL:
		return "poll";
	case EVBACKEND_EPOLL:
		return "epoll";
	case EVBACKEND_KQUEUE:
		return "kqueue";
	case EVBACKEND_DEVPOLL:
		return "/dev/poll";
	case EVBACKEND_PORT:
		return "event port";
	default:
		return "(unknown)";
	}
}

static const char *
get_openssl_version(void)
{
	static char version_string[32];
	const char *p;
	size_t i, spaces = 0;

	for (i = 0, p = SSLeay_version(SSLEAY_VERSION);
	    i < sizeof(version_string) - 1 && *p != '\0';
	    i++, p++) {
		if (*p == ' ')
			spaces++;
		if (spaces == 2)
			break;
		version_string[i] = *p;
	}
	version_string[++i] = '\0';
	return version_string;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
