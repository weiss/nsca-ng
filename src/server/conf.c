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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "hash.h"
#include "log.h"
#include "system.h"
#include "wrappers.h"

#define DEFAULT_COMMAND_FILE LOCALSTATEDIR "/nagios/rw/nagios.cmd"
#define DEFAULT_LISTEN "*"
#define DEFAULT_LOG_LEVEL LOG_LEVEL_NOTICE
#define DEFAULT_MAX_COMMAND_SIZE 16384
#define DEFAULT_MAX_QUEUE_SIZE 1024
#define DEFAULT_TEMP_DIRECTORY "/tmp"
#define DEFAULT_TIMEOUT 60.0 /* For considerations, see RFC 5482, section 6. */
#define DEFAULT_TLS_CIPHERS \
    "PSK-AES256-CBC-SHA:PSK-AES128-CBC-SHA:PSK-3DES-EDE-CBC-SHA:PSK-RC4-SHA"

static void hash_auth_blocks(cfg_t *);
static char *service_to_command(const char *);
static int parse_service_pattern_cb(cfg_t * restrict, cfg_opt_t * restrict,
                                    const char * restrict, void * restrict);
static int parse_command_pattern_cb(cfg_t * restrict, cfg_opt_t * restrict,
                                    const char * restrict, void * restrict);
static void free_auth_pattern_cb(void *);
static int validate_unsigned_int_cb(cfg_t *, cfg_opt_t *);
static int validate_unsigned_float_cb(cfg_t *, cfg_opt_t *);

/*
 * Exported functions.
 */

cfg_t *
conf_parse(const char *path)
{
	cfg_opt_t auth_opts[] = {
		CFG_STR("password", NULL, CFGF_NODEFAULT),
		CFG_PTR_LIST_CB("commands", NULL, CFGF_NODEFAULT,
		    parse_command_pattern_cb, free_auth_pattern_cb),
		CFG_PTR_LIST_CB("services", NULL, CFGF_NODEFAULT,
		    parse_service_pattern_cb, free_auth_pattern_cb),
		CFG_END()
	};
	cfg_opt_t opts[] = {
		CFG_FUNC("include", cfg_include),
		CFG_STR("command_file", DEFAULT_COMMAND_FILE, CFGF_NONE),
		CFG_STR("listen", DEFAULT_LISTEN, CFGF_NONE),
		CFG_INT("log_level", DEFAULT_LOG_LEVEL, CFGF_NONE),
		CFG_INT("max_command_size", DEFAULT_MAX_COMMAND_SIZE, CFGF_NONE),
		CFG_INT("max_queue_size", DEFAULT_MAX_QUEUE_SIZE, CFGF_NONE),
		CFG_STR("pid_file", NULL, CFGF_NODEFAULT),
		CFG_STR("temp_directory", DEFAULT_TEMP_DIRECTORY, CFGF_NONE),
		CFG_STR("tls_ciphers", DEFAULT_TLS_CIPHERS, CFGF_NONE),
		CFG_FLOAT("timeout", DEFAULT_TIMEOUT, CFGF_NONE),
		CFG_STR("user", NULL, CFGF_NODEFAULT),
		CFG_SEC("authorize", auth_opts,
		    CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
		CFG_END()
	};
	cfg_t *cfg = cfg_init(opts, CFGF_NONE); /* Aborts on error. */

	cfg_set_validate_func(cfg, "max_command_size",
	    validate_unsigned_int_cb);
	cfg_set_validate_func(cfg, "max_queue_size",
	    validate_unsigned_int_cb);
	cfg_set_validate_func(cfg, "log_level",
	    validate_unsigned_int_cb);
	cfg_set_validate_func(cfg, "timeout",
	    validate_unsigned_float_cb);

	debug("Parsing configuration file %s", path);

	if (access(path, R_OK) == -1) /* libConfuse won't complain. */
		die("Cannot read %s: %m", path);
	if (cfg_parse(cfg, path) == CFG_PARSE_ERROR)
		exit(EXIT_FAILURE);

	hash_auth_blocks(cfg);
	return cfg;
}

/*
 * Static functions.
 */

static void
hash_auth_blocks(cfg_t *cfg)
{
	unsigned int i, n_auth_blocks;

	if ((n_auth_blocks = cfg_size(cfg, "authorize")) == 0)
		die("No authorizations configured");

	hash_new((size_t)(n_auth_blocks /* Minimize collisions: */ * 1.5));

	for (i = 0; i < n_auth_blocks; i++) {
		cfg_t *auth = cfg_getnsec(cfg, "authorize", i);
		const char *identity = cfg_title(auth);

		debug("Parsing authorizations for %s", identity);

		if (cfg_size(auth, "password") == 0)
			die("No password specified for %s", identity);

		hash_insert(identity, auth);
	}
}

static char *
service_to_command(const char *service_pattern)
{
	const char *host_part, *service_part;
	char *at, *command_pattern, *pattern = xstrdup(service_pattern);

	if ((at = strrchr(pattern, '@')) == NULL)
		host_part = "[^;]+";
	else {
		host_part = at + 1;
		*at = '\0';
	}
	service_part = pattern;

	xasprintf(&command_pattern,
	    "PROCESS_SERVICE_CHECK_RESULT;%s;%s;.+;.+",
	    host_part, service_part);

	free(pattern);
	return command_pattern;
}

static int
parse_service_pattern_cb(cfg_t * restrict cfg, cfg_opt_t * restrict opt,
                         const char * restrict value, void * restrict result)
{
	char *command_pattern = service_to_command(value);
	int success = parse_command_pattern_cb(cfg, opt, command_pattern,
	    result);

	free(command_pattern);
	return success;
}

static int
parse_command_pattern_cb(cfg_t * restrict cfg, cfg_opt_t * restrict opt,
                         const char * restrict value, void * restrict result)
{
	regex_t **re = result;
	char *pattern;
	char errbuf[128];
	int success;

	xasprintf(&pattern, "^%s\n?$", value);
	*re = xmalloc(sizeof(regex_t));
	if ((success = regcomp(*re, pattern, REG_EXTENDED | REG_NOSUB)) != 0) {
		(void)regerror(success, *re, errbuf, sizeof(errbuf));
		cfg_error(cfg, "Error in `%s' pattern `%s': %s", opt->name,
		    value, errbuf);
	}
	free(pattern);
	return success;
}

static void
free_auth_pattern_cb(void *value)
{
	regfree(value);
	free(value);
}

/*
 * See <http://www.nongnu.org/confuse/tutorial-html/ar01s06.html>.
 */
static int
validate_unsigned_int_cb(cfg_t * restrict cfg, cfg_opt_t * restrict opt)
{
	long int value = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);

	if (value < 0) {
		cfg_error(cfg, "`%s' must be a positive integer", opt->name);
		return -1; /* Abort. */
	}
	return 0;
}

static int
validate_unsigned_float_cb(cfg_t * restrict cfg, cfg_opt_t * restrict opt)
{
	double value = cfg_opt_getnfloat(opt, cfg_opt_size(opt) - 1);

	if (value < 0.0) {
		cfg_error(cfg, "`%s' must be a positive value", opt->name);
		return -1; /* Abort. */
	}
	return 0;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
