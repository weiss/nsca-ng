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

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_NANOSLEEP
# include <time.h>
#endif
#include <unistd.h>

#include <ev.h>
#include <openssl/rand.h>

#include "client.h"
#include "conf.h"
#include "log.h"
#include "send_nsca.h"
#include "system.h"
#include "util.h"
#include "wrappers.h"

typedef struct {
	char *conf_file;
	char *port;
	char *server;
	int delay;
	int log_level;
	int log_target;
	int timeout;
	char delimiter;
	bool raw_commands;
} options;

conf *cfg = NULL;
int exit_code = EXIT_SUCCESS;

static options *get_options(int, char **);
static void free_options(options *);
static void delay_execution(unsigned int);
static unsigned long random_number(unsigned long);
static void forget_config(void);
static void usage(int) __attribute__((__noreturn__));

int
main(int argc, char **argv)
{
	options *opt;
	char *host_port;

	setprogname(argv[0]);

	log_set(LOG_LEVEL_WARNING, LOG_TARGET_STDERR);

	if (atexit(log_close) != 0 || atexit(forget_config))
		die("Cannot register function to be called on exit");

	if (!ev_default_loop(0))
		die("Cannot initialize libev");

	opt = get_options(argc, argv);
	cfg = conf_init(opt->conf_file != NULL ?
	    opt->conf_file : DEFAULT_CONF_FILE);

	if (opt->port != NULL)
		conf_setstr(cfg, "port", opt->port);
	if (opt->server != NULL)
		conf_setstr(cfg, "server", opt->server);
	if (opt->delay != -1)
		conf_setint(cfg, "delay", opt->delay);
	if (opt->timeout != -1)
		conf_setint(cfg, "timeout", opt->timeout);

	log_set(opt->log_level, opt->log_target);

	debug("%s starting up", nsca_version());

	if (conf_getint(cfg, "delay") != 0)
		delay_execution((unsigned int)conf_getint(cfg, "delay"));

	(void)xasprintf(&host_port, "%s:%s", conf_getstr(cfg, "server"),
	    conf_getstr(cfg, "port"));

	(void)client_start(host_port,
	    conf_getstr(cfg, "tls_ciphers"),
	    conf_getint(cfg, "timeout"),
	    opt->raw_commands ? CLIENT_MODE_COMMAND : CLIENT_MODE_CHECK_RESULT,
	    opt->delimiter);

	ev_run(EV_DEFAULT_UC_ 0);

	free(host_port);
	free_options(opt);
	return exit_code;
}

static options *
get_options(int argc, char **argv)
{
	extern int optind;
	extern char *optarg;
	options *opt = xmalloc(sizeof(options));
	int option;

	opt->conf_file = NULL;
	opt->port = NULL;
	opt->server = NULL;
	opt->delay = -1;
	opt->log_level = -1;
	opt->log_target = -1;
	opt->timeout = -1;
	opt->delimiter = '\t';
	opt->raw_commands = false;

	if (argc == 2) {
		if (strcmp(argv[1], "--help") == 0)
			usage(EXIT_SUCCESS);
		if (strcmp(argv[1], "--version") == 0) {
			(void)puts(nsca_version());
			exit(EXIT_SUCCESS);
		}
	}

	while ((option = getopt(argc, argv, "Cc:D:d:H:ho:p:SstVv")) != -1)
		switch (option) {
		case 'C':
			opt->raw_commands = true;
			break;
		case 'c':
			if (opt->conf_file != NULL)
				free(opt->conf_file);
			opt->conf_file = xstrdup(optarg);
			break;
		case 'D':
			opt->delay = atoi(optarg);
			if (opt->delay < 0)
				die("-D argument must be a positive integer");
			break;
		case 'd':
			if (strlen(optarg) != 1)
				die("-d argument must be a single character");
			if (*optarg == '\27'
			    || *optarg == '\n'
			    || *optarg == '\\')
				die("Illegal delimiter specified with -d");
			opt->delimiter = *optarg;
			break;
		case 'H':
			if (opt->server != NULL)
				free(opt->server);
			opt->server = xstrdup(optarg);
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'o':
			opt->timeout = atoi(optarg);
			if (opt->timeout < 0)
				die("-o argument must be a positive integer");
			break;
		case 'p':
			if (opt->port != NULL)
				free(opt->port);
			opt->port = xstrdup(optarg);
			break;
		case 'S':
			opt->log_target = opt->log_target != -1
			    ? LOG_TARGET_STDERR | opt->log_target
			    : LOG_TARGET_STDERR;
			break;
		case 's':
			opt->log_target = opt->log_target != -1
			    ? LOG_TARGET_SYSLOG | opt->log_target
			    : LOG_TARGET_SYSLOG;
			break;
		case 't':
			notice("Ignoring -t option for backward compatibility");
			break;
		case 'V':
			(void)puts(nsca_version());
			exit(EXIT_SUCCESS);
		case 'v':
			if (opt->log_level == -1)
				opt->log_level = LOG_LEVEL_NOTICE;
			else if (opt->log_level < LOG_LEVEL_DEBUG)
				opt->log_level++;
			break;
		default:
			usage(EXIT_FAILURE);
		}

	if (argc - optind > 0)
		die("Unexpected non-option argument: %s", argv[optind]);

	return opt;
}

static void
free_options(options *opt)
{
	if (opt->conf_file != NULL)
		free(opt->conf_file);
	if (opt->port != NULL)
		free(opt->port);
	if (opt->server != NULL)
		free(opt->server);

	free(opt);
}

static void
delay_execution(unsigned int max_delay)
{
#if HAVE_NANOSLEEP
	struct timespec delay;

	delay.tv_sec = (time_t)random_number(max_delay);
	delay.tv_nsec = (long)random_number(1000000000U);
	debug("Sleeping %ld seconds and %ld nanoseconds",
	    (long)delay.tv_sec, delay.tv_nsec);
	(void)nanosleep(&delay, NULL);
#else
	unsigned int delay = random_number(max_delay + 1);

	debug("Sleeping %u seconds", delay);
	(void)sleep(delay);
#endif
}

static unsigned long
random_number(unsigned long range)
{
	unsigned long random_value;

	(void)RAND_pseudo_bytes((unsigned char *)&random_value,
	    sizeof(random_value));

	/* See http://c-faq.com/lib/randrange.html for some caveats. */
	return random_value % range;
}

static void
forget_config(void)
{
	if (cfg != NULL) {
		char *password = conf_getstr(cfg, "password");

		if (password != NULL)
			(void)memset(password, 0, strlen(password));
		conf_free(cfg);
	}
}

static void
usage(int status)
{
	(void)fprintf(status == EXIT_SUCCESS ? stdout : stderr,
	    "Usage: %s [<options>]\n\n"
	    "Options:\n"
	    " -C               Accept `raw' monitoring commands.\n"
	    " -c <file>        Use the specified configuration <file>.\n"
	    " -D <delay>       Sleep up to <delay> seconds on startup.\n"
	    " -d <delimiter>   Expect <delimiter> to separate input fields.\n"
	    " -H <server>      Connect and talk to the specified <server>.\n"
	    " -h               Print this usage information and exit.\n"
	    " -o <timeout>     Use the specified connection <timeout>.\n"
	    " -p <port>        Connect to the specified <port> on the server.\n"
	    " -S               Write messages to the standard error output.\n"
	    " -s               Write messages to syslog.\n"
	    " -t               Ignore this option for backward compatibility.\n"
	    " -V               Print version information and exit.\n"
	    " -v [-v [-v]]     Increase the verbosity level.\n",
	    getprogname());

	exit(status);
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
