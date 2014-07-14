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

# NSCA_LIB_AIO
# ------------
# Check the availability of the POSIX AIO API, and of the aio_init(3) extension
# provided by glibc.  See for the latter:
#
# http://www.gnu.org/software/libc/manual/html_node/Configuration-of-AIO.html
#
# If the POSIX AIO API is available, we set $nsca_lib_posix_aio to "yes" and
# define HAVE_POSIX_AIO to 1.  We also set the output variable AIOLIBS.  Apart
# from that, we define symbols which describe the aio_init(3) extension, if
# available.
#
# We check whether POSIX AIO programs can actually be executed because at least
# on AIX 5.3, POSIX AIO is not enabled by default even though the header
# declarations and library functions are available.  When cross-compiling, we
# assume that POSIX AIO isn't available on the target system.  AC_CACHE_CHECK
# allows the user to override this assumption.
AC_DEFUN([NSCA_LIB_AIO],
[
  AC_CHECK_FUNC([aio_return],
    [nsca_lib_posix_aio=yes],
    [AC_CHECK_LIB([rt], [aio_return],
      [AIOLIBS='-lrt'
       nsca_lib_posix_aio=yes],
      [AC_CHECK_LIB([pthread], [aio_return],
        [AIOLIBS='-lrt -lpthread'
         nsca_lib_posix_aio=yes],
        [nsca_lib_posix_aio=no],
        [-lrt -lpthread])])])
  AS_IF([test "x$nsca_lib_posix_aio" = xyes],
    [AC_CACHE_CHECK([whether POSIX AIO works],
      [nsca_cv_lib_aio_enabled],
      [nsca_save_LIBS=$LIBS
       LIBS=$AIOLIBS
       AC_RUN_IFELSE(
         [AC_LANG_PROGRAM(
           [[#include <sys/types.h>
             #include <sys/stat.h>
             #include <fcntl.h>
             #include <aio.h>
             #include <signal.h>
             #include <string.h>
             #include <unistd.h>
             static void signal_handler(int sig, siginfo_t *info, void *ctx) {
               if (sig != SIGUSR1 || info->si_code != SI_ASYNCIO || ctx == NULL)
                 _exit(1);
               else {
                 (void)close(info->si_value.sival_int);
                 _exit(0);
               }
             }]],
           [[struct sigaction action;
             struct aiocb cb;
             char msg[] = "Hello, world!\n";
             int fd;
             action.sa_sigaction = signal_handler;
             action.sa_flags = SA_SIGINFO;
             (void)sigemptyset(&action.sa_mask);
             (void)sigaction(SIGUSR1, &action, NULL);
             if ((fd = open("/dev/null", O_WRONLY)) == -1)
               return 1;
             (void)memset(&cb, 0, sizeof(cb));
             cb.aio_buf = msg;
             cb.aio_nbytes = sizeof(msg);
             cb.aio_fildes = fd;
             cb.aio_offset = 0;
             cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
             cb.aio_sigevent.sigev_signo = SIGUSR1;
             cb.aio_sigevent.sigev_value.sival_int = fd;
             (void)aio_write(&cb);
             (void)sleep(10);
             (void)close(fd);
             return 1;]])],
         [nsca_cv_lib_aio_enabled=yes],
         [nsca_cv_lib_aio_enabled=no],
         [nsca_cv_lib_aio_enabled=no])
       LIBS=$nsca_save_LIBS])])
  AS_IF([test "x$nsca_cv_lib_aio_enabled" = xyes],
    [AC_DEFINE([HAVE_POSIX_AIO], [1],
      [Define to 1 if you have the POSIX AIO API.])
     nsca_save_LIBS=$LIBS
     LIBS=$AIOLIBS
     AC_CHECK_FUNCS([aio_init],
       [AC_CHECK_MEMBERS([struct aioinit.aio_threads,
         struct aioinit.aio_num,
         struct aioinit.aio_locks,
         struct aioinit.aio_usedba,
         struct aioinit.aio_debug,
         struct aioinit.aio_numusers,
         struct aioinit.aio_idle_time], [], [],
         [[#include <aio.h>]])])
     LIBS=$nsca_save_LIBS],
    [nsca_lib_posix_aio=no])
  AC_SUBST([AIOLIBS])
])# NSCA_LIB_AIO

dnl vim:set joinspaces textwidth=80:
