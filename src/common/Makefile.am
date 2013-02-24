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

GIT_DESCRIBE = git describe --tags --match 'v[0-9]*' --dirty 2>/dev/null
NSCA_VERSION = `($(GIT_DESCRIBE) || echo $(PACKAGE_VERSION)) | sed 's/^v//'`

AM_CPPFLAGS =                           \
  -DNSCA_VERSION=\"$(NSCA_VERSION)\"    \
  -I$(top_srcdir)/lib                   \
  $(EVCPPFLAGS)                         \
  $(SSLCPPFLAGS)

AM_LDFLAGS =                            \
  $(EVLDFLAGS)                          \
  $(SSLLDFLAGS)

LDADD =                                 \
  ../../lib/libcompat.a                 \
  $(EVLIBS)                             \
  $(SSLLIBS)

if USE_EMBEDDED_EV
AM_CPPFLAGS += -I$(top_srcdir)/lib/ev
LDADD += ../../lib/ev/libev.a
endif

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = buffer.c buffer.h log.c log.h tls.c tls.h util.c util.h
