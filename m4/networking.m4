# Copyright (c) 2013 Holger Weiss <holger@weiss.in-berlin.de>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# NSCA_LIB_NETWORKING
# -------------------
# Check the availability of some networking header files.  Also, find out which
# libraries define the networking functions we're interested in and add the
# according flags to LIBS.  We bail out if any of these functions aren't found.
AC_DEFUN([NSCA_LIB_NETWORKING],
[
  AC_CHECK_HEADERS([arpa/inet.h netinet/in.h sys/socket.h])
  # Solaris 8 and newer provide the inet_ntop(3) function in libnsl, older
  # Solaris versions provide inet_ntop(3) in libresolv instead.
  AC_SEARCH_LIBS([inet_ntop], [nsl resolv], [],
    [AC_MSG_FAILURE([cannot find the `inet_ntop' function])])
  # The libsocket library might or might not depend on libnsl.
  AC_SEARCH_LIBS([getpeername], [socket], [],
    [AC_CHECK_LIB([nsl], [getpeername],
      [LIBS="-lsocket -lnsl $LIBS"],
      [AC_MSG_FAILURE([cannot find the `getpeername' function])],
      [-lsocket -lnsl])])
])# NSCA_LIB_NETWORKING

dnl vim:set joinspaces textwidth=80:
