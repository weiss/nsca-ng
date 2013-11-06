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

#include "system.h"

#define PROGRAM_NAME "test_nsca"
#define LISTEN_ADDRESS "127.0.0.1"
#define LISTEN_PORT "12345" /* Don't interfere with a poduction server. */
#define COMMAND_FILE "fifo"
#define SERVER_PID_FILE "server.pid"
#define CLIENT_CONF_FILE "client.cfg"
#define SERVER_CONF_FILE "server.cfg"
#define TIMEOUT 10

#define DEFAULT_CLIENT_CONF "# Created by " PROGRAM_NAME "\n"   \
    "password = \"forty-two\"\n"

#define DEFAULT_SERVER_CONF "# Created by " PROGRAM_NAME "\n"   \
    "authorize \"*\" {\n"                                       \
    "    password = \"forty-two\"\n"                            \
    "    commands = \".*\"\n"                                   \
    "}\n"

#define CLIENT_COMMAND_LINE "send_nsca "                        \
    "-c `pwd`/" CLIENT_CONF_FILE " "                            \
    "-H " LISTEN_ADDRESS " "                                    \
    "-p " LISTEN_PORT

#define SERVER_COMMAND_LINE "nsca-ng "                          \
    "-c `pwd`/" SERVER_CONF_FILE " "                            \
    "-C `pwd`/" COMMAND_FILE " "                                \
    "-P `pwd`/" SERVER_PID_FILE " "                             \
    "-b " LISTEN_ADDRESS ":" LISTEN_PORT " "                    \
    "-l 0"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long expected_num_lines = 1;
static volatile char got_signal = 0;

static void get_options(int, char **);
static char *join(const char *, const char *);
static void run_command(const char *);
static void write_file(const char *, const char *);
static void create_fifo(void);
static void cat_fifo(long);
static void remove_fifo(void);
static void kill_server(void);
static void print_usage(FILE *);
static void print_version(void);
static void handle_signal(int);
static void xatexit(void (*)(void));
static void xsigaction(int, const struct sigaction * restrict,
                       struct sigaction * restrict);
static void __attribute__((__format__(__printf__, 1, 2), __noreturn__))
            die(const char *, ...);

int
main(int argc, char **argv)
{
	struct sigaction sa;
#ifndef __gnu_hurd__
	int fd;
#endif

	get_options(argc, argv);

	sa.sa_flags = 0;
	sa.sa_handler = handle_signal;
	(void)sigemptyset(&sa.sa_mask);
	xsigaction(SIGINT, &sa, NULL);
	xsigaction(SIGTERM, &sa, NULL);
	xsigaction(SIGALRM, &sa, NULL);
	(void)alarm(TIMEOUT);

	if (access(CLIENT_CONF_FILE, F_OK) == -1)
		write_file(CLIENT_CONF_FILE, DEFAULT_CLIENT_CONF);
	if (access(SERVER_CONF_FILE, F_OK) == -1)
		write_file(SERVER_CONF_FILE, DEFAULT_SERVER_CONF);

	create_fifo();
	xatexit(remove_fifo);

	/*
	 * Make sure there's a FIFO reader when the server starts up, in order
	 * to avoid having to wait ten seconds until the server notices that we
	 * opened the FIFO for reading (in the cat_fifo() function).  GNU Hurd
	 * doesn't like this trick, though.
	 */
#ifndef __gnu_hurd__
	fd = open(COMMAND_FILE, O_RDONLY | O_NONBLOCK);
#endif
	run_command(join(SERVER_COMMAND_LINE, getenv("NSCA_SERVER_FLAGS")));
	xatexit(kill_server);

	run_command(join(CLIENT_COMMAND_LINE, getenv("NSCA_CLIENT_FLAGS")));
	cat_fifo(expected_num_lines);
#ifndef __gnu_hurd__
	(void)close(fd);
#endif
	return EXIT_SUCCESS;
}

static void
get_options(int argc, char **argv)
{
	extern int optind;
	extern char *optarg;
	int option;

	if (argc == 2) {
		if (strcmp(argv[1], "--help") == 0) {
			print_usage(stdout);
			exit(EXIT_SUCCESS);
		}
		if (strcmp(argv[1], "--version") == 0) {
			print_version();
			exit(EXIT_SUCCESS);
		}
	}

	while ((option = getopt(argc, argv, "hl:V")) != -1)
		switch (option) {
		case 'h':
			print_usage(stdout);
			exit(EXIT_SUCCESS);
		case 'l':
			if ((expected_num_lines = atol(optarg)) < 1)
				die("-l must be a number greater than zero");
			break;
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			print_usage(stderr);
			exit(EXIT_FAILURE);
		}

	if (argc - optind > 0)
		die("Unexpected non-option argument: %s", argv[optind]);
}

static char *
join(const char *part1, const char *part2)
{
	static char joined[4096];
	size_t len1 = strlen(part1);

	if (len1 > sizeof(joined))
		die("Command line too long");

	(void)strcpy(joined, part1);

	if (part2 != NULL) {
		size_t len2 = strlen(part2);

		if (len1 + 1 + len2 + 1 > sizeof(joined))
			die("Command line too long");

		(void)strcat(joined, " ");
		(void)strcat(joined, part2);
	}
	return joined;
}

static void
run_command(const char *command)
{
	int status;

	if ((status = system(command)) == -1)
		die("Cannot execute %s: %s", command, strerror(errno));
	if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
		exit(77); /* Tell Autotest to skip this test. */
	if (status != 0)
		exit(1);
}

static void
write_file(const char *file, const char *contents)
{
	FILE *f;

	if ((f = fopen(file, "w")) == NULL)
		die("Cannot open %s: %s", file, strerror(errno));
	if (fputs(contents, f) == EOF)
		die("Cannot write %s: %s", file, strerror(errno));
	if (fclose(f) == EOF)
		die("Cannot close %s: %s", file, strerror(errno));
}

static void
create_fifo(void)
{
	if (mkfifo(COMMAND_FILE, 0666) == -1)
		(void)fprintf(stderr, "%s: Cannot create %s: %s\n",
		    PROGRAM_NAME, COMMAND_FILE, strerror(errno));
}

static void
cat_fifo(long n_lines)
{
	FILE *fifo;
	int c;
	enum {
		STATE_EAT_TIMESTAMP,
		STATE_PRINT_COMMAND
	} state = STATE_EAT_TIMESTAMP;

	do
		fifo = fopen(COMMAND_FILE, "r");
	while (fifo == NULL && errno == EINTR); /* Happens on GNU Hurd. */
	if (fifo == NULL)
		die("Cannot open %s: %s", COMMAND_FILE, strerror(errno));
	while ((c = getc(fifo)) != EOF) {
		if (state == STATE_EAT_TIMESTAMP) {
			if (c == ' ')
				state = STATE_PRINT_COMMAND;
			else if (c != '[' && !isdigit(c) && c != ']')
				die("Got unexpected `%c' from FIFO", c);
		} else {
			if (putchar(c) == EOF)
				die("Cannot write to stdout: %s",
				    strerror(errno));
			if (c == '\n') {
				if (--n_lines > 0)
					state = STATE_EAT_TIMESTAMP;
				else
					break;
			}
		}
	}
	if (ferror(fifo))
		die("Cannot read %s: %s", COMMAND_FILE,
		    got_signal ? "Interrupted" : strerror(errno));
}

static void
remove_fifo(void)
{
	if (unlink(COMMAND_FILE) == -1)
		(void)fprintf(stderr, "%s: Cannot remove %s: %s\n",
		    PROGRAM_NAME, COMMAND_FILE, strerror(errno));
}

static void
kill_server(void)
{
	FILE *f;
	char buf[64];
	pid_t pid;

	/*
	 * To minimize the risk that the server process interferes with any
	 * following test(s), we KILL the process instead of using the TERM
	 * signal.
	 */
	if ((f = fopen(SERVER_PID_FILE, "r")) == NULL)
		(void)fprintf(stderr, "%s: Cannot open %s: %s\n", PROGRAM_NAME,
		    SERVER_PID_FILE, strerror(errno));
	else if (fgets(buf, sizeof(buf), f) == NULL)
		(void)fprintf(stderr, "%s: Cannot read %s: %s\n", PROGRAM_NAME,
		    SERVER_PID_FILE, ferror(f) ? strerror(errno) : "EOF");
	else if ((pid = (pid_t)atol(buf)) < 1)
		(void)fprintf(stderr, "%s: PID file %s contains garbage\n",
		    PROGRAM_NAME, SERVER_PID_FILE);
	else if (kill(pid, SIGKILL) == -1)
		(void)fprintf(stderr, "%s: Cannot kill server PID %lu: %s\n",
		    PROGRAM_NAME, (unsigned long)pid, strerror(errno));
	else if (fclose(f) == EOF)
		(void)fprintf(stderr, "%s: Cannot close %s: %s\n", PROGRAM_NAME,
		    SERVER_PID_FILE, strerror(errno));
}

static void
print_usage(FILE *stream)
{
	(void)fprintf(stream,
	    "Usage: %s [<options>]\n\n"
	    "Options:\n"
	    " -h          Print this usage information and exit.\n"
	    " -l <lines>  Read this number of lines from FIFO (default: 1).\n"
	    " -V          Print version information and exit.\n",
	    PROGRAM_NAME);
}

static void
print_version(void)
{
	(void)system("send_nsca -V");
	(void)system("nsca-ng -V");
#if HAVE_POSIX_AIO
	(void)puts("The NSCA-ng server uses the POSIX AIO API on this system.");
#endif
}

static void
handle_signal(int signal_number __attribute__((__unused__)))
{
	got_signal = 1;
}

static void
xatexit(void function(void))
{
	if (atexit(function) != 0)
		die("Cannot register exit function");
}

static void
xsigaction(int signal_number, const struct sigaction * restrict action,
           struct sigaction * restrict old_action)
{
	if (sigaction(signal_number, action, old_action) == -1)
		die("Cannot set handler for signal %d: %s", signal_number,
		    strerror(errno));
}

static void
die(const char *format, ...)
{
	va_list ap;

	(void)fputs(PROGRAM_NAME ": ", stderr);

	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);

	(void)putc('\n', stderr);

	exit(EXIT_FAILURE);
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
