/*
 * Copyright (c) 2014 Alexander Golovko <alexandro@onsec.ru>
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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "client.h"
#include "uthash.h"

static nscang_client_t *nscang_client_instances;

static pthread_rwlock_t nscang_client_instances_lock =
    PTHREAD_RWLOCK_INITIALIZER;

unsigned int
set_psk(SSL *ssl, const char *hint, char *identity,
        unsigned int max_identity_length, unsigned char *psk,
        unsigned int max_psk_length)
{
	int rc;
	nscang_client_t *c = NULL;

	while (1) {
		rc = pthread_rwlock_rdlock(&nscang_client_instances_lock);
		if ((rc != EBUSY) && (rc != EAGAIN))
			break;
	}
	if (rc == 0) {
		HASH_FIND_PTR(nscang_client_instances, &ssl, c);
		pthread_rwlock_unlock(&nscang_client_instances_lock);
	}
	if (c != NULL) {
		strncpy(identity, c->identity, max_identity_length);
		identity[max_identity_length - 1] = 0x00;

		strncpy((char *)psk, c->psk, max_psk_length);
		psk[max_psk_length - 1] = 0x00;

		return strlen((char *)psk);
	}
	identity[0] = 0x00;
	psk[0] = 0x00;
	return 0;
}

int
nscang_client_init(nscang_client_t *c, char *host, int port, char *ciphers,
                   char *identity, char *psk)
{
	int rc;

	memset(c, 0x00, sizeof(nscang_client_t));

	c->ssl_ctx = SSL_CTX_new(SSLv23_client_method());
	if (c->ssl_ctx == NULL) {
		c->_errno = NSCANG_ERROR_SSL_CTX_CREATE;
		return 0;
	}

	if (ciphers != NULL) {
		if (SSL_CTX_set_cipher_list(c->ssl_ctx, ciphers) != 1) {
			c->_errno = NSCANG_ERROR_SSL_CIPHERS;
			return 0;
		}
	}

	SSL_CTX_set_options(c->ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

	c->bio = BIO_new(BIO_s_connect());
	if (c->bio == NULL) {
		c->_errno = NSCANG_ERROR_SSL_BIO_CREATE;
		return 0;
	}
	BIO_set_conn_hostname(c->bio, host);
	BIO_set_conn_int_port(c->bio, &port);
	BIO_set_nbio(c->bio, 1);

	c->ssl = SSL_new(c->ssl_ctx);
	if (c->ssl == NULL) {
		nscang_client_free(c);
		c->_errno = NSCANG_ERROR_SSL_CREATE;
		return 0;
	}

	SSL_set_bio(c->ssl, c->bio, c->bio);
	SSL_set_connect_state(c->ssl);
	SSL_set_psk_client_callback(c->ssl, set_psk);

	c->identity = malloc(strlen(identity) + 1);
	c->psk = malloc(strlen(psk) + 1);

	if ((c->identity == NULL) || (c->psk == NULL)) {
		c->_errno = NSCANG_ERROR_MALLOC;
		return 0;
	}

	strcpy(c->identity, identity);
	strcpy(c->psk, psk);

	/* Busyloop until we get an rw lock. */
	while (1) {
		rc = pthread_rwlock_wrlock(&nscang_client_instances_lock);
		if (rc != EBUSY)
			break;
	}
	if (rc != 0) {
		c->_errno = NSCANG_ERROR_LOCKING;
		return 0;
	}

	HASH_ADD_PTR(nscang_client_instances, ssl, c);
	pthread_rwlock_unlock(&nscang_client_instances_lock);

	c->state = NSCANG_STATE_NEW;

	return 1;
}

void
nscang_client_free(nscang_client_t *c)
{
	if (c->state != NSCANG_STATE_NONE)
		nscang_client_disconnect(c);

	if (c->ssl != NULL) {
		int rc;

		/* Busyloop until we get an rw lock. */
		while (1) {
			rc = pthread_rwlock_wrlock
			    (&nscang_client_instances_lock);
			if (rc != EBUSY)
				break;
		}
		if (rc == 0) {
			HASH_DEL(nscang_client_instances, c);
			pthread_rwlock_unlock(&nscang_client_instances_lock);
		}
	}
	if (c->ssl != NULL) {
		SSL_free(c->ssl);
		c->ssl = NULL;
		c->bio = NULL;
	}
	if (c->ssl_ctx != NULL) {
		SSL_CTX_free(c->ssl_ctx);
		c->ssl_ctx = NULL;
	}
	if (c->identity != NULL) {
		free(c->identity);
		c->identity = NULL;
	}
	if (c->psk != NULL) {
		free(c->psk);
		c->psk = NULL;
	}

	c->state = NSCANG_STATE_NONE;
}

int
nscang_client_write(nscang_client_t *c, void *buf, int len, int timeout)
{
	int rc;
	time_t endtime;
	fd_set fdset;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 100000;

	endtime = time(NULL) + timeout;

	while (1) {
		if ((rc = SSL_write(c->ssl, buf, len)) > 0)
			return 1;

		if (!BIO_should_retry(c->bio)) {
			c->_errno = NSCANG_ERROR_SSL;
			return 0;
		}

		if (time(NULL) > endtime) {
			c->_errno = NSCANG_ERROR_TIMEOUT;
			return 0;
		}

		FD_ZERO(&fdset);
		FD_SET(BIO_get_fd(c->bio, NULL), &fdset);
		select(1, NULL, &fdset, NULL, &tv);
	}
}

int
nscang_client_response(nscang_client_t *c, int timeout)
{
	int rc;
	time_t endtime;
	fd_set fdset;
	char buf[1024];
	int was_read = 0;
	char *ptr = buf;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 100000;

	endtime = time(NULL) + timeout;

	while (1) {
		rc = SSL_read(c->ssl, ptr, 1024 - was_read);

		if (rc > 0) {
			was_read = was_read + rc;
			ptr = buf + was_read;

			if (buf[was_read - 1] == '\n') {
				if (buf[was_read - 2] == '\r')
					buf[was_read - 2] = 0x00;
				else
					buf[was_read - 1] = 0x00;

				break;
			}

			if (was_read == 1024) {
				c->_errno = NSCANG_ERROR_TOO_LONG_RESPONSE;
				return 0;
			}
		} else if (!BIO_should_retry(c->bio)) {
			c->_errno = NSCANG_ERROR_SSL;
			return 0;
		}

		if (time(NULL) > endtime) {
			c->_errno = NSCANG_ERROR_TIMEOUT;
			return 0;
		}

		FD_ZERO(&fdset);
		FD_SET(BIO_get_fd(c->bio, NULL), &fdset);
		select(1, &fdset, NULL, NULL, &tv);
	}

	if (strncmp(buf, "MOIN", 4) == 0) {
		if (strcmp(buf, "MOIN 1") == 0) {
			return NSCANG_RESP_MOIN;
		} else {
			c->_errno = NSCANG_ERROR_BAD_PROTO_VERSION;
			strncpy(c->errstr, buf + 5, sizeof(c->errstr) - 1);
			c->errstr[sizeof(c->errstr) - 1] = 0x00;
			return 0;
		}
	} else if (strncmp(buf, "OKAY", 4) == 0) {
		return NSCANG_RESP_OKAY;
	} else if (strncmp(buf, "FAIL", 4) == 0) {
		c->_errno = NSCANG_ERROR_FAIL;
		strncpy(c->errstr, buf + 5, sizeof(c->errstr) - 1);
		c->errstr[sizeof(c->errstr) - 1] = 0x00;
		return 0;
	} else if (strncmp(buf, "BAIL", 4) == 0) {
		nscang_client_disconnect(c);
		c->_errno = NSCANG_ERROR_BAIL;
		strncpy(c->errstr, buf + 5, sizeof(c->errstr) - 1);
		c->errstr[sizeof(c->errstr) - 1] = 0x00;
		return 0;
	} else {
		char *msg = "BAIL Unknown response!";

		nscang_client_write(c, msg, sizeof(msg), 0);
		nscang_client_disconnect(c);
		c->_errno = NSCANG_ERROR_UNKNOWN_RESPONSE;
		strncpy(c->errstr, buf, sizeof(c->errstr) - 1);
		c->errstr[sizeof(c->errstr) - 1] = 0x00;
		return 0;
	}
}

void
nscang_client_disconnect(nscang_client_t *c)
{
	fd_set fdset;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 100000;

	if (c->state == NSCANG_STATE_MOIN)
		nscang_client_send_quit(c);

	if ((SSL_shutdown(c->ssl) == -1) && BIO_should_retry(c->bio)) {
		FD_ZERO(&fdset);
		FD_SET(BIO_get_fd(c->bio, NULL), &fdset);
		select(1, NULL, &fdset, NULL, &tv);
		SSL_shutdown(c->ssl);
	}

	SSL_clear(c->ssl);
	BIO_reset(c->bio);

	c->state = NSCANG_STATE_NEW;
}

int
nscang_client_send_moin(nscang_client_t *c, int timeout)
{
	char cmd[64];
	int len, rc;

	if (c->state == NSCANG_STATE_MOIN)
		return 1;

	if (c->state != NSCANG_STATE_NEW) {
		c->_errno = NSCANG_ERROR_BAD_STATE;
		return 0;
	}

	srandom(time(NULL));
	len = snprintf(cmd, sizeof(cmd), "MOIN 1 %08ld%08ld\r\n", random(),
	    random());

	if (!nscang_client_write(c, cmd, len, timeout))
		return 0;

	rc = nscang_client_response(c, timeout);

	if (!rc)
		return 0;

	if (rc != NSCANG_RESP_MOIN) {
		c->_errno = NSCANG_ERROR_PROTOCOL_MISMATCH;
		return 0;
	}

	c->state = NSCANG_STATE_MOIN;
	return 1;
}

int
nscang_client_send_push(nscang_client_t *c, char *host, char *service,
                        int status, char *message, int timeout)
{
	char cmd[64], command[1024];
	int len, rc;

	if (!nscang_client_send_moin(c, timeout))
		return 0;

	if (service == NULL)
		len = snprintf(command, sizeof(command) - 1,
		    "[%u] PROCESS_HOST_CHECK_RESULT;%s;%d;%s",
		    (unsigned int)time(NULL), host, status, message);
	else
		len = snprintf(command, sizeof(command) - 1,
		    "[%u] PROCESS_SERVICE_CHECK_RESULT;%s;%s;%d;%s",
		    (unsigned int)time(NULL), host, service, status, message);

	command[len++] = '\n';

	snprintf(cmd, sizeof(cmd), "PUSH %d\n", len);
	if (!nscang_client_write(c, cmd, strlen(cmd), timeout))
		return 0;

	rc = nscang_client_response(c, timeout);

	if (!rc)
		return 0;

	if (rc != NSCANG_RESP_OKAY) {
		c->_errno = NSCANG_ERROR_PROTOCOL_MISMATCH;
		return 0;
	}

	if (!nscang_client_write(c, command, len, timeout))
		return 0;

	rc = nscang_client_response(c, timeout);

	if (!rc)
		return 0;

	if (rc != NSCANG_RESP_OKAY) {
		c->_errno = NSCANG_ERROR_PROTOCOL_MISMATCH;
		return 0;
	}

	return 1;
}

int
nscang_client_send_quit(nscang_client_t *c)
{
	if (c->state != NSCANG_STATE_MOIN) {
		c->_errno = NSCANG_ERROR_BAD_STATE;
		return 0;
	}

	if (!nscang_client_write(c, "QUIT\n", 5, 0))
		return 0;

	return 1;
}

char *
nscang_client_errstr(nscang_client_t *c, char *buf, int buf_size)
{
	switch (c->_errno) {
	case NSCANG_ERROR_SSL_CTX_CREATE:
		strncpy(buf, "Can't create SSL context", buf_size);
		break;
	case NSCANG_ERROR_SSL_CIPHERS:
		strncpy(buf, "Bad ciphers list", buf_size);
		break;
	case NSCANG_ERROR_SSL_BIO_CREATE:
		strncpy(buf, "Can't create BIO socket", buf_size);
		break;
	case NSCANG_ERROR_SSL_CREATE:
		strncpy(buf, "Can't create SSL", buf_size);
		break;
	case NSCANG_ERROR_SSL:
		snprintf(buf, buf_size, "SSL error - %s - %s",
		    ERR_reason_error_string(ERR_peek_last_error()),
		    ERR_reason_error_string(ERR_peek_error()));
		break;
	case NSCANG_ERROR_MALLOC:
		strncpy(buf, "Can't allocate memory", buf_size);
		break;
	case NSCANG_ERROR_TIMEOUT:
		strncpy(buf, "Timeout was reached", buf_size);
		break;
	case NSCANG_ERROR_TOO_LONG_RESPONSE:
		strncpy(buf, "Protocol mismatch - too long response", buf_size);
		break;
	case NSCANG_ERROR_BAD_PROTO_VERSION:
		snprintf(buf, buf_size, "Protocol mismatch - bad version '%s'",
		    c->errstr);
		break;
	case NSCANG_ERROR_PROTOCOL_MISMATCH:
		strncpy(buf, "Protocol mismatch - unexpected server response",
		    buf_size);
		break;
	case NSCANG_ERROR_UNKNOWN_RESPONSE:
		strncpy(buf, "Protocol mismatch - unknown server response",
		    buf_size);
		break;
	case NSCANG_ERROR_BAIL:
		snprintf(buf, buf_size, "BAIL: %s", c->errstr);
		break;
	case NSCANG_ERROR_FAIL:
		snprintf(buf, buf_size, "FAIL: %s", c->errstr);
		break;
	case NSCANG_ERROR_BAD_STATE:
		snprintf(buf, buf_size, "Operation not permitted in state %d",
		    c->state);
		break;
	case NSCANG_ERROR_LOCKING:
		strncpy(buf, "Can't obtain lock for instances list", buf_size);
		break;
	default:
		buf[0] = 0x00;
	}
	buf[buf_size - 1] = 0x00;
	return buf;
}
