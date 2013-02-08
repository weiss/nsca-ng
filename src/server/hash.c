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

/*
 * NOTE: hdestroy(3) cannot be used in a sane and portable way.  On BSD,
 * hdestroy(3) calls free(3) on all keys; on Linux, it doesn't.  So, if we ever
 * need multiple hash tables, we should use a different implementation, such as
 * <http://uthash.sourceforge.net/>.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <search.h>

#include "hash.h"
#include "log.h"
#include "system.h"

void
hash_new(size_t size)
{
	debug("Creating hash table for %zu entries", size);

	if (hcreate(size) == 0)
		die("Cannot create hash table: %m");
}

void
hash_insert(const char * restrict key, void * restrict value)
{
	ENTRY e, *p;

	e.key = (char *)key;
	e.data = NULL; /* Initialize to make Clang's Static Analyzer happy. */

	/*
	 * Quoting the Linux man page:
	 *
	 * | SVr4 and POSIX.1-2001 specify that 'action' is significant only for
	 * | unsuccessful searches, so that an ENTER should not do anything for
	 * | a successful search.  In libc and glibc (before version 2.3), the
	 * | implementation violates the specification, updating the 'data' for
	 * | the given 'key' in this case.
	 *
	 * In order to replace existing entries on all systems, we update the
	 * value after searching for the entry.  (However, nsca-ng will never
	 * call this function with an existing key.)
	 */
	if ((p = hsearch(e, ENTER)) == NULL)
		die("Cannot insert `%s' into hash table: %m", key);
	p->data = value;
}

void *
hash_lookup(const char *key)
{
	ENTRY e, *p;

	e.key = (char *)key;
	e.data = NULL; /* Initialize to make Clang's Static Analyzer happy. */

	if ((p = hsearch(e, FIND)) == NULL)
		return NULL;
	else
		return p->data;
}

/* vim:set joinspaces noexpandtab textwidth=80 cinoptions=(4,u0: */
