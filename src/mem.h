/*** mem.h -- allocators and memory management
 *
 * Copyright (C) 2008-2015 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of uterus.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#if !defined INCLUDED_mem_h_
#define INCLUDED_mem_h_

#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#if defined SUSHI_MEM_DEBUG
extern size_t memusa;
# define xmalloc(_x)		__xmalloc(__FILE__, __LINE__, _x)
# define xrealloc(_p, _x)	__xrealloc(__FILE__, __LINE__, _p, _x)
# define xfree(_x)		__xfree(__FILE__, __LINE__, _x)

static inline void*
__xmalloc(const char *file, int line, size_t sz)
{
	void *res = malloc(sz);
	memusa += sz;
	fprintf(stderr, "mall %lu (%s:%i) => %p ... total %lu\n",
		sz, file, line, res, memusa);
	return res;
}

static inline void*
__xrealloc(const char *file, int line, void *p, size_t sz)
{
	void *res = realloc(p, sz);
	/* not correct but okay */
	memusa += sz;
	fprintf(stderr, "real %lu (%s:%i) => %p ... total %lu\n",
		sz, file, line, res, memusa);
	return res;
}

static inline void
__xfree(const char *file, int line, void *p)
{
	fprintf(stderr, "free %p (%s:%i) ... total %lu\n",
		p, file, line, memusa);
	/* compute me */
	memusa -= 0;
	free(p);
	return;
}

#else  /* !SUSHI_MEM_DEBUG */

#define xmalloc		malloc
#define xrealloc	xrealloc
#define xfree		free

#endif	/* SUSHI_MEM_DEBUG */

#define xnew(_x)	xmalloc(sizeof(_x))
#define xnew_a(_x, _s)	xmalloc(sizeof(_x) * (_s))
#define xnew_array	xnew_a
#define xresz_a(_x, _s)	xrealloc(_x, sizeof(*(_x)) * (_s))

static inline void*
snodup(const void *in)
{
	union {
		const void *c;
		void *p;
	} res = {
		.c = in,
	};
	return res.p;
}

#if defined MAP_FAILED
/* ah, sys/mman.h was inc'd */
# if !defined MAP_ANONYMOUS && defined MAP_ANON
#  define MAP_ANONYMOUS		(MAP_ANON)
# elif !defined MAP_ANON
#  define MAP_ANON		(0x1000U)
# endif	/* MAP_ANON->MAP_ANONYMOUS */
# if !defined MAP_MEM
#  define MAP_MEM		(MAP_PRIVATE | MAP_ANON)
# endif	 /* !MAP_MEM */
# if !defined PROT_MEM
#  define PROT_MEM		(PROT_READ | PROT_WRITE)
# endif	 /* !PROT_MEM */

# if !defined MREMAP_MAYMOVE
# define MREMAP_MAYMOVE		1
static inline void*
mremap(void *old, size_t ol_sz, size_t nu_sz, int flags)
{
/* impl'd only for memory maps */
	void *new;

	if (ol_sz == nu_sz) {
		return old;
	} else if (!(flags & MREMAP_MAYMOVE)) {
		return MAP_FAILED;
	}
	new = mmap(NULL, nu_sz, PROT_MEM, MAP_MEM, -1, 0);
	if (new != MAP_FAILED) {
		const size_t cp_sz = ol_sz < nu_sz ? ol_sz : nu_sz;
		memcpy(new, old, cp_sz);
		munmap(old, ol_sz);
	}
	return new;
}
# endif	/* !MREMAP_MAYMOVE */
#endif	/* MAP_FAILED */

static inline bool
mmapablep(int fd)
{
	struct stat st;
	if (fd == STDIN_FILENO) {
		return false;
	} else if (fstat(fd, &st) < 0) {
		return false;
	} else if (st.st_size < 0) {
		return false;
	} else if (!S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
		return false;
	}
	return true;
}

#endif	/* INCLUDED_mem_h_ */
