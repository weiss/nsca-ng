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

/*
 * The API is somewhat similar to the interface of the AnyEvent::Handle Perl
 * module.  Limitations include:
 *
 * - We don't provide AnyEvent::Handle's on_read() and on_eof() methods, though
 *   they'd be trivial to add if required.
 *
 * - Also, we don't support calling tls_read() or tls_read_line() before the
 *   previous read operation has been completed.  That is, a tls_read() or
 *   tls_read_line() call cannot be followed by another such call except in the
 *   callback function (or subsequent functions).  Adding a read request queue
 *   wouldn't be hard, but we never want to read new stuff before looking at the
 *   data received by the previous read operation, so we really don't need such
 *   functionality.
 */

#ifndef TLS_H
# define TLS_H

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# include <ev.h>
# include <openssl/bio.h>
# include <openssl/ssl.h>

# include "buffer.h"
# include "system.h"

# define TLS_NO_AUTO_DIE 0x0
# define TLS_AUTO_DIE 0x1

typedef struct tls_state_s {
/* public: */
	void *data;     /* Can freely be used by the caller. */
	const char *id; /* Client ID (e.g., "foo"). */
	char *addr;     /* Client IP address (e.g., "192.0.2.2"). */
	char *peer;     /* Client ID and IP address (e.g., "foo@192.0.2.2"). */

/* private: */
	ev_io init_watcher;
	ev_io read_watcher;
	ev_io write_watcher;
	ev_io shutdown_watcher;
	ev_timer timeout_watcher;
	ev_tstamp timeout;
	ev_tstamp last_activity;
	buffer *input_buffer;
	buffer *output_buffer;
	unsigned char *input;
	unsigned char *output;
	size_t input_size;
	size_t input_offset;
	size_t output_size;
	size_t output_offset;
	void (*connect_handler)(struct tls_state_s *);
	void (*read_handler)(struct tls_state_s * restrict, char * restrict);
	void (*drain_handler)(struct tls_state_s *);
	void (*error_handler)(struct tls_state_s *);
	void (*timeout_handler)(struct tls_state_s *);
	void (*line_too_long_handler)(struct tls_state_s *);
	void (*free_output)(void *);
	SSL *ssl;
	BIO *bio;
	int fd;
	unsigned int read_mode : 1;
} tls_state;

typedef struct {
/* public: */
	void *data;

/* private: */
	void (*connect_handler)(tls_state *);
	SSL_CTX *ssl;
	ev_tstamp timeout;
} tls_client_state;

typedef struct {
/* public: */
	void *data;

/* private: */
	void (*connect_handler)(tls_state *);
	ev_io accept_watcher;
	SSL_CTX *ssl;
	BIO *bio;
	ev_tstamp timeout;
} tls_server_state;

tls_client_state *tls_client_start(const char * restrict);
tls_server_state *tls_server_start(const char * restrict,
                                   const char * restrict,
                                   ev_tstamp,
                                   void (*)(tls_state *),
                                   unsigned int (*)(SSL *,
                                                    const char *,
                                                    unsigned char *,
                                                    unsigned int));
void tls_connect(tls_client_state *,
                 const char * restrict,
                 ev_tstamp timeout,
                 int flags,
                 void (*)(tls_state *),
                 void (*)(tls_state *),
                 unsigned int (*)(SSL *,
                                  const char *,
                                  char *,
                                  unsigned int,
                                  unsigned char *,
                                  unsigned int));
void tls_read(tls_state *, void (*)(tls_state *, char *), size_t);
void tls_read_line(tls_state *,
                   void (*)(tls_state * restrict, char * restrict));
void tls_write(tls_state * restrict, void * restrict, size_t, void (*)(void *));
void tls_write_line(tls_state * restrict, const char * restrict);
void tls_shutdown(tls_state *);
void tls_client_stop(tls_client_state *);
void tls_server_stop(tls_server_state *);
void tls_set_connection_id(tls_state *, const char *);
void tls_on_drain(tls_state *, void (*)(tls_state *));
void tls_on_timeout(tls_state *, void (*)(tls_state *));
void tls_on_error(tls_state *, void (*)(tls_state *));
void tls_on_line_too_long(tls_state *, void (*)(tls_state *));

#endif

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
