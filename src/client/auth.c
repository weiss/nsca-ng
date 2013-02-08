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

#include <string.h>

#include <openssl/ssl.h>

#include "auth.h"
#include "conf.h"
#include "send_nsca.h"
#include "system.h"

unsigned int
set_psk(SSL *ssl __attribute__((__unused__)),
        const char *hint __attribute__((__unused__)),
        char *identity,
        unsigned int max_identity_len,
        unsigned char *password,
        unsigned int max_password_len)
{
	char *configured_id = conf_getstr(cfg, "identity");
	char *configured_pw = conf_getstr(cfg, "password");
	size_t identity_len = MIN(strlen(configured_id), max_identity_len - 1);
	size_t password_len = MIN(strlen(configured_pw), max_password_len);

	(void)memcpy(identity, configured_id, identity_len);
	(void)memcpy(password, configured_pw, password_len);

	identity[identity_len] = '\0';

	return (unsigned int)password_len;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
