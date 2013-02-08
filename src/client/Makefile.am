## Process this file with automake(1) to produce Makefile.in.

## Copyright (c) 2013 Holger Weiss <holger@weiss.in-berlin.de>
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##
## 1. Redistributions of source code must retain the above copyright notice,
##    this list of conditions and the following disclaimer.
##
## 2. Redistributions in binary form must reproduce the above copyright notice,
##    this list of conditions and the following disclaimer in the documentation
##    and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.

AM_CPPFLAGS =                           \
  -DSYSCONFDIR='"$(sysconfdir)"'        \
  -I$(top_srcdir)/src/common            \
  -I$(top_srcdir)/lib                   \
  $(EVCPPFLAGS)                         \
  $(SSLCPPFLAGS)

AM_LDFLAGS =                            \
  $(EVLDFLAGS)                          \
  $(SSLLDFLAGS)

LDADD =                                 \
  ../common/libcommon.a                 \
  ../../lib/libcompat.a                 \
  $(EVLIBS)                             \
  $(SSLLIBS)

if USE_EMBEDDED_EV
AM_CPPFLAGS += -I$(top_srcdir)/lib/ev
LDADD += ../../lib/ev/libev.a
endif

sbin_PROGRAMS = send_nsca
send_nsca_SOURCES = auth.c auth.h client.c client.h conf.c conf.h input.c \
                    input.h parse.c parse.h send_nsca.c send_nsca.h
