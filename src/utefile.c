/*** utefile.c -- high level interface to ute files (r/w)
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of sushi/uterus.
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

#define UTEFILE_C
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utefile-private.h"
#include "utefile.h"
#include "utehdr.h"
#include "utetpc.h"
#include "mem.h"

/* only tick size we support atm */
#include "sl1t.h"

#define SMALLEST_LVTD	(0x8000000 << 10)


/* aux */
static __attribute__((unused)) int
hdr_version(utehdr_t hdr)
{
	if (memcmp(hdr->magic, "UTE+", 4)) {
		return 0;
	}
	return (int)hdr->version[3];
}

static char*
mmap_any(int fd, int prot, int flags, off_t off, size_t len)
{
	int pgsz = sysconf(_SC_PAGESIZE);
	sidx_t ofp = off / pgsz, ofi = off % pgsz;
	char *p = mmap(NULL, len + ofi, prot, flags, fd, ofp * pgsz);
	return LIKELY(p != MAP_FAILED) ? p + ofi : NULL;
}

static void
munmap_any(char *map, off_t off, size_t len)
{
	int pgsz = sysconf(_SC_PAGESIZE);
	sidx_t ofi = off % pgsz;
	munmap(map - ofi, len + ofi);
	return;
}

static bool
__fwr_trunc(int fd, size_t sz)
{
	if ((fd < 0) || (ftruncate(fd, sz) < 0)) {
		return false;
	}
	return true;
}

static inline int
__pflags(utectx_t ctx)
{
	return PROT_READ | ((ctx->oflags & UO_RDWR) ? PROT_WRITE : 0);
}

static bool
ute_trunc(utectx_t ctx, size_t sz)
{
	if (!__fwr_trunc(ctx->fd, sz)) {
		return false;
	}
	ctx->fsz = sz;
	return true;
}

static bool
ute_extend(utectx_t ctx, ssize_t sz)
{
/* extend the ute file by SZ bytes */
	size_t tot = sz + ctx->fsz;
	if (!__fwr_trunc(ctx->fd, tot)) {
		return false;
	}
	ctx->fsz = tot;
	return true;
}

/* header caching */
static void
cache_hdr(utectx_t ctx)
{
	/* we just use max size here */
	size_t sz = sizeof(struct utehdr2_s);
	void *res;
	int pflags = __pflags(ctx);

	/* just map the first sizeof(struct bla) bytes */
	res = mmap(NULL, sz, pflags, MAP_SHARED, ctx->fd, 0);
	if (LIKELY(res != MAP_FAILED)) {
		/* assign the header */
		ctx->hdrp = res;
		ctx->slut_sz = ctx->hdrp->slut_sz;
	} else {
		ctx->hdrp = NULL;
		ctx->slut_sz = 0;
	}
	return;
}

static void
close_hdr(utectx_t ctx)
{
	/* munmap the header */
	if (ctx->hdrp != NULL) {
		munmap((void*)ctx->hdrp, sizeof(*ctx->hdrp));
	}
	ctx->hdrp = NULL;
	return;
}

static void
creat_hdr(utectx_t ctx)
{
	static const char stdhdr[] = "UTE+v0.1";
	size_t sz = sizeof(struct utehdr2_s);

	/* trunc to sz */
	ute_trunc(ctx, sz);
	/* cache the header */
	cache_hdr(ctx);
	/* set standard header payload offset, just to be sure it's sane */
	if (LIKELY(ctx->hdrp != NULL)) {
		memset((void*)ctx->hdrp, 0, sz);
		memcpy((void*)ctx->hdrp, stdhdr, 8);
	}
	/* file creation means new slut */
	ctx->slut_sz = 0;
	return;
}

/* seek reset */
void
flush_seek(uteseek_t sk)
{
	if (sk->mpsz > 0) {
		/* seek points to something => munmap first */
		munmap(sk->data, sk->mpsz);
	}
	sk->idx = -1;
	sk->mpsz = 0;
	sk->data = NULL;
	sk->tsz = 0;
	sk->page = -1U;
	return;
}

void
seek_page(uteseek_t sk, utectx_t ctx, uint32_t pg)
{
	size_t psz = page_size(ctx, pg);
	size_t off = page_offset(ctx, pg);
	void *tmp;
	int pflags = __pflags(ctx);

	/* trivial checks */
	if (UNLIKELY(off >= ctx->fsz)) {
		return;
	}
	/* create a new seek */
	tmp = mmap(NULL, psz, pflags, MAP_SHARED, ctx->fd, off);
	if (UNLIKELY(tmp == MAP_FAILED)) {
		return;
	}
	sk->data = tmp;
	sk->idx = 0;
	sk->mpsz = psz;
	sk->tsz = sizeof(struct sl1t_s);
	sk->page = pg;
	return;
}

static void
reseek(utectx_t ctx, sidx_t i)
{
	uint32_t p = page_of_index(ctx, i);
	uint32_t o = offset_of_index(ctx, i);

	/* flush the old seek */
	flush_seek(ctx->seek);
	/* create a new seek */
	seek_page(ctx->seek, ctx, p);
	ctx->seek->idx = o;
	return;
}

/* seek to */
scom_t
ute_seek(utectx_t ctx, sidx_t i)
{
	uint32_t o;

	/* wishful thinking */
	if (UNLIKELY(index_past_eof_p(ctx, i))) {
		sidx_t new_i = index_to_tpc_index(ctx, i);
		return tpc_get_scom(ctx->tpc, new_i);
	} else if (UNLIKELY(!index_in_seek_page_p(ctx, i))) {
		reseek(ctx, i);
	}
	o = offset_of_index(ctx, i);
	return (scom_t)((char*)ctx->seek->data + o * ctx->seek->tsz);
}

static void
store_lvtd(utectx_t ctx)
{
	scom_t t;
	uint64_t sk;

	if (tpc_size(ctx->tpc) > 0) {
		t = tpc_last_scom(ctx->tpc);
		sk = tick_sortkey(t);
		ctx->lvtd = sk;
	} else {
		ctx->lvtd = SMALLEST_LVTD;
	}
	ctx->tpc->last = ctx->lvtd;
	ctx->tpc->lvtd = ctx->lvtd;
	return;
}

static void
store_slut(utectx_t ctx)
{
	struct utehdr2_s *h = (void*)ctx->hdrp;
	h->slut_sz = ctx->slut_sz;
	h->slut_nsyms = (uint16_t)ctx->slut->nsyms;
	return;
}

#define PROT_FLUSH	(PROT_READ | PROT_WRITE)
#define MAP_FLUSH	(MAP_SHARED)

static void
flush_tpc(utectx_t ctx)
{
	void *p;
	size_t sz = tpc_size(ctx->tpc);
	sidx_t off = ctx->fsz; 

	/* extend to take SZ additional bytes */
	if (ctx->oflags == UO_RDONLY || !ute_extend(ctx, sz)) {
		return;
	}
	p = mmap(NULL, sz, PROT_FLUSH, MAP_FLUSH, ctx->fd, off);
	if (p == MAP_FAILED) {
		return;
	}
	memcpy(p, ctx->tpc->tp, sz);
	munmap(p, sz);

	/* store the largest-value-to-date */
	store_lvtd(ctx);

	/* munmap the tpc? */
	clear_tpc(ctx->tpc);
	return;
}

static void
flush_slut(utectx_t ctx)
{
	char *p;
	void *stbl = NULL;
	size_t stsz = 0;
	sidx_t off = ctx->fsz;

	/* dont try at all in read-only mode */
	if (UNLIKELY(ctx->oflags == UO_RDONLY)) {
		return;
	}

	/* first of all serialise into memory */
	slut_seria(ctx->slut, &stbl, &stsz);

	if (UNLIKELY(stbl == NULL)) {
		return;
	} else if (UNLIKELY(stsz == 0)) {
		goto out;
	}
	/* extend to take STSZ additional bytes */
	if (!ute_extend(ctx, stsz)) {
		goto out;
	}
	/* align to multiples of page size */
	if ((p = mmap_any(ctx->fd, PROT_FLUSH, MAP_FLUSH, off, stsz)) == NULL) {
		goto out;
	}
	memcpy(p, stbl, stsz);
	munmap_any(p, off, stsz);

	/* store the size of the serialised slut */
	ctx->slut_sz = stsz;
	store_slut(ctx);

out:
	free(stbl);
	return;
}

static void
load_tpc(utectx_t ctx)
{
/* take the last page in CTX and make a tpc from it, trunc the file
 * accordingly, this is in a way a reverse flush_tpc() */
	size_t lpg = ute_npages(ctx);
	struct uteseek_s sk[1];

	if (UNLIKELY(lpg == 0)) {
		return;
	}

	/* seek to the last page */
	seek_page(sk, ctx, lpg - 1);
	/* create the tpc space */
	if (!tpc_active_p(ctx->tpc)) {
		make_tpc(ctx->tpc, UTE_BLKSZ(ctx), sizeof(struct sl1t_s));
	}
	/* copy the last page */
	memcpy(ctx->tpc->tp, sk->data, sk->mpsz);
	/* ... and set the new length */
	ctx->tpc->tidx = sk->mpsz;
	/* now munmap the seek */
	flush_seek(sk);
	/* also set the last and lvtd values */
	/* store the largest-value-to-date */
	store_lvtd(ctx);

	/* real shrinking was to dangerous without C-c handler,
	 * make fsz a multiple of page size */
	ctx->fsz -= tpc_size(ctx->tpc) + ctx->slut_sz;
	return;
}

static char*
mmap_slut(utectx_t ctx)
{
	size_t off = ctx->fsz - ctx->slut_sz;
	int pflags = __pflags(ctx);
	return mmap_any(ctx->fd, pflags, MAP_FLUSH, off, ctx->slut_sz);
}

static void
munmap_slut(utectx_t ctx, char *map)
{
	size_t off = ctx->fsz - ctx->slut_sz;
	munmap_any(map, off, ctx->slut_sz);
	return;
}

static void
load_slut(utectx_t ctx)
{
/* take the stuff past the last page in CTX and make a slut from it */
	char *slut;

	if (UNLIKELY(ctx->fsz == 0)) {
		return;
	}

	/* otherwise leap to behind the last tick and
	 * deserialise the look-up table */
	if ((slut = mmap_slut(ctx)) != NULL) {
		slut_deser(ctx->slut, slut, ctx->slut_sz);
		munmap_slut(ctx, slut);
	}

	/* real shrink is too dangerous, just adapt fsz instead */
	ctx->fsz -= ctx->slut_sz, ctx->slut_sz = 0;
	return;
}

static void
ute_init(utectx_t ctx)
{
	ctx->pgsz = (size_t)sysconf(_SC_PAGESIZE);
	flush_seek(ctx->seek);
	/* yikes, this is a bit confusing, we use the free here to
	 * initialise the tpc */
	free_tpc(ctx->tpc);
	return;
}

static void
ute_fini(utectx_t ctx)
{
	flush_seek(ctx->seek);
	free_tpc(ctx->tpc);
	free(ctx->fname);
	return;
}

/* utectx dup */
static utectx_t
ute_dup(utectx_t ctx)
{
	utectx_t tmp = xnew(*ctx);
	*tmp = *ctx;
	return tmp;
}


/* primitives to kick shit off */
utectx_t
ute_open(const char *path, int oflags)
{
	struct utectx_s res[1];
	struct stat st;

	/* rinse rinse rinse */
	memset(res, 0, sizeof(*res));

	/* massage the flags */
	if (UNLIKELY((oflags & UO_WRONLY))) {
		/* stupid nutters, we allow read/write, not write-only */
		oflags = (oflags & ~UO_WRONLY) | UO_RDWR;
	}
	/* we need to open the file RDWR at the moment, various
	 * mmap()s use PROT_WRITE */
	if (oflags != UO_RDONLY) {
		oflags |= UO_RDWR;
	}
	/* store the access flags */
	res->oflags = (uint16_t)oflags;

	/* open the file first */
	if ((res->fd = open(path, oflags, 0644)) < 0) {
		/* ooooh, leave with a big bang */
		return NULL;
	}
	fstat(res->fd, &st);
	if ((res->fsz = st.st_size) <= 0 && !(oflags & UO_CREAT)) {
		/* user didn't request creation, so fuck off here */
		return NULL;
	} else if ((oflags & UO_TRUNC) ||
		   (res->fsz <= 0 && (oflags & UO_CREAT))) {
		/* user requested truncation, or creation */
		creat_hdr(res);
	} else if (res->fsz > 0) {
		/* no truncation requested and file size is not 0 */
		cache_hdr(res);
	}
	/* save a copy of the file name */
	res->fname = strdup(path);
	/* initialise the rest */
	ute_init(res);
	/* initialise the tpc session */
	init_tpc();
	/* initialise the slut */
	init_slut();

	if ((oflags & UO_TRUNC) || res->fsz == 0) {
		/* set the largest-value to-date, which is pretty small */
		res->lvtd = SMALLEST_LVTD;
		make_slut(res->slut);
	} else {
		/* load the slut, must be first, as we need to shrink
		 * the file accordingly */
		load_slut(res);
		/* load the last page as tpc */
		load_tpc(res);
	}
	return ute_dup(res);
}

static void
ute_prep_sort(utectx_t ctx)
{
	/* delete the file */
	unlink(ctx->fname);
	return;
}

void
ute_close(utectx_t ctx)
{
	/* first make sure we write the stuff */
	ute_flush(ctx);
	if (!ute_sorted_p(ctx)) {
		ute_prep_sort(ctx);
		ute_sort(ctx);
	}
	/* serialse the slut */
	flush_slut(ctx);

	/* finish our tpc session */
	fini_tpc();
	/* finish our slut session */
	fini_slut();
	/* now proceed to closing and finalising */
	close_hdr(ctx);
	ute_fini(ctx);
	close(ctx->fd);
	return;
}

void
ute_flush(utectx_t ctx)
{
	if (tpc_active_p(ctx->tpc)) {
		/* also sort and diskify the currently active tpc */
		if (!tpc_sorted_p(ctx->tpc)) {
			tpc_sort(ctx->tpc);
		}
		if (tpc_needmrg_p(ctx->tpc)) {
			/* special case when the page cache has detected
			 * a major violation */
			ute_set_unsorted(ctx);
		}
		flush_tpc(ctx);
	}
	return;
}



/* accessor */
void
ute_add_tick(utectx_t ctx, scom_t t)
{
/* the big question here is if we want to allow arbitrary ticks as in
 * can T be of type scdl too? */
	if (!tpc_active_p(ctx->tpc)) {
		make_tpc(ctx->tpc, UTE_BLKSZ(ctx), sizeof(struct sl1t_s));
	} else if (tpc_full_p(ctx->tpc)) {
		/* oh current tpc is full, flush and start over */
		ute_flush(ctx);
	}
	/* we sort the tick question for now by passing on the size of T */
	tpc_add_tick(ctx->tpc, t, scom_thdr_size(t));
	return;
}

size_t
ute_nticks(utectx_t ctx)
{
/* for the moment just use the file size and number of pages
 * plus whats in the tpc */
	size_t aux_sz = sizeof(struct utehdr2_s) + ctx->slut_sz;
	size_t nticks = (ctx->fsz - aux_sz) / sizeof(struct sl1t_s);
	/* if there are non-flushed ticks, consider them */
	if (tpc_active_p(ctx->tpc)) {
		nticks += tpc_nticks(ctx->tpc);
	}
	return nticks;
}

size_t
ute_nsyms(utectx_t ctx)
{
/* return the number of symbols tracked in the ute file */
	size_t nsyms = ctx->hdrp->slut_nsyms;
	if (UNLIKELY(nsyms == 0)) {
		/* try the slut itself */
		nsyms = ctx->slut->nsyms;
	}
	return nsyms;
}

/* quick note one the actual cow stuff in ute:
 * - we keep ticks in pages, each 1024 sys pages wide.
 * - each tick page is ordered
 * - in the final file tick pages will be written so
 *   theyre ordered altogether
 * - there are scribble pages */


/* accessors */
size_t
ute_tick(utectx_t ctx, scom_t *tgt, sidx_t i)
{
	scom_t p = ute_seek(ctx, i);
	if (LIKELY(p != NULL)) {
		*tgt = p;
		return ctx->seek->tsz;
	}
	return 0;
}

size_t
ute_tick2(utectx_t ctx, void *tgt, size_t UNUSED(tsz), sidx_t i)
{
	scom_t p = ute_seek(ctx, i);
	if (LIKELY(p != NULL)) {
		memcpy(tgt, p, ctx->seek->tsz);
		return ctx->seek->tsz;
	}
	return 0;
}

/* slut accessors */
uint16_t
ute_sym2idx(utectx_t ctx, const char *sym)
{
	if (UNLIKELY(sym == NULL)) {
		abort();
	}
	return slut_sym2idx(ctx->slut, sym);
}

const char*
ute_idx2sym(utectx_t ctx, uint16_t idx)
{
	return slut_idx2sym(ctx->slut, idx);
}

/* utefile.c ends here */
