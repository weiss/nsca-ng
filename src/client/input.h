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

#ifndef INPUT_H
# define INPUT_H

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# include <ev.h>

# include "buffer.h"
# include "system.h"

typedef struct input_state_s {
/* public: */
	void *data; /* Can freely be used by the caller. */

/* private: */
	ev_io watcher;
	buffer *buffer;
	char input[128];
	void (*read_handler)(struct input_state_s * restrict, char * restrict);
	void (*eof_handler)(struct input_state_s *);
	int fd;
	char separator;
	bool eof;
} input_state;

input_state *input_start(char separator);
void input_read_chunk(input_state *,
                      void (*)(input_state * restrict, char * restrict));
void input_stop(input_state *);
void input_on_eof(input_state *, void (*)(input_state *));
void input_on_error(input_state *, void (*)(input_state *));

#endif

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
