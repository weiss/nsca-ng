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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "log.h"
#include "system.h"
#include "wrappers.h"

#define BUFFER_BLOCK_SIZE 128

/*
 * If the block_detach() function is used, `data' must be the first member of
 * the `block' struct, so that it can be free(3)d.
 */
typedef struct block_s {
	unsigned char data[BUFFER_BLOCK_SIZE];
	struct block_s *next;
} block;

struct buffer_s {
	block *first;
	block *last;
	size_t start_offset;
	size_t end_offset;
	size_t size;
};

static size_t buffer_find_char(buffer *, char);
static void block_add(buffer *);
static void block_remove(buffer *);
static void *block_detach(buffer *, size_t);

/*
 * Exported functions.
 */

buffer *
buffer_new(void)
{
	buffer *new = xmalloc(sizeof(buffer));

	debug("Creating buffer (address: %p)", (void *)new);

	new->first = NULL;
	new->last = NULL;
	new->start_offset = 0;
	new->end_offset = 0;
	new->size = 0;

	return new;
}

void
buffer_append(buffer * restrict buf, const void * restrict input, size_t size)
{
	const unsigned char *in = input;
	size_t n, n_todo;

	debug("Writing %zu bytes to buffer (address: %p)", size, (void *)buf);

	for (n_todo = size; n_todo > 0; n_todo -= n, in += n) {
		size_t available = buf->last == NULL ?
		    0 : BUFFER_BLOCK_SIZE - buf->end_offset;

		if (available == 0) {
			block_add(buf);
			buf->end_offset = 0;
			available = BUFFER_BLOCK_SIZE;
		}
		n = MIN(n_todo, available);
		(void)memcpy(buf->last->data + buf->end_offset, in, n);
		buf->end_offset += n;
	}
	buf->size += size;
}

size_t
buffer_read(buffer * restrict buf, void * restrict output, size_t size)
{
	unsigned char *out = output;
	size_t n_total = MIN(size, buf->size);
	size_t n_todo, n;

	debug("Reading %zu bytes from buffer (address: %p)", size, (void *)buf);

	for (n_todo = n_total; n_todo > 0; n_todo -= n, out += n) {
		assert(buf->first != NULL);
		n = MIN(n_todo, BUFFER_BLOCK_SIZE - buf->start_offset);
		(void)memcpy(out, buf->first->data + buf->start_offset, n);
		buf->start_offset += n;

		if (buf->start_offset == BUFFER_BLOCK_SIZE) {
			block_remove(buf);
			buf->start_offset = 0;
		}
	}
	buf->size -= n_total;

	/*
	 * Removing the remaining block of an empty buffer increases the
	 * likelihood that buffer_detach() will succeed if the buffer is reused.
	 */
	if (buf->size == 0 && buf->first != NULL) {
		block_remove(buf);
		buf->start_offset = 0;
	}
	return n_total;
}

void *
buffer_read_alloc(buffer * restrict buf, size_t * restrict size)
{
	void *data;

	if ((data = block_detach(buf, *size)) == NULL) {
		data = xmalloc(*size);
		*size = buffer_read(buf, data, *size);
	}
	return data;
}

char *
buffer_read_line(buffer *buf)
{
	char *line;
	size_t size;

	if ((size = buffer_find_char(buf, '\n')) == 0)
		return NULL;

	line = buffer_read_alloc(buf, &size);
	if (size >= 2 && line[size - 2] == '\r')
		line[size - 2] = '\0';
	else
		line[size - 1] = '\0';

	return line;
}

char *
buffer_read_chunk(buffer *buf, char terminator)
{
	char *chunk;
	size_t size;

	if ((size = buffer_find_char(buf, terminator)) == 0)
		return NULL;

	chunk = buffer_read_alloc(buf, &size);
	chunk[size - 1] = '\0';

	return chunk;
}

void *
buffer_slurp(buffer * restrict buf, size_t * restrict size)
{
	*size = buf->size;

	return buffer_read_alloc(buf, size);
}

size_t
buffer_size(buffer *buf)
{
	return buf->size;
}

void
buffer_free(buffer *buf)
{
	block *blk = buf->first;

	debug("Destroying buffer (address: %p)", (void *)buf);

	while (blk != NULL) {
		block *next = blk->next;

		free(blk);
		blk = next;
	}
	free(buf);
}

/*
 * Static functions.
 */

static size_t
buffer_find_char(buffer *buf, char character)
{
	block *b;
	size_t pos = 1;

	for (b = buf->first; b != NULL; b = b->next) {
		size_t i = b == buf->first ? buf->start_offset : 0;
		size_t end_offset = b == buf->last ?
		    buf->end_offset : BUFFER_BLOCK_SIZE;

		while (i < end_offset) {
			if ((char)b->data[i] == character)
				return pos;
			i++, pos++;
		}
	}
	return 0;
}

static void
block_add(buffer *buf)
{
	block *blk = xmalloc(sizeof(block));

	debug("Adding buffer block (buffer: %p, block: %p)",
	    (void *)buf, (void *)blk);

	if (buf->first == NULL)
		buf->first = blk;
	else { /* The buffer isn't empty. */
		assert(buf->last != NULL);
		buf->last->next = blk;
	}

	buf->last = blk;
	buf->last->next = NULL;
}

static void
block_remove(buffer *buf)
{
	block *blk = buf->first;

	debug("Removing buffer block (buffer: %p, block: %p)",
	    (void *)buf, (void *)blk);

	assert(blk != NULL);
	buf->first = blk->next;
	free(blk);

	if (buf->first == NULL)
		buf->last = NULL;
}

static void *
block_detach(buffer *buf, size_t size)
{
	block *blk = buf->first;

	if (blk != NULL
	    && buf->first == buf->last
	    && buf->start_offset == 0
	    && buf->size == size) {
		debug("Detaching buffer block (buffer: %p, block: %p)",
		    (void *)buf, (void *)blk);
		buf->first = NULL;
		buf->last = NULL;
		buf->start_offset = 0;
		buf->end_offset = 0;
		buf->size = 0;
		return blk->data;
	} else {
		debug("Cannot detach buffer block (buffer: %p, block: %p)",
		    (void *)buf, (void *)blk);
		return NULL;
	}
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
