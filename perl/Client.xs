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

struct nscang_object {
   nscang_client_t client;
   char *node_name;
   char *svc_description;
   int timeout;
};
typedef struct nscang_object nscang_object_t;

typedef nscang_object_t * Net__NSCAng__Client;

MODULE = Net::NSCAng::Client		PACKAGE = Net::NSCAng::Client		

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
      RETVAL->node_name = SVdup_OR_NULL(node_name);
      RETVAL->svc_description = SVdup_OR_NULL(svc_description);
      RETVAL->timeout = timeout;

      // Initialize the client library
      if(!nscang_client_init(
         &(RETVAL->client), host, port, SV_OR_NULL(ciphers), identity, psk
      )) {
	      char errstr[1024];
         nscang_client_free(&(RETVAL->client));
         croak("nscang_client_init: %s",
            nscang_client_errstr(&(RETVAL->client), errstr, sizeof(errstr))
         );
      }
   OUTPUT:
      RETVAL

void
DESTROY(self)
   Net::NSCAng::Client self
   CODE:
      if(self) {
	      nscang_client_free(&(self->client));
         if(self->node_name)
            free(self->node_name);
         if(self->svc_description)
            free(self->svc_description);
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

   if(SvOK(node_name)) cnode_name = SvPV_nolen(node_name);
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

	nscang_client_disconnect(client);

	if(nscang_client_send_push(client,
      cnode_name, csvc_description, return_code, plugin_output, self->timeout))
      XSRETURN_UNDEF;

   nscang_client_errstr(client, errstr, sizeof(errstr));
   RETVAL = newSVpv(errstr, 0);
   OUTPUT:
      RETVAL
