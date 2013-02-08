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

# NSCA_FUNC_DAEMON
# ----------------
# Check the availability of a BSD-style daemon(3) function.  If no such function
# is found, arrange for using the bundled implementation.
AC_DEFUN([NSCA_FUNC_DAEMON],
[
  AC_CHECK_FUNC([daemon],
    [AC_DEFINE([HAVE_DAEMON], [1],
      [Define to 1 if you have the `daemon' function.])],
    [AC_LIBOBJ([daemon])
     AC_CHECK_HEADERS([paths.h])
     AC_CHECK_DECL([_PATH_DEVNULL], [],
       [AC_DEFINE([_PATH_DEVNULL], ["/dev/null"],
         [Define to the path of the `null' device.])],
       [[#ifdef HAVE_PATHS_H
         #include <paths.h>
         #endif]])])
])# NSCA_FUNC_DAEMON

dnl vim:set joinspaces textwidth=80:
