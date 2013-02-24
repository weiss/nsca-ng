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

#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_POSIX_AIO
# include <aio.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ev.h>

#include "buffer.h"
#include "fifo.h"
#include "log.h"
#include "system.h"
#include "wrappers.h"

#define TIMEOUT 10.0

#if HAVE_POSIX_AIO
# ifdef SIGRTMIN
#  define SIG_AIO (SIGRTMIN + 1)
# else
#  define SIG_AIO SIGUSR1
# endif
#endif

#ifdef _AIX
# undef PIPE_BUF /* See <http://bugs.python.org/issue9862>. */
#endif

#ifndef PIPE_BUF
# define PIPE_BUF 512 /* POSIX guarantees PIPE_BUF >= 512. */
#endif

struct fifo_state_s { /* This is typedef'd to `fifo_state' in fifo.h. */
#if HAVE_POSIX_AIO
	struct aiocb async_cb;
	void (*free_aio_buf)(void *);
	ev_async async_watcher;
#else
	ev_idle idle_watcher;
	ev_timer timeout_watcher;
#endif
	ev_timer open_watcher;
	ev_io write_watcher;
	buffer *output_buffer;
	char *dump_file;
	const char *dump_dir;
	const char *path;
	unsigned char *output;
	size_t output_size;
	void (*free_output)(void *);
	size_t max_queue_size;
	int dump_fd;
	int fd;
};

static void open_cb(EV_P_ ev_timer *, int);
static void write_cb(EV_P_ ev_io *, int);
#if HAVE_POSIX_AIO
static void async_dump_data(fifo_state *);
static void async_signal_handler(int, siginfo_t *, void *);
static void async_cb(EV_P_ ev_async *, int);
#else
static void idle_cb(EV_P_ ev_idle *, int);
static void timeout_cb(EV_P_ ev_timer *, int);
static void sync_dump_data(fifo_state *);
#endif
static void dispatch_data(fifo_state *);
static char *make_process_file_command(fifo_state *);
static bool open_dump_file(fifo_state *);
static bool close_dump_file(fifo_state *);
static bool buffers_are_empty(fifo_state *);
static bool buffers_exceed_pipe_size(fifo_state *);
static void join_buffers(fifo_state *);
static void free_output(fifo_state *);

/*
 * Exported functions.
 */

fifo_state *
fifo_start(const char * restrict path, const char * restrict dump_dir,
           size_t max_queue_size)
{
	fifo_state *fifo = xmalloc(sizeof(fifo_state));
#if HAVE_POSIX_AIO
	struct sigaction action;
#endif

	debug("Starting command file writer");
#if HAVE_POSIX_AIO
	debug("Using the POSIX AIO API");

	action.sa_sigaction = async_signal_handler;
	action.sa_flags = SA_SIGINFO;
	(void)sigemptyset(&action.sa_mask);
	if (sigaction(SIG_AIO, &action, NULL) == -1)
		die("Cannot associate action with POSIX AIO signal: %m");

	fifo->free_aio_buf = NULL;
	fifo->async_watcher.data = fifo;
#else
	fifo->idle_watcher.data = fifo;
	fifo->timeout_watcher.data = fifo;
#endif
	fifo->open_watcher.data = fifo;
	fifo->write_watcher.data = fifo;
	fifo->output_buffer = buffer_new();
	fifo->dump_file = NULL;
	fifo->dump_dir = dump_dir;
	fifo->path = path;
	fifo->output = NULL;
	fifo->output_size = 0;
	fifo->free_output = free;
	fifo->max_queue_size = max_queue_size * 1024 * 1024;
	fifo->dump_fd = -1;
	fifo->fd = -1;

#if HAVE_POSIX_AIO
	ev_init(&fifo->async_watcher, async_cb);
#else
	ev_init(&fifo->idle_watcher, idle_cb);
	ev_init(&fifo->timeout_watcher, timeout_cb);
#endif
	ev_init(&fifo->open_watcher, open_cb);
	ev_init(&fifo->write_watcher, write_cb);

	ev_invoke(EV_DEFAULT_UC_ &fifo->open_watcher, 0);

	return fifo;
}

void
fifo_write(fifo_state * restrict fifo, void * restrict data, size_t size,
           void free_data(void *))
{
	if (buffers_are_empty(fifo) && free_data != NULL) {
		debug("Zero-copying %zu byte(s) to command file", size);
		fifo->output = data;
		fifo->output_size = size;
		fifo->free_output = free_data;
		dispatch_data(fifo);
	} else {
		if (fifo->max_queue_size > 0
		    && buffer_size(fifo->output_buffer) + size
		    > fifo->max_queue_size) {
			warning("Queued more than %zu MB, THROWING DATA AWAY",
			    fifo->max_queue_size / 1024 / 1024);
			buffer_free(fifo->output_buffer);
			fifo->output_buffer = buffer_new();
		}
		debug("Queueing %zu byte(s) for command file", size);
		buffer_append(fifo->output_buffer, data, size);
		dispatch_data(fifo);
		if (free_data != NULL)
			free_data(data);
	}
}

void
fifo_stop(fifo_state *fifo)
{
	debug("Stopping command file writer");

#if HAVE_POSIX_AIO
	if (ev_is_active(&fifo->async_watcher))
		ev_async_stop(EV_DEFAULT_UC_ &fifo->async_watcher);
#else
	if (ev_is_active(&fifo->idle_watcher))
		ev_idle_stop(EV_DEFAULT_UC_ &fifo->idle_watcher);
	if (ev_is_active(&fifo->timeout_watcher))
		ev_timer_stop(EV_DEFAULT_UC_ &fifo->timeout_watcher);
#endif
	if (ev_is_active(&fifo->open_watcher))
		ev_timer_stop(EV_DEFAULT_UC_ &fifo->open_watcher);
	if (ev_is_active(&fifo->write_watcher))
		ev_io_stop(EV_DEFAULT_UC_ &fifo->write_watcher);

	buffer_free(fifo->output_buffer);

	if (fifo->output != NULL)
		free(fifo->output);
	if (fifo->fd != -1)
		(void)close(fifo->fd);

	free(fifo);
}

/*
 * Static functions.
 */

static void
open_cb(EV_P_ ev_timer *w, int revents __attribute__((__unused__)))
{
	fifo_state *fifo = w->data;

	do
		fifo->fd = open(fifo->path, O_WRONLY | O_NONBLOCK);
	while (fifo->fd == -1 && errno == EINTR);

	if (fifo->fd == -1) {
		if (errno == ENXIO) /* Spit out a proper error message. */
			warning("No process is reading the command file");
		else
			warning("Cannot open %s: %m", fifo->path);

		ev_timer_set(&fifo->open_watcher, TIMEOUT, 0.0);
		ev_timer_start(EV_A_ w);
	} else {
		debug("Opened command file for writing");
		ev_io_set(&fifo->write_watcher, fifo->fd, EV_WRITE);
		if (!buffers_are_empty(fifo))
			dispatch_data(fifo);
	}
}

static void
write_cb(EV_P_ ev_io *w, int revents __attribute__((__unused__)))
{
	fifo_state *fifo = w->data;
	ssize_t n;

	do {
		if (fifo->output == NULL) {
			fifo->output = buffer_slurp(fifo->output_buffer,
			    &fifo->output_size);
			fifo->free_output = free;
		}
		if ((n = write(fifo->fd, fifo->output, fifo->output_size))
		    == -1)
			switch (errno) {
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
			case EWOULDBLOCK: /* Fall through. */
#endif
			case EAGAIN:
				debug("Writing to command file failed: %m");
				return;
			case EINTR:
				debug("Writing to command file failed: %m");
				continue;
			default:
				error("Cannot write to command file: %m");
				(void)close(fifo->fd);
				fifo->fd = -1;
				ev_io_stop(EV_A_ w);
				ev_timer_set(&fifo->open_watcher, TIMEOUT, 0.0);
				ev_timer_start(EV_A_ &fifo->open_watcher);
			}
		else {
			/*
			 * POSIX guarantees n == fifo->output_size, as we've
			 * written no more than PIPE_BUF bytes.  See the
			 * write(2) documentation.
			 */
			debug("Wrote %zd bytes to command file", n);
			free_output(fifo);
			if (buffer_size(fifo->output_buffer) == 0)
				ev_io_stop(EV_A_ w);
		}
	} while (n > 0 && buffer_size(fifo->output_buffer) > 0);
}

#if HAVE_POSIX_AIO

/*
 * We serialize our POSIX AIO operations (so that only a single write request is
 * queued at any given time).  This is done for the following reasons:
 *
 * - It allows us to support systems which don't provide realtime signals (such
 *   as NetBSD 6) without having to check the status of every request each time
 *   the AIO signal is received.
 *
 * - With realtime signals, there is the problem that the signal buffer might
 *   overflow, which means signals could be lost; and the POSIX API provides no
 *   notification of such overflow.  See:
 *
 *   http://aricwang.blogspot.de/2010/08/asynchronous-io-on-linux.html
 *
 * - With serialized I/O, there's no need to block the signal handler in the
 *   ev_async watcher callback (as suggested in the ev_async documentation of
 *   the ev(3) man page).
 *
 * - Parallelizing our disk I/O makes little sense anyway.
 */

static void
async_dump_data(fifo_state *fifo)
{
	join_buffers(fifo);

	if (!open_dump_file(fifo)) {
		free_output(fifo);
		return;
	}

	(void)memset(&fifo->async_cb, 0, sizeof(fifo->async_cb));
	fifo->free_aio_buf = fifo->free_output;
	fifo->async_cb.aio_buf = fifo->output;
	fifo->async_cb.aio_nbytes = fifo->output_size;
	fifo->async_cb.aio_fildes = fifo->dump_fd;
	fifo->async_cb.aio_offset = 0;
	fifo->async_cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	fifo->async_cb.aio_sigevent.sigev_signo = SIG_AIO;
	fifo->async_cb.aio_sigevent.sigev_value.sival_ptr = fifo;

	ev_async_start(EV_DEFAULT_UC_ &fifo->async_watcher);

	debug("Queuing %zu bytes for writing to %s", fifo->output_size,
	    fifo->dump_file);

	if (aio_write(&fifo->async_cb) == -1) {
		error("Cannot queue asynchrous write request: %m");
		ev_async_stop(EV_DEFAULT_UC_ &fifo->async_watcher);
		(void)close_dump_file(fifo);
		free_output(fifo);
		return;
	}

	/* The output buffer is now owned by fifo->async_cb.aio_buf. */
	fifo->output = NULL;
	fifo->output_size = 0;
}

static void
async_signal_handler(int signal_number __attribute__((__unused__)),
                     siginfo_t *signal_info,
                     void *context __attribute__((__unused__)))
{
	if (signal_info->si_code == SI_ASYNCIO) {
		fifo_state *fifo = signal_info->si_value.sival_ptr;

		ev_async_send(EV_DEFAULT_UC_ &fifo->async_watcher);
	}
}

static void
async_cb(EV_P_ ev_async *w, int revents __attribute__((__unused__)))
{
	fifo_state *fifo = w->data;
	ssize_t n;
	int result;

	debug("Handling POSIX AIO signal");

	ev_async_stop(EV_DEFAULT_UC_ &fifo->async_watcher);

	fifo->free_aio_buf((void *)fifo->async_cb.aio_buf);
	fifo->async_cb.aio_buf = NULL;

	if ((result = aio_error(&fifo->async_cb)) != 0)
		error("Cannot write to %s: %s", fifo->dump_file,
		    strerror(result));
	else if ((n = aio_return(&fifo->async_cb))
	    != (ssize_t)fifo->async_cb.aio_nbytes) /* Presumably an error. */
		error("Wrote %zd instead of %zu bytes to %s", n,
		    fifo->async_cb.aio_nbytes, fifo->dump_file);
	else {
		char *command = make_process_file_command(fifo);

		debug("Wrote %zd bytes to %s", n, fifo->dump_file);
		if (close_dump_file(fifo))
			fifo_write(fifo, command, strlen(command), free);
	}

	if (buffers_exceed_pipe_size(fifo))
		dispatch_data(fifo);
}

#else

static void
idle_cb(EV_P_ ev_idle *w, int revents __attribute__((__unused__)))
{
	fifo_state *fifo = w->data;

	ev_idle_stop(EV_A_ &fifo->idle_watcher);
	if (ev_is_active(&fifo->timeout_watcher))
		ev_timer_stop(EV_A_ &fifo->timeout_watcher);

	sync_dump_data(fifo);
}

static void
timeout_cb(EV_P_ ev_timer *w, int revents)
{
	fifo_state *fifo = w->data;

	debug("Idle watcher timed out");
	ev_invoke(EV_A_ &fifo->idle_watcher, revents);
}

static void
sync_dump_data(fifo_state *fifo)
{
	char *command;
	size_t offset;
	ssize_t n;

	join_buffers(fifo);

	if (!open_dump_file(fifo)) {
		free_output(fifo);
		return;
	}

	for (offset = 0; fifo->output_size > offset; offset += (size_t)n) {
		do
			n = write(fifo->dump_fd, fifo->output + offset,
			    fifo->output_size - offset);
		while (n == -1 && errno == EINTR);
		if (n == -1) {
			error("Cannot write to %s: %m", fifo->dump_file);
			(void)close_dump_file(fifo);
			free_output(fifo);
			return;
		}
		debug("Wrote %zu bytes to %s", fifo->output_size,
		    fifo->dump_file);
	}
	free_output(fifo);

	command = make_process_file_command(fifo);
	if (close_dump_file(fifo))
		fifo_write(fifo, command, strlen(command), free);
}

#endif

static void
dispatch_data(fifo_state *fifo)
{
	if (fifo->fd == -1)
		return; /* Queue the data if Nagios is down. */

	if (!buffers_exceed_pipe_size(fifo)) {
		if (!ev_is_active(&fifo->write_watcher)) {
			debug("Starting command file output watcher");
			ev_io_start(EV_DEFAULT_UC_ &fifo->write_watcher);

			/*
			 * Invoke write_cb() immediately to minimize the risk of
			 * exceeding the PIPE_BUF threshold.
			 */
			ev_invoke(EV_DEFAULT_UC_ &fifo->write_watcher,
			    EV_CUSTOM);
		} else
			debug("Command file output watcher is active");
	} else {
		if (ev_is_active(&fifo->write_watcher)) {
			debug("Stopping command file output watcher");
			ev_io_stop(EV_DEFAULT_UC_ &fifo->write_watcher);
		}
#if HAVE_POSIX_AIO
		if (!ev_is_active(&fifo->async_watcher)) {
			debug("Initiating asynchronous output request");
			async_dump_data(fifo);
		} else
			debug("Asynchronous output watcher is active");
#else
		if (!ev_is_active(&fifo->idle_watcher)) {
			debug("Starting idle and timer watchers");
			ev_timer_set(&fifo->timeout_watcher, TIMEOUT, 0.0);
			ev_timer_start(EV_DEFAULT_UC_ &fifo->timeout_watcher);
			ev_idle_start(EV_DEFAULT_UC_ &fifo->idle_watcher);
		} else
			debug("Idle and timer watchers are active");
#endif
	}
}

static char *
make_process_file_command(fifo_state *fifo)
{
	char *command;

	xasprintf(&command, "[%lu] PROCESS_FILE;%s;1\n",
	    (unsigned long)time(NULL), fifo->dump_file);

	return command;
}

static bool
open_dump_file(fifo_state *fifo)
{
	xasprintf(&fifo->dump_file, "%s/nsca.XXXXXX", fifo->dump_dir);
	if ((fifo->dump_fd = mkstemp(fifo->dump_file)) == -1) {
		error("Cannot create %s: %m", fifo->dump_file);
		free(fifo->dump_file);
		fifo->dump_file = NULL;
		return false;
	}
	return true;
}

static bool
close_dump_file(fifo_state *fifo)
{
	int result;

	do
		result = close(fifo->dump_fd);
	while (result == -1 && errno == EINTR);
	if (result == -1)
		error("Cannot close %s: %m", fifo->dump_file);

	free(fifo->dump_file);
	fifo->dump_file = NULL;
	return (bool)(result == 0);
}

static bool
buffers_are_empty(fifo_state *fifo)
{
	return (bool)(fifo->output == NULL
	    && buffer_size(fifo->output_buffer) == 0);
}

static bool
buffers_exceed_pipe_size(fifo_state *fifo)
{
	return (bool)(fifo->output_size > PIPE_BUF
	    || buffer_size(fifo->output_buffer) > PIPE_BUF);
}

static void
join_buffers(fifo_state *fifo)
{
	unsigned char *new_output;
	size_t new_size;

	if (buffer_size(fifo->output_buffer) == 0)
		return;

	new_size = fifo->output_size + buffer_size(fifo->output_buffer);
	new_output = xmalloc(new_size);

	debug("Joining output buffers (new size: %zu bytes)", new_size);

	if (fifo->output != NULL)
		(void)memcpy(new_output, fifo->output, fifo->output_size);

	(void)buffer_read(fifo->output_buffer,
	    new_output + fifo->output_size,
	    new_size - fifo->output_size);

	if (fifo->output != NULL)
		free_output(fifo);

	fifo->output = new_output;
	fifo->output_size = new_size;
	fifo->free_output = free;
}

static void
free_output(fifo_state *fifo)
{
	fifo->free_output(fifo->output);
	fifo->output = NULL;
	fifo->output_size = 0;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
