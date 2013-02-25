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
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ev.h>
#include <openssl/rand.h>

#include "auth.h"
#include "client.h"
#include "input.h"
#include "log.h"
#include "parse.h"
#include "send_nsca.h"
#include "system.h"
#include "tls.h"
#include "util.h"
#include "wrappers.h"

#ifndef NUM_SESSION_ID_BYTES
# define NUM_SESSION_ID_BYTES 6
#endif

struct client_state_s { /* This is typedef'd to `client_state' in client.h. */
	tls_client_state *tls_client;
	tls_state *tls;
	input_state *input;
	char *command;
	size_t command_length;
	int mode;
	char delimiter;
};

static void handle_input_chunk(input_state * restrict, char * restrict);
static void handle_input_eof(input_state *);
static void handle_tls_connect(tls_state *);
static void handle_tls_moin_response(tls_state * restrict, char * restrict);
static void handle_tls_push_response(tls_state * restrict, char * restrict);
static void handle_tls_command_response(tls_state * restrict, char * restrict);
static void handle_tls_quit_response(tls_state * restrict, char * restrict);
static void send_request(tls_state * restrict, const char * restrict);
static void bail(tls_state * restrict, const char * restrict, ...)
                 __attribute__((__format__(__printf__, 2, 3)));
static bool server_is_grumpy(tls_state * restrict, char * restrict);
static char *generate_session_id(void);
static char *base64(const unsigned char *, size_t);

/*
 * Exported functions.
 */

client_state *
client_start(const char *server, const char *ciphers, ev_tstamp timeout,
             int mode, char delimiter)
{
	client_state *client = xmalloc(sizeof(client_state));

	client->tls_client = tls_client_start(ciphers);
	client->tls_client->data = client;
	client->tls = NULL;
	client->input = NULL;
	client->mode = mode;
	client->delimiter = delimiter;

	tls_connect(client->tls_client, server, timeout, TLS_AUTO_DIE,
	    handle_tls_connect, NULL, set_psk);

	return client;
}

void
client_stop(client_state *client)
{
	if (client->input != NULL)
		input_stop(client->input);
	if (client->tls != NULL)
		tls_shutdown(client->tls);
	if (client->tls_client != NULL)
		tls_client_stop(client->tls_client);

	free(client);
}

/*
 * Static functions.
 */

static void
handle_input_chunk(input_state * restrict input, char * restrict chunk)
{
	client_state *client = input->data;
	char *request;

	if (client->mode == CLIENT_MODE_CHECK_RESULT) {
		chomp(chunk);
		client->command = parse_check_result(chunk, client->delimiter);
	} else
		client->command = parse_command(chunk);

	free(chunk);

	client->command_length = strlen(client->command);
	client->command[client->command_length++] = '\n';

	xasprintf(&request, "PUSH %u", client->command_length);
	info("%s C: %s", client->tls->peer, request);
	tls_write_line(client->tls, request);
	free(request);

	tls_read_line(client->tls, handle_tls_push_response);
}

static void
handle_input_eof(input_state *input)
{
	client_state *client = input->data;

	client->input = NULL;
	send_request(client->tls, "QUIT");
	tls_read_line(client->tls, handle_tls_quit_response);
}

static void
handle_tls_connect(tls_state *tls)
{
	client_state *client = tls->data;
	char *session_id = generate_session_id();
	char *request = concat("MOIN 1 ", session_id);

	client->tls = tls;
	tls_set_connection_id(tls, session_id);
	free(session_id);
	send_request(tls, request);
	free(request);
	tls_read_line(tls, handle_tls_moin_response);
}

static void
handle_tls_moin_response(tls_state * restrict tls, char * restrict line)
{
	client_state *client = tls->data;
	int protocol_version;
	char *args[2];

	info("%s S: %s", tls->peer, line);

	if (strncasecmp("MOIN", line, 4) == 0) {
		if (!parse_line(line, args, 2))
			bail(tls, "Cannot parse MOIN response");
		else if ((protocol_version = atoi(args[1])) <= 0)
			bail(tls, "Expected protocol version");
		else if (protocol_version != 1)
			bail(tls, "Protocol version %d not supported",
			    protocol_version);
		else { /* The handshake succeeded. */
			char separator;

			if (client->mode == CLIENT_MODE_CHECK_RESULT)
				separator = '\27';
			else
				separator = '\n';

			debug("Protocol handshake successful");
			client->input = input_start(separator);
			client->input->data = client;

			input_on_eof(client->input, handle_input_eof);
			input_read_chunk(client->input, handle_input_chunk);
		}
	} else if (!server_is_grumpy(tls, line))
		bail(tls, "Received unexpected MOIN response");

	free(line);
}

static void
handle_tls_push_response(tls_state * restrict tls, char * restrict line)
{
	client_state *client = tls->data;

	info("%s S: %s", tls->peer, line);

	if (strcasecmp("OKAY", line) == 0) {
		notice("Transmitting to %s: %.*s", tls->peer,
		    (int)client->command_length - 1, client->command);
		tls_write(tls, client->command, client->command_length, free);
		tls_read_line(tls, handle_tls_command_response);
	} else if (!server_is_grumpy(tls, line)) {
		free(client->command);
		bail(tls, "Received unexpected PUSH response");
	}

	free(line);
}

static void
handle_tls_command_response(tls_state * restrict tls, char * restrict line)
{
	client_state *client = tls->data;

	info("%s S: %s", tls->peer, line);

	if (strcasecmp("OKAY", line) == 0)
		input_read_chunk(client->input, handle_input_chunk);
	else if (!server_is_grumpy(tls, line))
		bail(tls, "Received unexpected response after sending command(s)");

	free(line);
}

static void
handle_tls_quit_response(tls_state * restrict tls, char * restrict line)
{
	client_state *client = tls->data;

	info("%s S: %s", tls->peer, line);

	if (strcasecmp("OKAY", line) == 0)
		client_stop(client);
	else if (!server_is_grumpy(tls, line))
		bail(tls, "Received unexpected QUIT response");

	free(line);
}

static void
send_request(tls_state * restrict tls, const char * restrict request)
{
	info("%s C: %s", tls->peer, request);
	tls_write_line(tls, request);
}

static void
bail(tls_state * restrict tls, const char * restrict fmt, ...)
{
	va_list ap;
	char *message;
	client_state *client = tls->data;

	va_start(ap, fmt);
	xvasprintf(&message, fmt, ap);
	va_end(ap);

	info("%s C: %s %s", tls->peer, "BAIL", message);

	tls_write(tls, "BAIL ", sizeof("BAIL ") - 1, NULL);
	tls_write_line(tls, message);
	client_stop(client);
	critical("%s", message);
	free(message);
	exit_code = EXIT_FAILURE;
}

static bool
server_is_grumpy(tls_state * restrict tls, char * restrict line)
{
	client_state *client = tls->data;

	if (strncasecmp("FAIL", line, 4) == 0
	    || strncasecmp("BAIL", line, 4) == 0) {
		client_stop(client);
		critical("Server said: %s", line);
		exit_code = EXIT_FAILURE;
		return true;
	}
	return false;
}

static char *
generate_session_id(void)
{
	unsigned char random_bytes[NUM_SESSION_ID_BYTES];

	(void)RAND_pseudo_bytes(random_bytes, sizeof(random_bytes));
	return base64(random_bytes, sizeof(random_bytes));
}

static char *
base64(const unsigned char *input, size_t input_size)
{
	BIO *bio_b64, *bio_mem;
	char *mem_data, *output;
	long output_size;

	bio_mem = BIO_new(BIO_s_mem());
	bio_b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(bio_b64, BIO_FLAGS_BASE64_NO_NL);
	bio_b64 = BIO_push(bio_b64, bio_mem);
	(void)BIO_write(bio_b64, input, (int)input_size);
	(void)BIO_flush(bio_b64);

	output_size = BIO_get_mem_data(bio_b64, &mem_data) + 1;
	output = xmalloc((size_t)output_size);
	(void)memcpy(output, mem_data, (size_t)output_size);
	output[output_size - 1] = '\0';

	BIO_free_all(bio_b64);
	return output;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
