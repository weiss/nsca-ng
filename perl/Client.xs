// vim:set cindent:
#include <openssl/ssl.h>
#include <string.h>
#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "client.h"

#define SV_OR_DIE(var) { if(!SvOK(var)) croak("%s must not be undef", #var); }
#define SV_OR_NULL(var) (SvOK(var) ? SvPV_nolen(var) : NULL)
#define SVdup_OR_NULL(var) (SvOK(var) ? strdup(SvPV_nolen(var)) : NULL)
#define FREE_IF_SET(ptr) { if(ptr) free(ptr); }

struct nscang_object {
   nscang_client_t client;
   int client_initialized;
   char *host;
   char *identity;
   char *psk;
   char *ciphers;
   char *node_name;
   char *svc_description;
   int port;
   int timeout;
};
typedef struct nscang_object nscang_object_t;

typedef nscang_object_t * Net__NSCAng__Client;

static char *strdupnull(const char *s) {
   return NULL == s ? NULL : strdup(s);
}

#define DUP_OR_RETURN(attr) { \
   if(attr) { \
      if(!(o->attr = strdupnull(attr))) return 1; \
   } else { \
      o->attr = NULL; \
   }\
}
static int init_object(
      nscang_object_t *o,
      char *host, int port, char *identity, char *psk, char *ciphers,
      char *node_name, char * svc_description, int timeout) {
   o->timeout = timeout;
   o->port = port;
   DUP_OR_RETURN(host);
   DUP_OR_RETURN(identity);
   DUP_OR_RETURN(psk);
   DUP_OR_RETURN(ciphers);
   DUP_OR_RETURN(node_name);
   DUP_OR_RETURN(svc_description);
   return 0;
}

static void init_client(nscang_object_t *o)
{

   // Initialize the client library
   if(!nscang_client_init(&o->client, o->host, o->port, o->ciphers, o->identity, o->psk)) {
      char errstr[1024];

      nscang_client_free(&o->client);
      croak("nscang_client_init: %s",
            nscang_client_errstr(&o->client, errstr, sizeof(errstr))
           );
   }
   o->client_initialized = 1;
}

static void free_object(nscang_object_t *o) {
   FREE_IF_SET(o->svc_description);
   FREE_IF_SET(o->node_name);
   FREE_IF_SET(o->ciphers);
   FREE_IF_SET(o->psk);
   FREE_IF_SET(o->identity);
   FREE_IF_SET(o->host);
}

MODULE = Net::NSCAng::Client		PACKAGE = Net::NSCAng::Client

PROTOTYPES: DISABLE

BOOT:
	SSL_library_init();
	SSL_load_error_strings();

Net::NSCAng::Client
_new(class, host, port, identity, psk, ciphers, node_name, svc_description, timeout)
   char * class
   char * host
   int port
   char * identity
   char * psk
   SV * ciphers
   SV * node_name
   SV * svc_description
   int timeout

   CODE:
      // Alloc new object
      Newxz(RETVAL, 1, nscang_object_t);
      if(!RETVAL)
         croak("no memory for %s", class);

      // Copy/duplicate values to object attributes
      if(init_object(RETVAL,
               host, port,  identity, psk,
               SV_OR_NULL(ciphers), SV_OR_NULL(node_name),
               SV_OR_NULL(svc_description), timeout)
        ) {
         free_object(RETVAL);
         croak("no memory for object");
      }

      init_client(RETVAL);
   OUTPUT:
      RETVAL

void
DESTROY(self)
   Net::NSCAng::Client self
   CODE:
      if(self) {
	      nscang_client_free(&(self->client));
         free_object(self);
         Safefree(self);
      }

SV *
_result(self, is_host_result, node_name, svc_description, return_code, plugin_output)
   Net::NSCAng::Client self
   int is_host_result
   SV * node_name
   SV * svc_description
   int return_code
   char * plugin_output

   CODE:
   char *cnode_name = self->node_name;
   char *csvc_description = self->svc_description;
   char errstr[1024];
   nscang_client_t *client = &self->client;

   if(!self->client_initialized)
      init_client(self);

   if(SvOK(node_name))
      cnode_name = SvPV_nolen(node_name);

   if(is_host_result) {
      csvc_description = NULL;
   } else {
      if(SvOK(svc_description))
         csvc_description = SvPV_nolen(svc_description);
   }

   if(!(is_host_result || csvc_description))
      croak("svc_description missing");

   if(!cnode_name)
      croak("node_name missing");

	if(nscang_client_send_push(client,
      cnode_name, csvc_description, return_code, plugin_output, self->timeout))
      XSRETURN_UNDEF;

   if(client->_errno == NSCANG_ERROR_TIMEOUT) {
	   nscang_client_disconnect(client);

      if(nscang_client_send_push(client,
               cnode_name, csvc_description, return_code, plugin_output, self->timeout))
         XSRETURN_UNDEF;
   }

   // Retrieve the error description
   nscang_client_errstr(client, errstr, sizeof(errstr));
   // Remember to reinitialize the client text time through
   self->client_initialized = 0;
   nscang_client_free(&(self->client));

   RETVAL = newSVpv(errstr, 0);
   OUTPUT:
      RETVAL

SV *
command(self, command)
   Net::NSCAng::Client self
   SV * command

   CODE:
   char errstr[1024];
   char *ccommand;
   nscang_client_t *client = &self->client;

   if(!self->client_initialized)
      init_client(self);

   if(SvOK(command)) {
      ccommand = SvPV_nolen(command);
   } else {
      croak("command missing");
   }

	if(nscang_client_send_command(client, ccommand, self->timeout))
      XSRETURN_UNDEF;

   if(client->_errno == NSCANG_ERROR_TIMEOUT)
	   nscang_client_disconnect(client);

   // Retrieve the error description
   nscang_client_errstr(client, errstr, sizeof(errstr));
   // Remember to reinitialize the client text time through
   self->client_initialized = 0;
   nscang_client_free(&(self->client));

   croak(errstr);
