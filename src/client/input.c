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

#include <errno.h>
#include <unistd.h> /* Required by <fcntl.h> on some systems. */
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#include "input.h"
#include "log.h"
#include "system.h"
#include "util.h"
#include "wrappers.h"

static void read_cb(EV_P_ ev_io *, int);

/*
 * Exported functions.
 */

input_state *
input_start(char separator)
{
	int flags;

	input_state *input = xmalloc(sizeof(input_state));

	debug("Starting standard input reader");

	input->watcher.data = input;
	input->buffer = buffer_new();
	input->read_handler = NULL;
	input->eof_handler = NULL;
	input->fd = STDIN_FILENO;
	input->separator = separator;
	input->eof = false;

	if ((flags = fcntl(input->fd, F_GETFL, 0)) == -1)
		die("Cannot get standard input descriptor flags: %m");
	if (fcntl(input->fd, F_SETFL, flags | O_NONBLOCK) == -1)
		die("Cannot mark standard input as non-blocking: %m");

	ev_io_init(&input->watcher, read_cb, input->fd, EV_READ);

	return input;
}

void
input_read_chunk(input_state *input, void handle_read(input_state *, char *))
{
	debug("Got request to read a chunk from standard input");

	if (!input->eof) {
		input->read_handler = handle_read;
		ev_io_start(EV_DEFAULT_UC_ &input->watcher);
		ev_invoke(EV_DEFAULT_UC_ &input->watcher, EV_CUSTOM);
	} else {
		debug("There's no (more) data available");
		if (input->eof_handler != 0)
			input->eof_handler(input);
		input_stop(input);
	}
}

void
input_stop(input_state *input)
{
	debug("Stopping standard input reader");

	if (ev_is_active(&input->watcher))
		ev_io_stop(EV_DEFAULT_UC_ &input->watcher);

	buffer_free(input->buffer);
	free(input);
}

void
input_on_eof(input_state *input, void handle_eof(input_state *))
{
	input->eof_handler = handle_eof;
}

/*
 * Static functions.
 */

static void
read_cb(EV_P_ ev_io *w, int revents __attribute__((__unused__)))
{
	input_state *input = w->data;
	char *chunk;
	ssize_t n;

	do {
		if ((chunk = buffer_read_chunk(input->buffer, input->separator))
		    != NULL) {
			debug("Got complete chunk from standard input");
			ev_io_stop(EV_A_ w);
			input->read_handler(input, chunk);
			break;
		}
		if ((n = read(input->fd, input->input, sizeof(input->input)))
		    < 0) {
			if (errno == EAGAIN
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
			    || errno == EWOULDBLOCK
#endif
			    || errno == EINTR)
				debug("Cannot read from standard input: %m");
			else
				die("Cannot read from standard input: %m");
		} else if (n == 0) {
			size_t len = buffer_size(input->buffer);

			debug("Got EOF from standard input");
			input->eof = true;
			if (len > 0) {
				debug("Providing the available data");
				ev_io_stop(EV_A_ w);
				chunk = xmalloc(len + 1);
				(void)buffer_read(input->buffer, chunk, len);
				chunk[len] = '\0';
				input->read_handler(input, chunk);
			} else {
				debug("No (more) data available");
				if (input->eof_handler != 0)
					input->eof_handler(input);
				input_stop(input);
			}
		} else {
			debug("Got %zd bytes from standard input", n);
			buffer_append(input->buffer, input->input, (size_t)n);
		}
	} while (n > 0);
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
