/*
 * Copyright (c) 2014 Alexander Golovko <alexandro@onsec.ru>
 * All rights reserved.
 * Portions (c) 2015 Matthias Bethke <matthias@towiski.de>
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

#ifndef __NSCANG_CLIENT_H
#define __NSCANG_CLIENT_H

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "uthash.h"

typedef enum { NSCANG_STATE_NONE=0, NSCANG_STATE_NEW, NSCANG_STATE_MOIN } nscang_state_t;
typedef enum { NSCANG_RESP_MOIN=1, NSCANG_RESP_OKAY } nscang_response_t;
typedef enum {
	NSCANG_ERROR_MALLOC=1,
	NSCANG_ERROR_TIMEOUT,
	NSCANG_ERROR_TOO_LONG_RESPONSE,
	NSCANG_ERROR_BAD_PROTO_VERSION,
	NSCANG_ERROR_PROTOCOL_MISMATCH,
	NSCANG_ERROR_UNKNOWN_RESPONSE,
	NSCANG_ERROR_BAIL,
	NSCANG_ERROR_FAIL,
	NSCANG_ERROR_BAD_STATE,
	NSCANG_ERROR_SSL_CTX_CREATE=101,
	NSCANG_ERROR_SSL_CIPHERS,
	NSCANG_ERROR_SSL_BIO_CREATE,
	NSCANG_ERROR_SSL_CREATE,
	NSCANG_ERROR_SSL
} nscang_error_t;

typedef struct {
	SSL_CTX *ssl_ctx;
	BIO *bio;
	SSL *ssl;
	nscang_state_t state;

	char *identity;
	char *psk;

	nscang_error_t _errno;
	char errstr[1024];

	UT_hash_handle hh;
} nscang_client_t;

int nscang_client_init(nscang_client_t *c, char *host, int port,
                       char *ciphers, char *identity, char *psk);
void nscang_client_free(nscang_client_t *c);
void nscang_client_disconnect(nscang_client_t *c);
int nscang_client_send_moin(nscang_client_t *c, int timeout);
int nscang_client_send_command(nscang_client_t *c, const char *command, int timeout);
int nscang_client_send_push(nscang_client_t *c, char *host, char *service,
                            int status, char *message, int timeout);
int nscang_client_send_quit(nscang_client_t *c);
char *nscang_client_errstr(nscang_client_t *c, char *buf, int buf_size);

#endif /* __NSCANG_CLIENT_H */
