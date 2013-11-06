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

#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include <confuse.h>
#include <openssl/ssl.h>

#include "auth.h"
#include "hash.h"
#include "log.h"
#include "system.h"
#include "util.h"

static bool match(regex_t * restrict, const char * restrict);

/*
 * Exported functions.
 */

unsigned int
check_psk(SSL *ssl __attribute__((__unused__)), const char *identity,
          unsigned char *password, unsigned int max_password_len)
{
	cfg_t *auth;
	const char *configured_pw;
	size_t password_len;

	if ((auth = hash_lookup(identity)) == NULL
	    && (auth = hash_lookup("*")) == NULL) {
		warning("Client-supplied ID `%s' is unknown", identity);
		return 0;
	}
	debug("Verifying key provided by %s", identity);

	configured_pw = cfg_getstr(auth, "password");
	password_len = MIN(strlen(configured_pw), max_password_len);
	(void)memcpy(password, configured_pw, password_len);
	return (unsigned int)password_len;
}

bool
is_authorized(const char * restrict identity, const char * restrict command)
{
	const char *settings[] = { "hosts", "services", "commands" };
	cfg_t *auth;
	char *newline;
	size_t i;

	if ((auth = hash_lookup(identity)) == NULL
	    && (auth = hash_lookup("*")) == NULL) {
		/* Shouldn't happen, as the client is authenticated. */
		error("Cannot find authorizations for %s", identity);
		return false;
	}
	if ((newline = strchr(command, '\n')) == NULL) {
		warning("Command submitted by %s isn't newline-terminated",
		    identity);
		return false;
	}
	if (*(newline + 1) != '\0') {
		warning("Command submitted by %s contains embedded newline(s)",
		    identity);
		return false;
	}
	/* Match against the command without the leading bracketed timestamp. */
	if ((command = strchr(command, ']')) == NULL) {
		warning("Timestamp missing from command submitted by %s",
		    identity);
		return false;
	}
	command = skip_whitespace(command + 1);

	for (i = 0; i < sizeof(settings) / sizeof(*settings); i++) {
		cfg_opt_t *opt = cfg_getopt(auth, settings[i]);
		unsigned int j;

		for (j = 0; j < cfg_opt_size(opt); j++) {
			regex_t *pattern = cfg_opt_getnptr(opt, j);

			if (match(pattern, command))
				return true;
		}
	}
	return false;
}

/*
 * Static functions.
 */

static bool
match(regex_t * restrict pattern, const char * restrict command)
{
	char errbuf[128];
	int result = regexec(pattern, command, 0, NULL, 0);

	switch (result) {
	case 0:
		return true;
	case REG_NOMATCH:
		return false;
	default:
		(void)regerror(result, pattern, errbuf, sizeof(errbuf));
		error("Error matching command: %s", errbuf);
		return false;
	}
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
