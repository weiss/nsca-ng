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

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_BSD_LIBUTIL_H
# include <bsd/libutil.h>
#else
# include <libutil.h> /* Either in /usr/include or in ../../lib/pidfile. */
#endif
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <confuse.h>
#include <ev.h>

#include "conf.h"
#include "log.h"
#include "server.h"
#include "system.h"
#include "util.h"
#include "wrappers.h"

typedef struct {
	char *bind;
	char *conf_file;
	char *command_file;
	char *pid_file;
	int log_level;
	int log_target;
	bool foreground;
} options;

static cfg_t *cfg = NULL;
static struct pidfh *pfh = NULL;
static char *pid_file = NULL;
static bool restart = false;
static ev_signal sighup_watcher, sigint_watcher, sigterm_watcher;

static options *get_options(int, char **);
static void free_options(options *);
static void drop_privileges(const char *, const char *);
static void remove_pidfile(void);
static void forget_config(void);
static void signal_cb(EV_P_ ev_signal *, int __attribute__((__unused__)));
static void usage(int) __attribute__((__noreturn__));

int
main(int argc, char **argv)
{
	server_state *server;
	options *opt;
	pid_t other_pid;

	setprogname(argv[0]);

	log_set(LOG_LEVEL_INFO, LOG_TARGET_STDERR);

	if (atexit(log_close) != 0 || atexit(forget_config) != 0)
		die("Cannot register function to be called on exit");

	if (!ev_default_loop(0))
		die("Cannot initialize libev");

	opt = get_options(argc, argv);
	cfg = conf_parse(opt->conf_file != NULL ?
	    opt->conf_file : DEFAULT_CONF_FILE);

	if (cfg_size(cfg, "user") > 0 || cfg_size(cfg, "chroot") > 0)
		drop_privileges(cfg_size(cfg, "user") > 0 ?
		    cfg_getstr(cfg, "user") : NULL,
		    cfg_size(cfg, "chroot") > 0 ?
		    cfg_getstr(cfg, "chroot") : NULL);

	if (opt->log_target == -1)
		opt->log_target = opt->foreground ?
		    LOG_TARGET_STDERR : LOG_TARGET_SYSLOG;
	else if (!opt->foreground && opt->log_target & LOG_TARGET_STDERR)
		die("The `-S' option may not be specified without `-F'");

	if (opt->log_level != -1)
		cfg_setint(cfg, "log_level", opt->log_level);
	if (opt->command_file != NULL)
		cfg_setstr(cfg, "command_file", opt->command_file);
	if (opt->pid_file != NULL)
		cfg_setstr(cfg, "pid_file", opt->pid_file);
	if (opt->bind != NULL)
		cfg_setstr(cfg, "listen", opt->bind);

	if (access(cfg_getstr(cfg, "temp_directory"), X_OK | W_OK) == -1)
		die("Cannot write temporary files into %s: %m",
		    cfg_getstr(cfg, "temp_directory"));

	pid_file = cfg_size(cfg, "pid_file") > 0 ?
	    cfg_getstr(cfg, "pid_file") : NULL;

	log_set((int)cfg_getint(cfg, "log_level"),
	    opt->log_target | LOG_TARGET_STDERR);

	if (strchr(cfg_getstr(cfg, "listen"), ':') == NULL) {
		char *host_port;

		xasprintf(&host_port, "%s:%s", cfg_getstr(cfg, "listen"),
		    DEFAULT_PORT);
		cfg_setstr(cfg, "listen", host_port);
		free(host_port);
	}

	if (pid_file != NULL) {
		if ((pfh = pidfile_open(pid_file, 0600, &other_pid)) == NULL) {
			if (errno == EEXIST)
				die("%s seems to be running already (PID: %jd)",
				    PACKAGE_NAME, (intmax_t)other_pid);
			else
				die("Cannot create %s: %m", pid_file);
		}
		if (atexit(remove_pidfile) != 0)
			die("Cannot register function to be called on exit");
	}

	server = server_start(
	    cfg_getstr(cfg, "listen"),
	    cfg_getstr(cfg, "tls_ciphers"),
	    cfg_getstr(cfg, "command_file"),
	    cfg_getstr(cfg, "temp_directory"),
	    (size_t)cfg_getint(cfg, "max_command_size"),
	    (size_t)cfg_getint(cfg, "max_queue_size"),
	    cfg_getfloat(cfg, "timeout"));

	if (!opt->foreground) {
		if (daemon(0, 0) == -1)
			die("Cannot daemonize: %m");
		ev_loop_fork(EV_DEFAULT_UC);
	}

	log_set((int)cfg_getint(cfg, "log_level"), opt->log_target);

	if (pid_file != NULL && pidfile_write(pfh) == -1)
		die("Cannot write PID to %s: %m", pid_file);

	notice("%s starting up", nsca_version());

	ev_signal_init(&sighup_watcher, signal_cb, SIGHUP);
	ev_signal_init(&sigint_watcher, signal_cb, SIGINT);
	ev_signal_init(&sigterm_watcher, signal_cb, SIGTERM);
	ev_signal_start(EV_DEFAULT_UC_ &sighup_watcher);
	ev_signal_start(EV_DEFAULT_UC_ &sigint_watcher);
	ev_signal_start(EV_DEFAULT_UC_ &sigterm_watcher);

	ev_run(EV_DEFAULT_UC_ 0);

	server_stop(server);
	free_options(opt);
	if (restart) {
		notice("Restarting");
		/* The atexit(3) functions won't be called on exec(3). */
		remove_pidfile();
		forget_config();
		execvp(argv[0], argv);
		die("Cannot restart myself: %m");
	}
	notice("Exiting");
	return EXIT_SUCCESS;
}

static options *
get_options(int argc, char **argv)
{
	extern int optind;
	extern char *optarg;
	options *opt = xmalloc(sizeof(options));
	int option;

	opt->bind = NULL;
	opt->conf_file = NULL;
	opt->command_file = NULL;
	opt->pid_file = NULL;
	opt->log_level = -1;
	opt->log_target = -1;
	opt->foreground = false;

	if (argc == 2) {
		if (strcmp(argv[1], "--help") == 0)
			usage(EXIT_SUCCESS);
		if (strcmp(argv[1], "--version") == 0) {
			(void)puts(nsca_version());
			exit(EXIT_SUCCESS);
		}
	}

	while ((option = getopt(argc, argv, "b:C:c:Fhl:P:SsV")) != -1)
		switch (option) {
		case 'b':
			if (opt->bind != NULL)
				free(opt->bind);
			opt->bind = xstrdup(optarg);
			break;
		case 'C':
			if (opt->command_file != NULL)
				free(opt->command_file);
			opt->command_file = xstrdup(optarg);
			break;
		case 'c':
			if (opt->conf_file != NULL)
				free(opt->conf_file);
			opt->conf_file = xstrdup(optarg);
			break;
		case 'F':
			opt->foreground = true;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'l':
			opt->log_level = atoi(optarg);
			break;
		case 'P':
			if (opt->pid_file != NULL)
				free(opt->pid_file);
			opt->pid_file = xstrdup(optarg);
			break;
		case 'S':
			opt->log_target = LOG_TARGET_SYSLOG | LOG_TARGET_STDERR;
			break;
		case 's':
			opt->log_target = opt->log_target != -1
			    ? LOG_TARGET_SYSLOG | opt->log_target
			    : LOG_TARGET_SYSLOG;
			break;
		case 'V':
			(void)puts(nsca_version());
			exit(EXIT_SUCCESS);
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
	if (opt->bind != NULL)
		free(opt->bind);
	if (opt->conf_file != NULL)
		free(opt->conf_file);
	if (opt->command_file != NULL)
		free(opt->command_file);
	if (opt->pid_file != NULL)
		free(opt->pid_file);

	free(opt);
}

static void
drop_privileges(const char *user, const char *new_root)
{
	struct passwd *pw;

	errno = 0;
	if (user != NULL) {
		if ((pw = getpwnam(user)) == NULL) {
			if (errno == 0)
				die("Cannot find user %s", user);
			else
				die("Cannot lookup user %s: %m", user);
		}
		if (initgroups(user, pw->pw_gid) == -1)
			die("Cannot set up group list for user %s: %m", user);
	}
	if (new_root != NULL && (chroot(new_root) == -1 || chdir("/") == -1))
		die("Cannot change root directory to %s: %m", new_root);
	if (user != NULL
	    && (setgid(pw->pw_gid) == -1 || setuid(pw->pw_uid) == -1))
		die("Cannot switch to user %s: %m", user);
}

static void
remove_pidfile(void)
{
	if (pid_file != NULL && pidfile_remove(pfh) == -1) {
		error("Cannot remove %s: %m", pid_file);
		pid_file = NULL; /* Don't retry removing the PID file. */
	}
}

static void
forget_config(void)
{
	if (cfg != NULL) {
		unsigned int i, n_auth_blocks = cfg_size(cfg, "authorize");

		for (i = 0; i < n_auth_blocks; i++) {
			cfg_t *auth = cfg_getnsec(cfg, "authorize", i);

			if (cfg_size(auth, "password") > 0) {
				char *password = cfg_getstr(auth, "password");

				(void)memset(password, 0, strlen(password));
			}
		}
		cfg_free(cfg);
	}
}

static void
signal_cb(EV_P_ ev_signal *w, int revents __attribute__((__unused__)))
{
	const char *sig_string;

	ev_signal_stop(EV_A_ &sighup_watcher);
	ev_signal_stop(EV_A_ &sigint_watcher);
	ev_signal_stop(EV_A_ &sigterm_watcher);

	switch (w->signum) {
	case SIGHUP:
		sig_string = "SIGHUP";
		break;
	case SIGINT:
		sig_string = "SIGINT";
		break;
	case SIGTERM:
		sig_string = "SIGTERM";
		break;
	default:
		sig_string = "unknown signal"; /* Huh? */
	}
	notice("Received %s, shutting down connections", sig_string);

	ev_break(EV_A_ EVBREAK_ALL);

	if (w->signum == SIGHUP)
		restart = true;
}

static void
usage(int status)
{
	(void)fprintf(status == EXIT_SUCCESS ? stdout : stderr,
	    "Usage: %s [<options>]\n\n"
	    "Options:\n"
	    " -b <host[:port]> Bind to <host[:port]>.\n"
	    " -C <fifo>        Submit commands into the specified <fifo>.\n"
	    " -c <file>        Use the specified configuration <file>.\n"
	    " -F               Don't detach from the controlling terminal.\n"
	    " -h               Print this usage information and exit.\n"
	    " -l <level>       Set the specified log <level>.\n"
	    " -P <file>        Write the PID into the specified <file>.\n"
	    " -S               Write messages to syslog and standard error.\n"
	    " -s               Write messages to syslog.\n"
	    " -V               Print version information and exit.\n",
	    getprogname());

	exit(status);
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
