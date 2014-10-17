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

EXTRA_DIST = nsca-ng.cfg nsca-ng.service nsca-ng.socket send_nsca.cfg \
             valgrind.supp

if BUILD_CLIENT
install-data-local: install-nsca-client-conf
uninstall-local: uninstall-nsca-client-conf
endif

if BUILD_SERVER
install-data-local: install-nsca-server-conf
uninstall-local: uninstall-nsca-server-conf
endif

# We use "test -r" because Solaris 10 doesn't provide "test -e".

install-nsca-client-conf:
	$(AM_V_at)$(MKDIR_P) '$(DESTDIR)$(sysconfdir)'
	$(AM_V_at)if [ -r '$(DESTDIR)$(sysconfdir)/send_nsca.cfg' ]; then \
	    echo 'Preserving the installed send_nsca.cfg file.'; \
	else \
	    $(INSTALL_DATA) '$(srcdir)/send_nsca.cfg' \
	      '$(DESTDIR)$(sysconfdir)'; \
	fi

install-nsca-server-conf:
	$(AM_V_at)$(MKDIR_P) '$(DESTDIR)$(sysconfdir)'
	$(AM_V_at)if [ -r '$(DESTDIR)$(sysconfdir)/nsca-ng.cfg' ]; then \
	    echo 'Preserving the installed nsca-ng.cfg file.'; \
	else \
	    $(INSTALL_DATA) '$(srcdir)/nsca-ng.cfg' \
	      '$(DESTDIR)$(sysconfdir)'; \
	fi

uninstall-nsca-client-conf:
	$(AM_V_at)if cmp -s '$(srcdir)/send_nsca.cfg' \
	  '$(DESTDIR)$(sysconfdir)/send_nsca.cfg'; then \
	    rm -f '$(DESTDIR)$(sysconfdir)/send_nsca.cfg'; \
	else \
	    echo 'Preserving the installed send_nsca.cfg file.'; \
	fi

uninstall-nsca-server-conf:
	$(AM_V_at)if cmp -s '$(srcdir)/nsca-ng.cfg' \
	  '$(DESTDIR)$(sysconfdir)/nsca-ng.cfg'; then \
	    rm -f '$(DESTDIR)$(sysconfdir)/nsca-ng.cfg'; \
	else \
	    echo 'Preserving the installed nsca-ng.cfg file.'; \
	fi
