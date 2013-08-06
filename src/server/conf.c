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
#include <sys/stat.h>
#include <errno.h>
#include <ftw.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "hash.h"
#include "log.h"
#include "system.h"
#include "wrappers.h"

#define MAX_INCLUDE 1000000UL
#define DEFAULT_COMMAND_FILE LOCALSTATEDIR "/nagios/rw/nagios.cmd"
#define DEFAULT_LISTEN "*"
#define DEFAULT_LOG_LEVEL LOG_LEVEL_NOTICE
#define DEFAULT_MAX_COMMAND_SIZE 16384
#define DEFAULT_MAX_QUEUE_SIZE 1024
#define DEFAULT_TEMP_DIRECTORY "/tmp"
#define DEFAULT_TIMEOUT 60.0 /* For considerations, see RFC 5482, section 6. */
#define DEFAULT_TLS_CIPHERS \
    "PSK-AES256-CBC-SHA:PSK-AES128-CBC-SHA:PSK-3DES-EDE-CBC-SHA:PSK-RC4-SHA"

static unsigned long n_included = 0;
static cfg_t *include_cfg;

static int parse_include(cfg_t * restrict, const char * restrict);
static void check_parse_success(int);
static void process_auth_sections(cfg_t *);
static void validate_auth_section(cfg_t *);
static void fallback_to_defaults(cfg_t * restrict, cfg_t * restrict);
static char *host_to_command(const char *);
static char *service_to_command(const char *);
static int parse_host_pattern_cb(cfg_t * restrict, cfg_opt_t * restrict,
                                 const char * restrict, void * restrict);
static int parse_service_pattern_cb(cfg_t * restrict, cfg_opt_t * restrict,
                                    const char * restrict, void * restrict);
static int parse_command_pattern_cb(cfg_t * restrict, cfg_opt_t * restrict,
                                    const char * restrict, void * restrict);
static void free_auth_pattern_cb(void *);
static int validate_unsigned_int_cb(cfg_t *, cfg_opt_t *);
static int validate_unsigned_float_cb(cfg_t *, cfg_opt_t *);
static int include_cb(cfg_t * restrict, cfg_opt_t * restrict, int,
                      const char ** restrict);
static int include_file_cb(const char *, const struct stat *, int,
                           struct FTW *);

/*
 * Exported functions.
 */

cfg_t *
conf_parse(const char *path)
{
	struct stat sb;

	/*
	 * To let the user specify a global default value for an "authorize"
	 * setting, also add the setting as a CFG_STR to the list of global
	 * options, and add the setting's name to the "settings" array in the
	 * fallback_to_defaults() function.  This should be done for all
	 * authorization settings.
	 */
	cfg_opt_t auth_opts[] = {
		CFG_STR("password", NULL, CFGF_NODEFAULT),
		CFG_PTR_LIST_CB("commands", NULL, CFGF_NODEFAULT,
		    parse_command_pattern_cb, free_auth_pattern_cb),
		CFG_PTR_LIST_CB("hosts", NULL, CFGF_NODEFAULT,
		    parse_host_pattern_cb, free_auth_pattern_cb),
		CFG_PTR_LIST_CB("services", NULL, CFGF_NODEFAULT,
		    parse_service_pattern_cb, free_auth_pattern_cb),
		CFG_END()
	};
	cfg_opt_t opts[] = {
		CFG_FUNC("include", include_cb),
		CFG_STR("chroot", NULL, CFGF_NODEFAULT),
		CFG_STR("command_file", DEFAULT_COMMAND_FILE, CFGF_NONE),
		CFG_STR("commands", NULL, CFGF_NODEFAULT),
		CFG_STR("hosts", NULL, CFGF_NODEFAULT),
		CFG_STR("listen", DEFAULT_LISTEN, CFGF_NONE),
		CFG_INT("log_level", DEFAULT_LOG_LEVEL, CFGF_NONE),
		CFG_INT("max_command_size", DEFAULT_MAX_COMMAND_SIZE, CFGF_NONE),
		CFG_INT("max_queue_size", DEFAULT_MAX_QUEUE_SIZE, CFGF_NONE),
		CFG_STR("password", NULL, CFGF_NODEFAULT),
		CFG_STR("pid_file", NULL, CFGF_NODEFAULT),
		CFG_STR("services", NULL, CFGF_NODEFAULT),
		CFG_STR("temp_directory", DEFAULT_TEMP_DIRECTORY, CFGF_NONE),
		CFG_STR("tls_ciphers", DEFAULT_TLS_CIPHERS, CFGF_NONE),
		CFG_FLOAT("timeout", DEFAULT_TIMEOUT, CFGF_NONE),
		CFG_STR("user", NULL, CFGF_NODEFAULT),
		CFG_SEC("authorize", auth_opts,
		    CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
		CFG_END()
	};
	cfg_t *cfg = cfg_init(opts, CFGF_NONE); /* Aborts on error. */

	cfg_set_validate_func(cfg, "log_level",
	    validate_unsigned_int_cb);
	cfg_set_validate_func(cfg, "max_command_size",
	    validate_unsigned_int_cb);
	cfg_set_validate_func(cfg, "max_queue_size",
	    validate_unsigned_int_cb);
	cfg_set_validate_func(cfg, "timeout",
	    validate_unsigned_float_cb);

	debug("Parsing configuration file %s", path);

	if (stat(path, &sb) == -1)
		die("Cannot access %s: %m", path);
	if (S_ISDIR(sb.st_mode))
		check_parse_success(parse_include(cfg, path));
	else
		check_parse_success(cfg_parse(cfg, path));

	process_auth_sections(cfg);
	return cfg;
}

/*
 * Static functions.
 */

static int
parse_include(cfg_t * restrict cfg, const char * restrict path)
{
	char *include_statement;
	int status;

	xasprintf(&include_statement, "include('%s')", path);
	status = cfg_parse_buf(cfg, include_statement);
	free(include_statement);
	return status;
}

static void
check_parse_success(int status)
{
	switch (status) {
	case CFG_SUCCESS:
		break;
	case CFG_PARSE_ERROR: /* An error message has been printed already. */
		exit(EXIT_FAILURE);
	case CFG_FILE_ERROR:
		die("Cannot open configuration file for reading");
	default: /* Won't happen unless the libConfuse API changes. */
		die("Cannot parse the configuration");
	}
}

static void
process_auth_sections(cfg_t *cfg)
{
	unsigned int i, n_auth_blocks;

	if ((n_auth_blocks = cfg_size(cfg, "authorize")) == 0)
		die("No authorizations configured");

	hash_new((size_t)(n_auth_blocks /* Minimize collisions: */ * 1.5));

	for (i = 0; i < n_auth_blocks; i++) {
		cfg_t *auth = cfg_getnsec(cfg, "authorize", i);
		const char *identity = cfg_title(auth);

		debug("Processing authorizations for %s", identity);
		fallback_to_defaults(cfg, auth);
		validate_auth_section(auth);
		hash_insert(identity, auth);
	}
}

static void
validate_auth_section(cfg_t *auth)
{
	const char *identity = cfg_title(auth);

	if (cfg_size(auth, "password") == 0)
		die("No password specified for %s", identity);
}

static void
fallback_to_defaults(cfg_t * restrict cfg, cfg_t * restrict auth)
{
	const char *settings[] = {
	    "hosts",
	    "services",
	    "commands",
	    "password"
	};
	size_t i;

	for (i = 0; i < sizeof(settings) / sizeof(*settings); i++) {
		cfg_opt_t *global_opt = cfg_getopt(cfg, settings[i]);
		unsigned int j;

		if (cfg_size(auth, settings[i]) == 0
		    && cfg_opt_size(global_opt) > 0)
			for (j = 0; j < cfg_opt_size(global_opt); j++)
				if (cfg_setopt(auth,
				    cfg_getopt(auth, settings[i]),
				    cfg_opt_getnstr(global_opt, j)) == NULL)
					exit(EXIT_FAILURE);
	}
}

static char *
host_to_command(const char *host_pattern)
{
	char *command_pattern;

	xasprintf(&command_pattern, "PROCESS_HOST_CHECK_RESULT;%s;.+",
	    host_pattern);

	return command_pattern;
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
parse_host_pattern_cb(cfg_t * restrict cfg, cfg_opt_t * restrict opt,
                      const char * restrict value, void * restrict result)
{
	char *command_pattern = host_to_command(value);
	int success = parse_command_pattern_cb(cfg, opt, command_pattern,
	    result);

	free(command_pattern);
	return success;
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

static int
include_cb(cfg_t * restrict cfg, cfg_opt_t * restrict opt, int argc,
           const char ** restrict argv)
{
	struct stat sb;
	char *path;
	int status;

	if (++n_included > MAX_INCLUDE) {
		cfg_error(cfg, "Processed too many `%s' directives", opt->name);
		return 1;
	}
	if (argc != 1) {
		cfg_error(cfg, "`%s' expects one argument", opt->name);
		return 1;
	}
	if ((path = cfg_tilde_expand(argv[0])) == NULL) {
		cfg_error(cfg, "Cannot tilde-expand %s", argv[0]);
		return 1;
	}

	if (stat(path, &sb) == -1) {
		cfg_error(cfg, "Cannot access %s: %s", path, strerror(errno));
		status = 1;
	} else if (S_ISREG(sb.st_mode)) {
		status = cfg_include(cfg, opt, argc, argv);
	} else if (S_ISDIR(sb.st_mode)) {
		include_cfg = cfg;

		if (nftw(path, include_file_cb, /* Arbitrary: */ 20, 0) == -1) {
			cfg_error(cfg, "Cannot traverse %s tree: %s", path,
			    strerror(errno));
			status = 1;
		} else
			status = 0;
	} else {
		cfg_error(cfg, "%s is not a file or directory", path);
		status = 1;
	}

	free(path);
	return status;
}

static int
include_file_cb(const char *path,
                const struct stat *sb __attribute__((__unused__)), int type,
                struct FTW *info __attribute__((__unused__)))
{
	char *dot;

	if (type != FTW_F) {
		debug("Not including %s, as it's not a regular file", path);
		return 0;
	}
	if ((dot = strrchr(path, '.')) == NULL
	    || (strcmp(dot, ".cfg") != 0 && strcmp(dot, ".conf") != 0)) {
		debug("Not including %s, as it's not a .cfg/.conf file", path);
		return 0;
	}
	debug("Parsing %s", path);
	check_parse_success(parse_include(include_cfg, path));
	return 0;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
