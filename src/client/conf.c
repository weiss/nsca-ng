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
#include <unistd.h>

#include "conf.h"
#include "log.h"
#include "system.h"
#include "util.h"
#include "wrappers.h"

#define DEFAULT_PASSWORD "change-me"
#define DEFAULT_PORT "5668"
#define DEFAULT_SERVER "localhost"
#define DEFAULT_TIMEOUT 15
#define DEFAULT_TLS_CIPHERS \
    "PSK-AES256-CBC-SHA:PSK-AES128-CBC-SHA:PSK-3DES-EDE-CBC-SHA:PSK-RC4-SHA"

struct conf_s { /* This is typedef'd to `conf' in conf.h. */
	const char *key;
	enum {
		TYPE_NONE,
		TYPE_STRING,
		TYPE_INTEGER
	} type;
	union {
		char *s;
		long i;
	} value;
};

static unsigned long line_number = 0;

static conf *lookup_conf(conf * restrict, const char * restrict);
static void parse_conf_file(const char * restrict, conf * restrict);
static char *read_conf_line(FILE *);

/*
 * Exported functions.
 */

conf *
conf_init(const char *path)
{
	char host_name[256];
	static conf cfg[] = {
		{ "delay", TYPE_INTEGER, { 0 } },
		{ "encryption_method", TYPE_STRING, { NULL } },
		{ "identity", TYPE_STRING, { NULL } },
		{ "password", TYPE_STRING, { NULL } },
		{ "port", TYPE_STRING, { NULL } },
		{ "server", TYPE_STRING, { NULL } },
		{ "timeout", TYPE_INTEGER, { 0 } },
		{ "tls_ciphers", TYPE_STRING, { NULL } },
		{ NULL, TYPE_NONE, { NULL } }
	};

	debug("Initializing configuration context");

	if (gethostname(host_name, sizeof(host_name)) == -1)
		die("Cannot get host name: %m");

	conf_setstr(cfg, "identity", host_name);
	conf_setstr(cfg, "password", DEFAULT_PASSWORD);
	conf_setstr(cfg, "port", DEFAULT_PORT);
	conf_setstr(cfg, "server", DEFAULT_SERVER);
	conf_setint(cfg, "timeout", DEFAULT_TIMEOUT);
	conf_setstr(cfg, "tls_ciphers", DEFAULT_TLS_CIPHERS);

	parse_conf_file(path, cfg);

	return cfg;
}

long
conf_getint(conf * restrict cfg, const char * restrict key)
{
	conf *c = lookup_conf(cfg, key);

	return c->value.i;
}

char *
conf_getstr(conf * restrict cfg, const char * restrict key)
{
	conf *c = lookup_conf(cfg, key);

	return c->value.s;
}

void
conf_setint(conf * restrict cfg, const char * restrict key, long value)
{
	conf *c = lookup_conf(cfg, key);

	c->value.i = value;
}

void
conf_setstr(conf * restrict cfg, const char * restrict key,
            const char * restrict value)
{
	conf *c = lookup_conf(cfg, key);

	if (c->value.s != NULL)
		free(c->value.s);

	c->value.s = xstrdup(value);
}

void
conf_free(conf *cfg)
{
	conf *c;

	debug("Destroying configuration context");

	for (c = cfg; c->key != NULL; c++)
		if (c->type == TYPE_STRING && c->value.s != NULL)
			free(c->value.s);
}

/*
 * Static functions.
 */

static conf *
lookup_conf(conf * restrict cfg, const char * restrict key)
{
	conf *c;

	for (c = cfg; c->key != NULL && strcmp(key, c->key) != 0; c++)
		continue;
	if (c->key == NULL)
		die("Unknown variable `%s'", key);

	return c;
}

static void
parse_conf_file(const char * restrict path, conf * restrict cfg)
{
	FILE *f;
	char *line;

	if ((f = fopen(path, "r")) == NULL)
		die("Cannot open %s: %m", path);

	while ((line = read_conf_line(f)) != NULL) {
		conf *c;
		char *token, *in, *out;
		char quote_sign, value[2048];
		size_t len;
		bool done = false, escape = false;

		/*
		 * Parse the key.
		 */
		token = skip_whitespace(line);

		if (*token == '\0' || *token == '#') { /* The line is empty. */
			free(line);
			continue;
		}
		if ((len = strcspn(token, " \t=")) == 0)
			die("%s:%lu: Cannot parse line", path, line_number);
		for (c = cfg; c->key != NULL && (strlen(c->key) != len
		    || strncmp(token, c->key, len) != 0); c++)
			continue;
		if (c->key == NULL)
			die("%s:%lu: Unknown variable name `%.*s'", path,
			    line_number, (int)len, token);

		/*
		 * Eat the `='.
		 */
		token = skip_whitespace(token + len);

		if (*token != '=')
			die("%s:%lu: Expected `=' after `%s'", path,
			    line_number, c->key);

		/*
		 * Parse the value.
		 */
		token = skip_whitespace(token + 1);

		switch (*token) {
		case '#':
		case '\0':
			die("%s:%lu: No value assigned to `%s'", path,
			    line_number, c->key);
		case '"':
		case '\'':
			quote_sign = *token;
			token++;
			break;
		default:
			quote_sign = 0;
		}
		for (in = token, out = value, len = 0;
		    *in != '\0' && !done; in++) {
			if (len >= sizeof(value) - 1)
				die("%s:%lu: Value of `%s' is too long", path,
				    line_number, c->key);
			if (escape) {
				*out++ = *in;
				len++;
				escape = false;
				continue;
			}
			if (*in == quote_sign || (quote_sign == 0 &&
			    (*in == '#' || *in == ' ' || *in == '\t')))
				done = true;
			else if (*in == '\\')
				escape = true;
			else {
				*out++ = *in;
				len++;
			}
		}
		value[len] = '\0';

		/*
		 * Check the end of the line.
		 */
		token = skip_whitespace(in);

		if (*token != '\0' && *token != '#')
			die("%s:%lu: Unexpected stuff after `%s'", path,
			    line_number, value);

		debug("%s:%lu: %s = %s", path, line_number, c->key, value);

		/*
		 * Save the value.
		 */
		if (c->type == TYPE_STRING)
			conf_setstr(cfg, c->key, value);
		else {
			char *end;
			long num = strtol(value, &end, 0);

			if (*end != '\0')
				die("%s:%lu: Nonnumeric value assigned to `%s'",
				    path, line_number, c->key);

			conf_setint(cfg, c->key, num);
		}

		/* Forget this copy of the value, in case it's sensitive. */
		(void)memset(value, 0, strlen(value));
		(void)memset(line, 0, strlen(line));
		free(line);
	}

	if (fclose(f) == EOF)
		die("Cannot close %s: %m", path);
}

static char *
read_conf_line(FILE *f)
{
	size_t len, pos = 0, size = 512;
	char *line = xmalloc(size);

	while ((len = xfgets(line + pos, (int)(size - pos), f)) > 0) {
		pos += len;
		if (line[pos - 1] != '\n' && pos == size - 1)
			line = xrealloc(line, size *= 2);
		else {
			line_number++;
			if (pos >= 2 && line[pos - 2] == '\r') {
				line[pos - 2] = '\n';
				pos--;
			}
			if (pos >= 2 && line[pos - 2] == '\\')
				pos -= 2;
			else
				break;
		}
	}
	if (pos == 0) { /* EOF */
		free(line);
		return NULL;
	}
	chomp(line);
	return xrealloc(line, pos + 1);
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
