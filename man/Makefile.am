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

edit = sed \
  -e 's|[@]sysconfdir[@]|$(sysconfdir)|g' \
  -e 's|[@]localstatedir[@]|$(localstatedir)|g' \
  -e 's|[@]version[@]|$(PACKAGE_VERSION)|g' \
  -e 's|[@]date[@]|$(RELEASE_DATE)|g'

man8_MANS = nsca-ng.man send_nsca.man
man5_MANS = nsca-ng.cfg.man send_nsca.cfg.man

PDF_FILES = nsca-ng.pdf nsca-ng.cfg.pdf send_nsca.pdf send_nsca.cfg.pdf
EXTRA_DIST = nsca-ng.in send_nsca.in nsca-ng.cfg.in send_nsca.cfg.in
CLEANFILES = $(man8_MANS) $(man5_MANS) $(PDF_FILES)

.in.man:
	$(AM_V_GEN)$(edit) $< > $@

.man.ps:
	$(AM_V_GEN)$(top_srcdir)/build-aux/man2ps $< $@

.ps.pdf:
	$(AM_V_at)ps2pdf $< $@

pdf-local: $(PDF_FILES)
