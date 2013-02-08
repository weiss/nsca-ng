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

#include "log.h"
#include "system.h"
#include "wrappers.h"

void *
xmalloc(size_t size)
{
	void *new;

	if ((new = malloc(size)) == NULL)
		die("Error: Cannot allocate %zu bytes", size);

	return new;
}

void *
xrealloc(void *p, size_t size)
{
	void *new;

	if ((new = realloc(p, size)) == NULL) {
		free(p);
		die("Cannot reallocate %zu bytes: %m", size);
	}
	return new;
}

char *
xstrdup(const char *string)
{
	char *new;

	if ((new = strdup(string)) == NULL)
		die("Cannot duplicate string: %m");

	return new;
}

void
xasprintf(char ** restrict result, const char * restrict format, ...)
{
	va_list ap;

	va_start(ap, format);
	xvasprintf(result, format, ap);
	va_end(ap);
}

void
xvasprintf(char ** restrict result, const char * restrict format, va_list ap)
{
	if (vasprintf(result, format, ap) < 0)
		die("Cannot create buffer with formatted output");
}

size_t
xfgets(char * restrict buf, int size, FILE * restrict fp)
{
	size_t len;

	if (fgets(buf, size, fp) != NULL)
		len = strlen(buf);
	else {
		if (ferror(fp))
			die("Cannot read input stream: %m");
		len = 0;
	}
	return len;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
