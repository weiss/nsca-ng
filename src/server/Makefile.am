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
  -DLOCALSTATEDIR='"$(localstatedir)"'  \
  -I$(top_srcdir)/src/common            \
  -I$(top_srcdir)/lib                   \
  $(CONFUSECPPFLAGS)                    \
  $(EVCPPFLAGS)                         \
  $(SSLCPPFLAGS)

AM_LDFLAGS =                            \
  $(CONFUSELDFLAGS)                     \
  $(EVLDFLAGS)                          \
  $(SSLLDFLAGS)

LDADD =                                 \
  ../common/libcommon.a                 \
  ../../lib/libcompat.a                 \
  $(CONFUSELIBS)                        \
  $(EVLIBS)                             \
  $(SSLLIBS)                            \
  $(AIOLIBS)                            \
  $(PIDFILELIBS)

if USE_EMBEDDED_EV
AM_CPPFLAGS += -I$(top_srcdir)/lib/ev
LDADD += ../../lib/ev/libev.a
endif

if USE_EMBEDDED_PIDFILE
AM_CPPFLAGS += -I$(top_srcdir)/lib/pidfile
LDADD += ../../lib/pidfile/libpidfile.a
endif

sbin_PROGRAMS = nsca-ng
nsca_ng_SOURCES = auth.c auth.h conf.c conf.h fifo.c fifo.h hash.c hash.h \
                  nsca-ng.c server.c server.h
