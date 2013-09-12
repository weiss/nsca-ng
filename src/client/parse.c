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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "parse.h"
#include "system.h"
#include "util.h"
#include "wrappers.h"

static char *escape(const char *);

/*
 * Exported functions.
 */

char *
parse_command(const char *line)
{
	char *command;

	debug("Parsing monitoring command");

	line = skip_whitespace(line);
	if (line[0] == '[')
		command = xstrdup(line);
	else
		xasprintf(&command, "[%lu] %s", (unsigned long)time(NULL),
		    line);

	return command;
}

char *
parse_check_result(const char *input, char delimiter)
{
	const char *fields[4] = { NULL, NULL, NULL, NULL };
	char *command, *escaped;
	int lengths[3];
	int n, pos, start_pos;

	debug("Parsing check result");

	if (strpbrk(input, "\\\n") != NULL)
		input = escaped = escape(input);
	else
		escaped = NULL;

	fields[0] = input;
	start_pos = 0;
	n = 1;

	for (pos = 0; n < 4 && input[pos] != '\0'; pos++)
		if (input[pos] == delimiter) {
			lengths[n - 1] = pos - start_pos;
			debug("Check result field %d has %d characters (%d-%d)",
			    n, lengths[n - 1], start_pos, pos - 1);

			/* Handle the next field. */
			start_pos = pos + 1;
			fields[n] = &input[start_pos];
			n++;
		}

	switch (n) {
	case 3:
		debug("Got host check result");
		xasprintf(&command,
		    "[%lu] PROCESS_HOST_CHECK_RESULT;%.*s;%.*s;%s",
		    (unsigned long)time(NULL),
		    lengths[0], fields[0],
		    lengths[1], fields[1],
		    fields[2]);
		break;
	case 4:
		debug("Got service check result");
		xasprintf(&command,
		    "[%lu] PROCESS_SERVICE_CHECK_RESULT;%.*s;%.*s;%.*s;%s",
		    (unsigned long)time(NULL),
		    lengths[0], fields[0],
		    lengths[1], fields[1],
		    lengths[2], fields[2],
		    fields[3]);
		break;
	default:
		die("Input format incorrect, see the %s(8) man page",
		    getprogname());
	}

	if (escaped != NULL)
		free(escaped);

	return command;
}

/*
 * Static functions.
 */

static char *
escape(const char *input)
{
	const char *in;
	char *escaped, *out;
	size_t size = strlen(input) + 1;

	for (in = input; *in != '\0'; in++)
		if (*in == '\\' || *in == '\n')
			size++;

	escaped = xmalloc(size);

	for (in = input, out = escaped; *in != '\0'; in++, out++)
		switch (*in) {
		case '\\':
			*out++ = '\\';
			*out = '\\';
			break;
		case '\n':
			*out++ = '\\';
			*out = 'n';
			break;
		default:
			*out = *in;
		}
	*out = '\0';

	return escaped;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
