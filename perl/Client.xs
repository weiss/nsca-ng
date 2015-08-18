#include <openssl/ssl.h>
#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "client.h"

#define SV_OR_DIE(var) { if(!SvOK(var)) croak("%s must not be undef", #var); }

struct nscang_object {
   nscang_client_t client;
};
typedef struct nscang_object nscang_object_t;

typedef nscang_object_t * Net__NSCAng__Client;

MODULE = Net::NSCAng::Client		PACKAGE = Net::NSCAng::Client		

BOOT:
	SSL_library_init();
	SSL_load_error_strings();

Net::NSCAng::Client
_new(class, host, port, identity, psk, ciphers)
   char * class
   SV * host
   int port
   SV * identity
   SV * psk
   SV * ciphers

   PROTOTYPE: DISABLE
   CODE:
      Newxz(RETVAL, 1, nscang_object_t);
      if(!RETVAL)
         croak("no memory for %s", class);

      SV_OR_DIE(host);
      SV_OR_DIE(identity);
      SV_OR_DIE(psk);

      if(!nscang_client_init(
         &(RETVAL->client),
         SvOK(host) ? SvPV_nolen(host) : NULL,
         port,
         SvOK(ciphers) ? SvPV_nolen(ciphers) : NULL,
         SvPV_nolen(identity),
         SvPV_nolen(psk)
      )) {
	      char errstr[1024];
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
         Safefree(self);
      }

void
svc_result(self, host, svc_description, return_code, plugin_output, timeout)
   Net::NSCAng::Client self
   char * host
   char * svc_description
   int return_code
   char * plugin_output
   int timeout

   CODE:
      printf("a");

