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
 * This is an implementation of a single-threaded event-based TLS client and
 * server which supports both synchronous and asynchronous protocols.
 *
 * Coping with OpenSSL's retry semantics is a bit hairy when operating with
 * non-blocking sockets:
 *
 * - When calling SSL_read(), we cannot simply replace our `tls->input_buffer'
 *   with a buffer on the stack, as this would break the following requirement
 *   (documented in the SSL_read() man page): "When an SSL_read() operation has
 *   to be repeated because of SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE, it
 *   must be repeated with the same arguments."  In the following thread, David
 *   Schwartz says that one could get around this requirement by setting some
 *   (seemingly unrelated) OpenSSL options:
 *
 *   http://thread.gmane.org/gmane.comp.encryption.openssl.devel/12242
 *
 *   However, even if this were true for the current OpenSSL code, we'll rather
 *   not rely on the behaviour until the documentation is updated.
 *
 * - We set the SSL_MODE_ENABLE_PARTIAL_WRITE option because SSL_write() splits
 *   up the data into 16 kB chunks, so sending larger pieces of data would
 *   result in multiple trips around the event loop, as the first SSL_write()
 *   attempt(s) would always be rejected with SSL_ERROR_WANT_WRITE.
 *
 * - An application must not wait for I/O readiness of the underlying socket
 *   unless an OpenSSL call explicitly returned SSL_ERROR_WANT_READ or
 *   SSL_ERROR_WANT_WRITE.  Therefore, we kick off new SSL I/O requests via
 *   ev_feed_event() instead of having libev wait for actual socket readiness
 *   before calling the OpenSSL functions.
 *
 * - As an SSL_read() call invalidates the current state of the SSL_write()
 *   stream and vice versa (i.e., there's only a single state of an SSL
 *   connection), such a call has to be followed by a check whether the `ev_io'
 *   watcher for the opposite I/O direction is currently active.  If so, we use
 *   ev_feed_event() to feed the event that watcher is waiting for into the
 *   event loop (we check the `events' member of the `ev_io' struct to find out
 *   whether it's EV_READ or EV_WRITE).  This forces an SSL_read()/SSL_write()
 *   call, which updates the state of that stream.
 *
 * For additional explanations, see the relevant OpenSSL man pages and the
 * following thread:
 *
 * http://thread.gmane.org/gmane.comp.encryption.openssl.devel/15766
 *
 * An alternative approach would be to use a BIO_s_mem() layer instead of
 * letting OpenSSL handle the socket I/O itself.  This is what the Perl module
 * AnyEvent::Handle does, as explained here:
 *
 * http://lists.schmorp.de/pipermail/libev/2008q2/000325.html
 * http://funcptr.net/2012/04/08/openssl-as-a-filter-(or-non-blocking-openssl)/
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ev.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "log.h"
#include "system.h"
#include "tls.h"
#include "wrappers.h"

#ifndef INET6_ADDRSTRLEN
# define INET6_ADDRSTRLEN 46
#endif

#define LINE_MAX_SIZE 2048
#define LINE_BUFFER_SIZE 128
#define LINE_TERMINATOR "\r\n"

enum {
	READ_LINE, /* The read() function relies on READ_LINE == 0. */
	READ_BYTES
};

enum {
	TLS_CLIENT,
	TLS_SERVER
};

static void (*warning_f)(const char *, ...);
static void (*error_f)(const char *, ...);

static SSL_CTX *initialize_openssl(const SSL_METHOD *, const char *);
static tls_state *tls_new(int, int);
static void tls_free(tls_state *);
static void connect_cb(EV_P_ ev_io *, int);
static void accept_tcp_cb(EV_P_ ev_io *, int);
static void accept_ssl_cb(EV_P_ ev_io *, int);
static void read_cb(EV_P_ ev_io *, int);
static void write_cb(EV_P_ ev_io *, int);
static void shutdown_cb(EV_P_ ev_io *, int);
static void timeout_cb(EV_P_ ev_timer *, int);
static void default_timeout_handler(tls_state *);
static void default_line_too_long_handler(tls_state *);
static void start_shutdown(tls_state *);
static char *read_line(tls_state *);
static char *read_bytes(tls_state *);
static void write_buffered(tls_state * restrict, const void * restrict, size_t);
static void reset_watcher_state(EV_P_ ev_io *);
static void check_tls_error(EV_P_ ev_io *, int);
static void log_tls_message(void (*)(const char *, ...), const char *, ...);
static bool get_peer_address(int, char *, socklen_t);

/*
 * Exported functions.
 */

tls_client_state *
tls_client_start(const char *ciphers)
{
	tls_client_state *ctx = xmalloc(sizeof(tls_client_state));

	debug("Starting TLS client");

	ctx->ssl = initialize_openssl(SSLv23_client_method(), ciphers);
	return ctx;
}

tls_server_state *
tls_server_start(const char * restrict host_port,
                 const char * restrict ciphers,
                 ev_tstamp timeout,
                 void handle_connect(tls_state *),
                 unsigned int check_psk(SSL *,
                                        const char *,
                                        unsigned char *,
                                        unsigned int))
{
	tls_server_state *ctx = xmalloc(sizeof(tls_server_state));

	debug("Starting TLS server");

	ctx->ssl = initialize_openssl(SSLv23_server_method(), ciphers);
	SSL_CTX_set_psk_server_callback(ctx->ssl, check_psk);

	/*
	 * We call BIO_set_nbio() in addition to BIO_set_nbio_accept() in order
	 * to set non-blocking I/O mode also for the *accepted* sockets, not
	 * only for the listening socket:
	 *
	 * http://permalink.gmane.org/gmane.comp.encryption.openssl.user/27438
	 */
	if ((ctx->bio = BIO_new_accept((char *)host_port)) == NULL)
		die("Cannot create socket: %m");
	(void)BIO_set_bind_mode(ctx->bio, BIO_BIND_REUSEADDR);
	(void)BIO_set_nbio_accept(ctx->bio, 1);
	(void)BIO_set_nbio(ctx->bio, 1); /* For the *accepted* sockets. */
	if (BIO_do_accept(ctx->bio) <= 0)
		log_tls_message(die, "Cannot bind to %s", host_port);

	debug("Listening on %s", host_port);

	ctx->connect_handler = handle_connect;
	ctx->timeout = timeout;
	ctx->accept_watcher.data = ctx;

	ev_io_init(&ctx->accept_watcher, accept_tcp_cb,
	    (int)BIO_get_fd(ctx->bio, NULL), EV_READ);
	ev_io_start(EV_DEFAULT_UC_ &ctx->accept_watcher);

	return ctx;
}

void
tls_connect(tls_client_state *ctx,
            const char * restrict server,
            ev_tstamp timeout,
            int flags,
            void handle_connect(tls_state *),
            void handle_timeout(tls_state *),
            unsigned int set_psk(SSL *,
                                 const char *,
                                 char *,
                                 unsigned int,
                                 unsigned char *,
                                 unsigned int))
{
	tls_state *tls = tls_new(TLS_CLIENT, flags);
	char *colon;

	tls->data = ctx->data;
	tls->connect_handler = handle_connect;
	tls->timeout = timeout;
	tls->peer = xstrdup(server);
	if ((colon = strrchr(tls->peer, ':')) != NULL)
		*colon = '\0'; /* Strip off the port. */

	tls_on_timeout(tls, handle_timeout);

	if ((tls->bio = BIO_new_connect((char *)server)) == NULL)
		log_tls_message(die, "Cannot create BIO object");
	(void)BIO_set_nbio(tls->bio, 1);

	if ((tls->ssl = SSL_new(ctx->ssl)) == NULL)
		log_tls_message(die, "Cannot create SSL object");
	SSL_set_bio(tls->ssl, tls->bio, tls->bio);
	SSL_set_psk_client_callback(tls->ssl, set_psk);

	ev_invoke(EV_DEFAULT_UC_ &tls->init_watcher, EV_CUSTOM);
}

void
tls_read(tls_state *tls, void handle_read(tls_state *, char *), size_t size)
{
	if (ev_is_active(&tls->read_watcher))
		die("Internal error: Concurrent read requests issued");

	if (size == READ_LINE) {
		tls->read_mode = READ_LINE;
		tls->input_size = LINE_BUFFER_SIZE;
		debug("Waiting for a line from %s", tls->peer);
	} else {
		tls->read_mode = READ_BYTES;
		tls->input_size = size;
		debug("Waiting for %zu byte(s) from %s", size,
		    tls->peer);
	}
	tls->read_handler = handle_read;

	ev_io_set(&tls->read_watcher, tls->fd, EV_READ);
	ev_io_start(EV_DEFAULT_UC_ &tls->read_watcher);
	ev_feed_event(EV_DEFAULT_UC_ &tls->read_watcher, EV_READ);
}

void
tls_read_line(tls_state *tls, void handle_read_line(tls_state *, char *))
{
	tls_read(tls, handle_read_line, READ_LINE);
}

void
tls_write(tls_state * restrict tls, void * restrict data, size_t size,
          void free_data(void *))
{
	if (tls->output == NULL && buffer_size(tls->output_buffer) == 0
	    && free_data != NULL) {
		debug("Zero-copying %zu byte(s) for %s", size, tls->peer);
		tls->output = data;
		tls->output_size = size;
		tls->free_output = free_data;
		if (!ev_is_active(&tls->write_watcher)) {
			ev_io_set(&tls->write_watcher, tls->fd, EV_WRITE);
			ev_io_start(EV_DEFAULT_UC_ &tls->write_watcher);
			ev_feed_event(EV_DEFAULT_UC_ &tls->write_watcher,
			    EV_WRITE);
		}
	} else {
		write_buffered(tls, data, size);
		if (free_data != NULL)
			free_data(data);
	}
}

void
tls_write_line(tls_state * restrict tls, const char * restrict string)
{
	write_buffered(tls, string, strlen(string));
	write_buffered(tls, LINE_TERMINATOR, sizeof(LINE_TERMINATOR) - 1);
}

void
tls_shutdown(tls_state *tls)
{
	/*
	 * Make sure this function won't be called recursively if an error or
	 * timeout occurs during the TLS shutdown.
	 */
	tls->error_handler = NULL;
	tls->timeout_handler = default_timeout_handler;

	tls_on_drain(tls, start_shutdown);
}

void
tls_client_stop(tls_client_state *ctx)
{
	debug("Stopping TLS client");

	SSL_CTX_free(ctx->ssl);
	free(ctx);
}

void
tls_server_stop(tls_server_state *ctx)
{
	debug("Stopping TLS server");

	SSL_CTX_free(ctx->ssl);
	BIO_free_all(ctx->bio);
	free(ctx);
}

void
tls_set_connection_id(tls_state *tls, const char *id)
{
	char *peer;

	xasprintf(&peer, "%s (ID: %s)", tls->peer, id);
	free(tls->peer);
	tls->peer = peer;
}

void
tls_on_drain(tls_state *tls, void handle_drain(tls_state *))
{
	tls->drain_handler = handle_drain;
	if (!ev_is_active(&tls->write_watcher))
		tls->drain_handler(tls);
}

void
tls_on_error(tls_state *tls, void handle_error(tls_state *))
{
	tls->error_handler = handle_error;
}

void
tls_on_timeout(tls_state *tls, void handle_timeout(tls_state *))
{
	if (ev_is_active(&tls->timeout_watcher))
		ev_timer_stop(EV_DEFAULT_UC_ &tls->timeout_watcher);

	tls->timeout_handler = handle_timeout != NULL ?
	    handle_timeout : default_timeout_handler;

	if (tls->timeout > 0.0) {
		ev_timer_set(&tls->timeout_watcher, tls->timeout, 0.0);
		ev_timer_start(EV_DEFAULT_UC_ &tls->timeout_watcher);
		tls->last_activity = ev_now(EV_DEFAULT_UC);
	}
}

void
tls_on_line_too_long(tls_state *tls, void handle_line_too_long(tls_state *))
{
	tls->line_too_long_handler = handle_line_too_long;
}

/*
 * Static functions.
 */

static SSL_CTX *
initialize_openssl(const SSL_METHOD *method, const char *ciphers)
{
	SSL_CTX *ssl_ctx;

	(void)SSL_library_init();
	SSL_load_error_strings();
	OPENSSL_config(NULL);

	(void)atexit(ERR_free_strings);
	(void)atexit(CONF_modules_free);

	if ((ssl_ctx = SSL_CTX_new(method)) == NULL)
		die("Cannot create SSL context");
	if (SSL_CTX_set_cipher_list(ssl_ctx, ciphers) != 1)
		die("Cannot set SSL cipher(s)");
	(void)SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
	(void)SSL_CTX_set_mode(ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

	(void)signal(SIGPIPE, SIG_IGN);

	return ssl_ctx;
}

static tls_state *
tls_new(int type, int flags)
{
	tls_state *tls = xmalloc(sizeof(tls_state));

	debug("Initializing connection context");

	tls->data = NULL;
	tls->id = NULL;
	tls->addr = NULL;
	tls->peer = NULL;
	tls->init_watcher.data = tls;
	tls->read_watcher.data = tls;
	tls->write_watcher.data = tls;
	tls->shutdown_watcher.data = tls;
	tls->timeout_watcher.data = tls;
	tls->timeout = 0.0;
	tls->last_activity = ev_now(EV_DEFAULT_UC);
	tls->input_buffer = buffer_new();
	tls->output_buffer = buffer_new();
	tls->input = NULL;
	tls->output = NULL;
	tls->input_size = 0;
	tls->input_offset = 0;
	tls->output_size = 0;
	tls->output_offset = 0;
	tls->connect_handler = NULL;
	tls->read_handler = NULL;
	tls->drain_handler = NULL;
	tls->error_handler = NULL;
	tls->timeout_handler = default_timeout_handler;
	tls->line_too_long_handler = default_line_too_long_handler;
	tls->free_output = free;
	tls->ssl = NULL;
	tls->bio = NULL;
	tls->fd = -1;
	tls->read_mode = 0;

	if (flags & TLS_AUTO_DIE) {
		warning_f = die;
		error_f = die;
	} else {
		warning_f = warning;
		error_f = error;
	}

	ev_init(&tls->init_watcher,
	    type == TLS_CLIENT ? connect_cb : accept_ssl_cb);
	ev_init(&tls->read_watcher, read_cb);
	ev_init(&tls->write_watcher, write_cb);
	ev_init(&tls->shutdown_watcher, shutdown_cb);
	ev_init(&tls->timeout_watcher, timeout_cb);

	/* Check for data before checking for timeout. */
	ev_set_priority(&tls->timeout_watcher, -1);

	return tls;
}

static void
tls_free(tls_state *tls)
{
	debug("Destroying connection context");

	if (ev_is_active(&tls->init_watcher))
		ev_io_stop(EV_DEFAULT_UC_ &tls->init_watcher);
	if (ev_is_active(&tls->read_watcher))
		ev_io_stop(EV_DEFAULT_UC_ &tls->read_watcher);
	if (ev_is_active(&tls->write_watcher))
		ev_io_stop(EV_DEFAULT_UC_ &tls->write_watcher);
	if (ev_is_active(&tls->shutdown_watcher))
		ev_io_stop(EV_DEFAULT_UC_ &tls->shutdown_watcher);
	if (ev_is_active(&tls->timeout_watcher))
		ev_timer_stop(EV_DEFAULT_UC_ &tls->timeout_watcher);

	buffer_free(tls->input_buffer);
	buffer_free(tls->output_buffer);

	if (tls->input != NULL)
		free(tls->input);
	if (tls->output != NULL)
		free(tls->output);
	if (tls->addr != NULL)
		free(tls->addr);
	if (tls->peer != NULL)
		free(tls->peer);
	if (tls->ssl != NULL)
		SSL_free(tls->ssl);

	free(tls);
}

static void
connect_cb(EV_P_ ev_io *w, int revents)
{
	tls_state *tls = w->data;
	int result;

	tls->last_activity = ev_now(EV_A);
	result = SSL_connect(tls->ssl);

	/*
	 * Initially, connect_cb() is called by ev_invoke() from tls_connect()
	 * with `revents' set to EV_CUSTOM.  The above SSL_connect() call then
	 * creates the socket descriptor (`tls->fd') we can watch.
	 */
	if (revents & EV_CUSTOM
	    && (tls->fd = (int)BIO_get_fd(tls->bio, NULL)) == -1)
		log_tls_message(die, "Cannot create socket");

	if (result <= 0) {
		debug("TLS connection not (yet) established");
		if (revents & EV_CUSTOM) {
			ev_io_set(w, tls->fd, EV_WRITE);
			ev_io_start(EV_A_ &tls->init_watcher);
			ev_feed_event(EV_A_ &tls->init_watcher, EV_WRITE);
		}
		check_tls_error(EV_A_ w, result);
	} else {
		debug("TLS connection established");
		if (!(revents & EV_CUSTOM))
			ev_io_stop(EV_A_ w);
		tls->connect_handler(tls);
	}
}

static void
accept_tcp_cb(EV_P_ ev_io *w, int revents __attribute__((__unused__)))
{
	tls_server_state *ctx = w->data;

	while (1) { /* Accept all available TCP connections. */
		tls_state *tls;

		if (BIO_do_accept(ctx->bio) <= 0) {
			if (BIO_should_retry(ctx->bio))
				debug("Cannot accept connection right now");
			else
				log_tls_message(warning,
				    "Cannot accept connection");
			return; /* Let's do something else. */
		}

		tls = tls_new(TLS_SERVER, TLS_NO_AUTO_DIE);
		tls->bio = BIO_pop(ctx->bio);
		tls->fd = (int)BIO_get_fd(tls->bio, NULL);
		tls->addr = xmalloc(INET6_ADDRSTRLEN);
		tls->connect_handler = ctx->connect_handler;
		tls->timeout = ctx->timeout;
		tls->data = ctx->data;

		if (!get_peer_address(tls->fd, tls->addr, INET6_ADDRSTRLEN)) {
			tls_free(tls);
			return; /* Let's do something else. */
		}

		tls_on_timeout(tls, default_timeout_handler);

		if ((tls->ssl = SSL_new(ctx->ssl)) == NULL) {
			log_tls_message(error,
			    "Cannot create SSL object for %s", tls->addr);
			tls_free(tls);
			return; /* Let's do something else. */
		}
		SSL_set_bio(tls->ssl, tls->bio, tls->bio);

		ev_io_set(&tls->init_watcher, tls->fd, EV_READ);
		ev_io_start(EV_A_ &tls->init_watcher);
		ev_feed_event(EV_A_ &tls->init_watcher, EV_READ);

		debug("Accepted connection from %s", tls->addr);
	}
}

static void
accept_ssl_cb(EV_P_ ev_io *w, int revents __attribute__((__unused__)))
{
	tls_state *tls = w->data;
	int result;

	tls->last_activity = ev_now(EV_A);

	if ((result = SSL_accept(tls->ssl)) <= 0) {
		debug("TLS handshake with %s not (yet) successful", tls->addr);
		check_tls_error(EV_A_ w, result);
	} else { /* The TLS connection is established. */
		if ((tls->id = SSL_get_psk_identity(tls->ssl)) == NULL) {
			error("Cannot retrieve client identity");
			tls_free(tls);
		} else {
			xasprintf(&tls->peer, "%s@%s", tls->id, tls->addr);
			debug("TLS handshake with %s successful", tls->peer);
			ev_io_stop(EV_A_ w);
			tls->connect_handler(tls);
		}
	}
}

static void
read_cb(EV_P_ ev_io *w, int revents __attribute__((__unused__)))
{
	tls_state *tls = w->data;
	char *data;

	reset_watcher_state(EV_A_ &tls->write_watcher);
	tls->last_activity = ev_now(EV_A);

	data = tls->read_mode == READ_LINE ? read_line(tls) : read_bytes(tls);

	if (data != NULL) {
		ev_io_stop(EV_A_ w);
		tls->read_handler(tls, data);
	}
}

static void
write_cb(EV_P_ ev_io *w, int revents __attribute__((__unused__)))
{
	tls_state *tls = w->data;
	int n;

	reset_watcher_state(EV_A_ &tls->read_watcher);
	tls->last_activity = ev_now(EV_A);

	do {
		int n_todo;

		if (tls->output == NULL) {
			tls->output = buffer_slurp(tls->output_buffer,
			    &tls->output_size);
			tls->free_output = free;
		}
		do {
			n_todo = (int)(tls->output_size
			    - tls->output_offset);

			if ((n = SSL_write(tls->ssl,
			    tls->output + tls->output_offset, n_todo)) <= 0) {
				debug("Sent 0 of %d bytes to %s",
				    n_todo, tls->peer);
				check_tls_error(EV_A_ w, n);
			} else if (n < n_todo) {
				debug("Sent %d of %d bytes to %s",
				    n, n_todo, tls->peer);
				tls->output_offset += (size_t)n;
			} else { /* We're done (n == n_todo). */
				debug("Sent %d bytes to %s, as requested",
				    n, tls->peer);
				tls->free_output(tls->output);
				tls->output = NULL;
				tls->output_size = 0;
				tls->output_offset = 0;
				if (buffer_size(tls->output_buffer) == 0) {
					ev_io_stop(EV_A_ w);
					if (tls->drain_handler != NULL)
						tls->drain_handler(tls);
				}
			}
		} while (n > 0 && n < n_todo);
	} while (n > 0 && buffer_size(tls->output_buffer) > 0);
}

static void
shutdown_cb(EV_P_ ev_io *w, int revents __attribute__((__unused__)))
{
	tls_state *tls = w->data;
	int result;

	tls->last_activity = ev_now(EV_A);

	do {
		switch (result = SSL_shutdown(tls->ssl)) {
		case 1: /* We sent and received peer's `close notify'. */
			debug("TLS shutdown with %s successful", tls->peer);
			tls_free(tls);
			break;
		case 0: /* We sent `close notify', let's wait for the peer. */
			debug("Sent TLS `close notify' alert to %s", tls->peer);
			break;
		default: /* We must wait for I/O, or an error occurred. */
			debug("TLS shutdown with %s not (yet) successful",
			    tls->peer);
			check_tls_error(EV_A_ w, result);
		}
	} while (result == 0);
}

static void
timeout_cb(EV_P_ ev_timer *w, int revents __attribute__((__unused__)))
{
	tls_state *tls = w->data;
	ev_tstamp diff = tls->last_activity + tls->timeout - ev_now(EV_A);

	if (diff > 0.0) {
		debug("Resetting timeout watcher for %s, %.2f seconds left",
		    tls->peer, (double)diff);

		ev_timer_set(w, diff, 0.0);
		ev_timer_start(EV_A_ w);
	} else {
		warning_f("Connection to %s timed out",
		    tls->peer != NULL ? tls->peer : tls->addr);

		if (ev_is_active(&tls->init_watcher))
			ev_io_stop(EV_DEFAULT_UC_ &tls->init_watcher);
		if (ev_is_active(&tls->read_watcher))
			ev_io_stop(EV_DEFAULT_UC_ &tls->read_watcher);
		if (ev_is_active(&tls->write_watcher))
			ev_io_stop(EV_DEFAULT_UC_ &tls->write_watcher);
		if (ev_is_active(&tls->shutdown_watcher))
			ev_io_stop(EV_DEFAULT_UC_ &tls->shutdown_watcher);

		ev_timer_set(w, tls->timeout, 0.0);
		ev_timer_start(EV_A_ w);

		tls->timeout_handler(tls);
	}
}

static void
default_timeout_handler(tls_state *tls)
{
	tls_free(tls);
}

static void
default_line_too_long_handler(tls_state *tls)
{
	tls_free(tls);
}

static void
start_shutdown(tls_state *tls)
{
	debug("Initiating shutdown of connection to %s", tls->peer);

	ev_io_set(&tls->shutdown_watcher, tls->fd, EV_WRITE);
	ev_io_start(EV_DEFAULT_UC_ &tls->shutdown_watcher);
	ev_feed_event(EV_DEFAULT_UC_ &tls->shutdown_watcher, EV_WRITE);
}

static char *
read_line(tls_state *tls)
{
	char *line = NULL;
	int n;

	if (tls->input == NULL)
		tls->input = xmalloc(tls->input_size);
	do {
		if ((line = buffer_read_line(tls->input_buffer)) != NULL) {
			debug("Received complete line from %s", tls->peer);
			free(tls->input);
			tls->input = NULL;
			tls->input_size = 0;
			break;
		}
		if (buffer_size(tls->input_buffer) + tls->input_size
		    > LINE_MAX_SIZE) {
			warning_f("Line received from %s is too long",
			    tls->peer);
			ev_io_stop(EV_DEFAULT_UC_ &tls->read_watcher);
			tls->line_too_long_handler(tls);
			break;
		}

		if ((n = SSL_read(tls->ssl, tls->input, (int)tls->input_size))
		    <= 0) {
			debug("Didn't receive line from %s (yet)", tls->peer);
			check_tls_error(EV_DEFAULT_UC_ &tls->read_watcher, n);
		} else {
			debug("Buffered %d bytes from %s", n, tls->peer);
			buffer_append(tls->input_buffer, tls->input,
			    (size_t)n);
		}
	} while (n > 0);

	return line;
}

static char *
read_bytes(tls_state *tls)
{
	char *bytes = NULL;
	int n, n_todo;

	if (tls->input == NULL)
		tls->input = xmalloc(tls->input_size + 1); /* 1 for '\0'. */
	do {
		n_todo = (int)(tls->input_size - tls->input_offset);

		if ((n = SSL_read(tls->ssl, tls->input + tls->input_offset,
		    n_todo)) <= 0) {
			debug("Received 0 of %d bytes from %s", n_todo,
			    tls->peer);
			check_tls_error(EV_DEFAULT_UC_ &tls->read_watcher, n);
		} else if (n < n_todo) {
			debug("Received %d of %d bytes from %s", n, n_todo,
			    tls->peer);
			tls->input_offset += (size_t)n;
		} else { /* We're done (n == n_todo). */
			debug("Received %d bytes from %s, as requested",
			    n, tls->peer);
			tls->input[tls->input_size] = '\0';
			bytes = (char *)tls->input;
			tls->input = NULL;
			tls->input_size = 0;
			tls->input_offset = 0;
			break; /* Make Clang's Static Analyzer happy. */
		}
	} while (n > 0 && n < n_todo);

	return bytes;
}

static void
write_buffered(tls_state * restrict tls, const void * restrict data, size_t size)
{
	debug("Queueing %zu byte(s) for %s", size, tls->peer);

	buffer_append(tls->output_buffer, data, size);

	if (!ev_is_active(&tls->write_watcher)) {
		ev_io_set(&tls->write_watcher, tls->fd, EV_WRITE);
		ev_io_start(EV_DEFAULT_UC_ &tls->write_watcher);
		ev_feed_event(EV_DEFAULT_UC_ &tls->write_watcher, EV_WRITE);
	}
}

static void
reset_watcher_state(EV_P_ ev_io *w)
{
	tls_state *tls = w->data;

	if (ev_is_active(w)) {
		debug("Resetting %s watcher state for %s",
		    w == &tls->read_watcher ? "input" : "output", tls->peer);
		ev_feed_event(EV_A_ w, w->events);
	}
}

static void
check_tls_error(EV_P_ ev_io *w, int code)
{
	tls_state *tls = w->data;
	const char *peer = tls->peer != NULL ? tls->peer : tls->addr;

	/* See the SSL_get_error() man page. */
	switch (SSL_get_error(tls->ssl, code)) {
	case SSL_ERROR_WANT_ACCEPT:
	case SSL_ERROR_WANT_READ:
		if (!(w->events & EV_READ)) {
			ev_io_stop(EV_A_ w);
			ev_io_set(w, w->fd, EV_READ);
			ev_io_start(EV_A_ w);
		}
		debug("Waiting for input from %s", peer);
		return;
	case SSL_ERROR_WANT_CONNECT:
	case SSL_ERROR_WANT_WRITE:
		if (!(w->events & EV_WRITE)) {
			ev_io_stop(EV_A_ w);
			ev_io_set(w, w->fd, EV_WRITE);
			ev_io_start(EV_A_ w);
		}
		debug("Waiting for output to %s", peer);
		return;
	case SSL_ERROR_SYSCALL:
		if (ERR_peek_error() != 0)
			log_tls_message(warning_f, "TLS I/O error (%s)", peer);
		else if (code == 0)
			warning_f("%s aborted the TLS connection", peer);
		else if (code == -1)
			switch (errno) {
			case EINTR:
				debug("TLS I/O call has been interrupted");
				return;
			case 0:
				/*
				 * As far as I know, the only situation which
				 * might trigger this is an EOF during TLS
				 * shutdown.  See:
				 *
				 * http://marc.info/?m=127100694709805
				 */
				warning_f("%s aborted the TLS shutdown", peer);
			default:
				warning_f("Socket error (%s): %m", peer);
			}
		else
			error_f("TLS I/O error (%s)", peer);
		break;
	case SSL_ERROR_ZERO_RETURN:
		warning_f("%s closed the TLS connection", peer);
		(void)SSL_shutdown(tls->ssl); /* Try to send `close notify'. */
		break;
	case SSL_ERROR_SSL:
		if (ERR_peek_error() != 0)
			log_tls_message(warning_f, "TLS error (%s)", peer);
		else
			error_f("TLS error (%s)", peer);
		break;
	case SSL_ERROR_NONE: /* Should never happen if code <= 0. */
	case SSL_ERROR_WANT_X509_LOOKUP:
	default:
		error_f("Unknown OpenSSL error (%s)", peer);
	}

	if (tls->error_handler != NULL)
		tls->error_handler(tls);

	tls_free(tls);
}

static void
log_tls_message(void log(const char *, ...), const char *format, ...)
{
	unsigned long e;
	va_list ap;
	char *message;

	va_start(ap, format);
	xvasprintf(&message, format, ap);
	va_end(ap);

	if ((e = ERR_get_error()) != 0)
		log("%s: %s", message, ERR_reason_error_string(e));
	else
		log("%s", message);

	free(message);
	ERR_clear_error();
}

static bool
get_peer_address(int fd, char *destination, socklen_t size)
{
	struct sockaddr_storage sa_storage;
	struct sockaddr *sa = (struct sockaddr *)&sa_storage;
	socklen_t len = sizeof(struct sockaddr_storage);

	if (getpeername(fd, sa, &len) == -1) {
		error("Cannot get peer's IP address: %m");
		return false;
	}
	switch (sa->sa_family) {
	case AF_INET:
		if (inet_ntop(AF_INET,
		    &((struct sockaddr_in *)(void *)sa)->sin_addr,
		    destination, size) == NULL) {
			error("Cannot convert IPv4 address to text form: %m");
			return false;
		}
		break;
#ifdef AF_INET6
	case AF_INET6:
		if (inet_ntop(AF_INET6,
		    &((struct sockaddr_in6 *)(void *)sa)->sin6_addr,
		    destination, size) == NULL) {
			error("Cannot convert IPv6 address to text form: %m");
			return false;
		}
		break;
#endif
	default:
		error("Unknown address familiy: %d", sa->sa_family);
		return false;
	}
	return true;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
