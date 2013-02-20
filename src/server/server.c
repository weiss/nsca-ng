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

#if HAVE_AIO_INIT
# include <aio.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ev.h>

#include "auth.h"
#include "fifo.h"
#include "log.h"
#include "server.h"
#include "system.h"
#include "tls.h"
#include "util.h"
#include "wrappers.h"

struct server_state_s { /* This is typedef'd to `server_state' in server.h. */
	tls_server_state *tls_server;
	fifo_state *fifo;
	size_t max_command_size;
};

typedef struct {
	server_state *ctx;
	size_t input_length;
} connection_state;

static void handle_connect(tls_state *);
static void handle_handshake(tls_state * restrict, char * restrict);
static void handle_connection(tls_state * restrict, char * restrict);
static void handle_push(tls_state * restrict, char * restrict);
static void handle_error(tls_state *);
static void handle_timeout(tls_state *);
static void handle_line_too_long(tls_state *);
static void send_response(tls_state * restrict, const char * restrict);
static void bail(tls_state * restrict, const char * restrict, ...)
                 __attribute__((__format__(__printf__, 2, 3)));
static bool client_exited(tls_state * restrict, const char * restrict);
static void connection_stop(tls_state *);

/*
 * Exported functions.
 */

server_state *
server_start(const char * restrict listen,
             const char * restrict ciphers,
             const char * restrict command_file,
             const char * restrict temp_directory,
             size_t max_command_size,
             size_t max_queue_size,
             ev_tstamp timeout)
{
	server_state *ctx = xmalloc(sizeof(server_state));
#if HAVE_AIO_INIT
	struct aioinit ai;

	(void)memset(&ai, 0, sizeof(ai));
# if HAVE_STRUCT_AIOINIT_AIO_THREADS
	ai.aio_threads = 2;
# endif
# if HAVE_STRUCT_AIOINIT_AIO_NUM
	ai.aio_num = 2;
# endif
# if HAVE_STRUCT_AIOINIT_AIO_LOCKS
	ai.aio_locks = 0;
# endif
# if HAVE_STRUCT_AIOINIT_AIO_USEDBA
	ai.aio_usedba = 0;
# endif
# if HAVE_STRUCT_AIOINIT_AIO_DEBUG
	ai.aio_debug = 0;
# endif
# if HAVE_STRUCT_AIOINIT_AIO_NUMUSERS
	ai.aio_numusers = 0;
# endif
# if HAVE_STRUCT_AIOINIT_AIO_IDLE_TIME
	ai.aio_idle_time = 1;
# endif
	aio_init(&ai);
#endif
	ctx->max_command_size = max_command_size;
	ctx->fifo = fifo_start(command_file, temp_directory, max_queue_size);
	ctx->tls_server = tls_server_start(listen, ciphers, timeout,
	    handle_connect, check_psk);
	ctx->tls_server->data = ctx;

	return ctx;
}

void
server_stop(server_state *ctx)
{
	tls_server_stop(ctx->tls_server);
	fifo_stop(ctx->fifo);
	free(ctx);
}

/*
 * Static functions.
 */

static void
handle_connect(tls_state *tls)
{
	connection_state *connection = xmalloc(sizeof(connection_state));

	connection->ctx = tls->data;
	tls->data = connection;

	tls_on_timeout(tls, handle_timeout);
	tls_on_error(tls, handle_error);
	tls_on_line_too_long(tls, handle_line_too_long);

	tls_read_line(tls, handle_handshake);
}

static void
handle_handshake(tls_state * restrict tls, char * restrict line)
{
	connection_state *connection = tls->data;
	char *args[3];

	info("%s C: %s", tls->peer, line);

	if (strncasecmp("MOIN", line, 4) == 0) {
		if (!parse_line(line, args, 3)) {
			warning("Cannot parse MOIN request from %s", tls->peer);
			send_response(tls, "FAIL Cannot parse MOIN request");
			tls_read_line(tls, handle_handshake);
		} else if (atoi(args[1]) <= 0) {
			warning("Expected protocol version from %s", tls->peer);
			send_response(tls, "FAIL Expected protocol version");
			tls_read_line(tls, handle_handshake);
		} else {
			debug("MOIN handshake successful");
			tls_set_connection_id(tls, args[2]);
			send_response(tls, "MOIN 1");
			tls_read_line(tls, handle_connection);
		}
	} else if (strncasecmp("PING", line, 4) == 0) {
		send_response(tls, "PONG 1");
		tls_shutdown(tls);
		connection_stop(tls);
	} else if (!client_exited(tls, line)) {
		warning("Expected MOIN or PING or BAIL from %s", tls->peer);
		send_response(tls, "FAIL Expected MOIN or PING or BAIL");
		tls_read_line(tls, handle_handshake);
	}
	free(line);
}

static void
handle_connection(tls_state * restrict tls, char * restrict line)
{
	connection_state *connection = tls->data;
	char *args[2];
	int data_size;

	info("%s C: %s", tls->peer, line);

	if (strncasecmp("NOOP", line, 4) == 0)
		send_response(tls, "OKAY");
	else if (strncasecmp("PUSH", line, 4) == 0) {
		if (!parse_line(line, args, 2)) {
			warning("Cannot parse PUSH request from %s", tls->peer);
			send_response(tls, "FAIL Cannot parse PUSH request");
			tls_read_line(tls, handle_connection);
		} else if ((data_size = atoi(args[1])) <= 0) {
			warning("Expected number of bytes from %s", tls->peer);
			send_response(tls, "FAIL Expected number of bytes");
			tls_read_line(tls, handle_connection);
		} else if (connection->ctx->max_command_size > 0
		    && (size_t)data_size > connection->ctx->max_command_size) {
			warning("Command from %s too long", tls->peer);
			send_response(tls, "FAIL PUSH data size too large");
			tls_read_line(tls, handle_connection);
		} else {
			send_response(tls, "OKAY");
			connection->input_length = (size_t)data_size;
			tls_read(tls, handle_push, connection->input_length);
		}
	} else if (!client_exited(tls, line)) {
		warning("Expected PUSH or NOOP or QUIT from %s", tls->peer);
		send_response(tls, "FAIL Expected PUSH or NOOP or QUIT");
		tls_read_line(tls, handle_connection);
	}
	free(line);
}

static void
handle_push(tls_state * restrict tls, char * restrict data)
{
	connection_state *connection = tls->data;
	int width = connection->input_length > 0
	    && data[connection->input_length - 1] == '\n'
	    ? (int)connection->input_length - 1 : (int)connection->input_length;

	info("%s C: %.*s", tls->peer, width, data);

	if (is_authorized(tls->id, data)) {
		notice("Queuing data from %s: %.*s", tls->peer, width, data);
		fifo_write(connection->ctx->fifo, data,
		    connection->input_length, free);
		send_response(tls, "OKAY");
	} else {
		warning("Refusing data from %s: %.*s", tls->peer, width, data);
		free(data);
		send_response(tls, "FAIL You're not authorized");
	}

	tls_read_line(tls, handle_connection);
}

static void
handle_error(tls_state *tls)
{
	connection_stop(tls);
}

static void
handle_timeout(tls_state *tls)
{
	bail(tls, "Connection timed out");
}

static void
handle_line_too_long(tls_state *tls)
{
	bail(tls, "Request line too long");
}

static void
send_response(tls_state * restrict tls, const char * restrict response)
{
	info("%s S: %s", tls->peer, response);
	tls_write_line(tls, response);
}

static void
bail(tls_state * restrict tls, const char * restrict fmt, ...)
{
	va_list ap;
	char *message;

	va_start(ap, fmt);
	xvasprintf(&message, fmt, ap);
	va_end(ap);

	info("%s S: %s %s", tls->peer, "BAIL", message);

	tls_write(tls, "BAIL ", sizeof("BAIL ") - 1, NULL);
	tls_write_line(tls, message);
	tls_shutdown(tls);

	connection_stop(tls);
	free(message);
}

static bool
client_exited(tls_state * restrict tls, const char * restrict line)
{
	if (strncasecmp("QUIT", line, 4) == 0) {
		send_response(tls, "OKAY");
		tls_shutdown(tls);
		connection_stop(tls);
		return true;
	} else if (strncasecmp("BAIL", line, 4) == 0) {
		error("%s said: %s", tls->peer, line);
		tls_shutdown(tls);
		connection_stop(tls);
		return true;
	}
	return false;
}

static void
connection_stop(tls_state *tls)
{
	connection_state *connection = tls->data;

	free(connection);
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
