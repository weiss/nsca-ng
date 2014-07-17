#ifndef __NSCANG_CLIENT_H
#define __NSCANG_CLIENT_H

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "uthash.h"

typedef struct {
  SSL_CTX *ssl_ctx;
  BIO     *bio;
  SSL     *ssl;
  int     state;

  char *identity;
  char *psk;

  int _errno;
  char errstr[1024];

  UT_hash_handle hh;
} nscang_client_t;


#define NSCANG_STATE_NONE 0
#define NSCANG_STATE_NEW  1
#define NSCANG_STATE_MOIN 2

#define NSCANG_RESP_MOIN 1
#define NSCANG_RESP_OKAY 2

#define NSCANG_ERROR_MALLOC             1
#define NSCANG_ERROR_TIMEOUT            2
#define NSCANG_ERROR_TOO_LONG_RESPONSE  3
#define NSCANG_ERROR_BAD_PROTO_VERSION  4
#define NSCANG_ERROR_PROTOCOL_MISMATCH  5
#define NSCANG_ERROR_UNKNOWN_RESPONSE   6
#define NSCANG_ERROR_BAIL               7
#define NSCANG_ERROR_FAIL               8
#define NSCANG_ERROR_BAD_STATE          9
#define NSCANG_ERROR_LOCKING            10

#define NSCANG_ERROR_SSL_CTX_CREATE     101
#define NSCANG_ERROR_SSL_CIPHERS        102
#define NSCANG_ERROR_SSL_BIO_CREATE     103
#define NSCANG_ERROR_SSL_CREATE         104
#define NSCANG_ERROR_SSL                105


int nscang_client_init( nscang_client_t *c, char *host, int port,
                        char *ciphers, char *identity, char *psk);
void nscang_client_free( nscang_client_t *c);

void nscang_client_disconnect( nscang_client_t *c);

int nscang_client_send_moin( nscang_client_t *c, int timeout);
int nscang_client_send_push( nscang_client_t *c, char *host, char *service,
                             int status, char *message, int timeout);
int nscang_client_send_quit( nscang_client_t *c);


char * nscang_client_errstr( nscang_client_t *c, char *buf, int buf_size);

#endif /* __NSCANG_CLIENT_H */

