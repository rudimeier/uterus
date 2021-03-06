/*** ute-anal.c -- simple analyses
 *
 * Copyright (C) 2013-2015 Sebastian Freundt
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

/** test client for libuterus */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include "utefile-private.h"
#include "utefile.h"
#include "scommon.h"
#include "sl1t.h"
#include "scdl.h"
#include "ssnp.h"
#include "mem.h"
#include "m30.h"
#if defined HAVE_PNG_H
/* for analysis pictures */
# include <png.h>
#endif	/* HAVE_PNG_H */

#include "cmd-aux.c"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

/* one day verbpr() might become --verbose */
#if defined DEBUG_FLAG
# include <assert.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# define UNUSED_dbg(x)		UNUSED(x)
# define UNUSED_nodbg(x)	x
#else
# define UDEBUG(args...)
# define UNUSED_dbg(x)		x
# define UNUSED_nodbg(x)	UNUSED(x)
#endif	/* DEBUG_FLAG */

typedef const struct anal_ctx_s *anal_ctx_t;

struct anal_ctx_s {
	int intv;
	int modu;
	utectx_t u;
};


/* the actual anal'ing */
/* we're anal'ing single scom's, much like a prf() in ute-print */
static struct anal_pot_s {
	double o;
	double c;
	time_t to;
	time_t tc;
	double lo;
	double hi;
	time_t tlo;
	time_t thi;
} pot;

static int
anal(anal_ctx_t UNUSED(ctx), scom_t ti)
{
	unsigned int ttf = scom_thdr_ttf(ti);
	time_t t;
	double p;

	switch (ttf) {
	case SL1T_TTF_BID:
	case SL1T_TTF_BIDASK:
		p = ffff_m30_d((m30_t){.v = AS_CONST_SL1T(ti)->v[0]});
		t = scom_thdr_sec(ti);
		break;
	default:
		return 0;
	}

	if (UNLIKELY(!pot.to)) {
		pot.o = p;
		pot.to = t;
	}
	if (p < pot.lo) {
		pot.lo = p;
		pot.tlo = t;
	}
	if (p > pot.hi) {
		pot.hi = p;
		pot.thi = t;
	}
	/* always go for the clo */
	pot.c = p;
	pot.tc = t;
	return 0;
}

static void
pr_pot(void)
{
	static char buf[64];
	enum {
		SHAPE__,
		SHAPE_N,
		SHAPE_M,
	} shape;
	char *bp = buf;

	if (pot.tlo < pot.thi) {
		shape = SHAPE_M;
	} else if (pot.tlo > pot.thi) {
		shape = SHAPE_N;
	} else {
		shape = SHAPE__;
	}

	switch (shape) {
	default:
	case SHAPE__:
		*bp++ = '-';
		break;
	case SHAPE_N:
		*bp++ = 'N';
		break;
	case SHAPE_M:
		*bp++ = 'M';
		break;
	}
	*bp++ = ',';

	bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f", pot.o);
	*bp++ = ',';
	switch (shape) {
	default:
	case SHAPE__:
		break;
	case SHAPE_N:
		bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f", pot.hi);
		*bp++ = ',';
		bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f", pot.lo);
		break;
	case SHAPE_M:
		bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f", pot.lo);
		*bp++ = ',';
		bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f", pot.hi);
		break;
	}
	*bp++ = ',';

	bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f", pot.c);
	*bp++ = ',';

	/* relative values now */
	double rel_co = pot.c / pot.o - 1.0;
	bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f%%", 100.0 * rel_co);
	*bp++ = ',';

	double rel_hl = pot.hi / pot.lo - 1.0;
	bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f%%", 100.0 * rel_hl);
	*bp++ = ',';

	double rel_co2 = (pot.c - pot.o) / (pot.c + pot.o);
	bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f%%", 100.0 * rel_co2);
	*bp++ = ',';

	double rel_hl2 = (pot.hi - pot.lo) / (pot.hi + pot.lo);
	bp += snprintf(bp, sizeof(buf) - (bp - buf), "%.4f%%", 100.0 * rel_hl2);
	*bp++ = ',';

	/* finalise buffer */
	*--bp = '\0';
	puts(buf);
	return;
}


#if defined HAVE_PNG_H
/* hold maps, they indicate the return when going long or short
 * at one point and close the position at another */
#define HMAP_WIDTH	(256U)
#define HMAP_FACTR	(4U)
#define MINI_WIDTH	(HMAP_WIDTH / HMAP_FACTR)

static struct {
	double bids[HMAP_WIDTH];
	double asks[HMAP_WIDTH];
	float hmap[HMAP_WIDTH * HMAP_WIDTH];
	size_t idx;
} hmap;

static png_structp pp;
static png_infop ip;

static void
rset_hmap(void)
{
	pp = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	ip = png_create_info_struct(pp);

	png_set_IHDR(
		pp, ip, HMAP_WIDTH, HMAP_WIDTH, 8U/*depth*/,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	memset(&hmap, -1, sizeof(hmap));
	return;
}

static void
fill_hmap(void)
{
	double rel_hl = pot.hi / pot.lo - 1.0;

	/* lower half is bid by ask */
	for (size_t i = 0; i < HMAP_WIDTH; i++) {
		double b = hmap.bids[i];
		double a = hmap.asks[i];

		for (size_t j = 0; j <= i; j++) {
			double ret_long = (b / hmap.asks[j] - 1.0f) / rel_hl;
			double ret_shrt = (hmap.bids[j] / a - 1.0f) / rel_hl;

			/* lower half */
			hmap.hmap[HMAP_WIDTH * i + j] = (float)ret_long;
			hmap.hmap[HMAP_WIDTH * j + i] = (float)ret_shrt;
		}
	}
	return;
}

static int
t_anal(anal_ctx_t UNUSED(ctx), scom_t ti)
{
	unsigned int ttf = scom_thdr_ttf(ti);
	size_t idx = hmap.idx;

	switch (ttf) {
		m30_t b, a;
	case SL1T_TTF_BID:
		b = (m30_t){.v = AS_CONST_SL1T(ti)->bid};
		hmap.bids[idx] = ffff_m30_d(b);
		break;
	case SL1T_TTF_ASK:
		a = (m30_t){.v = AS_CONST_SL1T(ti)->ask};
		hmap.asks[idx] = ffff_m30_d(a);
		break;
	case SL1T_TTF_BIDASK:
		b = (m30_t){.v = AS_CONST_SL1T(ti)->v[0]};
		a = (m30_t){.v = AS_CONST_SL1T(ti)->v[1]};
		hmap.bids[idx] = ffff_m30_d(b);
		hmap.asks[idx] = ffff_m30_d(a);
		goto succ;
	default:
		return -1;
	}
	if (isnan(hmap.bids[idx]) || isnan(hmap.asks[idx])) {
		return 1;
	}
succ:
	hmap.idx++;
	return 0;
}

static void
prnt_hmap(const char *sym)
{
	static png_byte *rows[HMAP_WIDTH];
	FILE *fp;

	/* construct the file name */
	{
		size_t ssz = strlen(sym);
		memcpy(rows, sym, ssz);
		memcpy((char*)rows + ssz, ".png", sizeof(".png"));

		if (UNLIKELY((fp = fopen((char*)rows, "wb")) == NULL)) {
			return;
		}
	}

	for (size_t i = 0; i < HMAP_WIDTH; i++) {
		png_byte *rp;

		rp = rows[i] = (void*)(hmap.hmap + i * HMAP_WIDTH);
		for (size_t j = 0; j < HMAP_WIDTH; j++) {
			float rij = hmap.hmap[HMAP_WIDTH * i + j];
			int v = (int)(255.0f * rij);

			if (v < 0) {
				*rp++ = (uint8_t)-v;
				*rp++ = 0U;
				*rp++ = 0U;
				*rp++ = (uint8_t)-v;
			} else if (v > 0) {
				*rp++ = 0U;
				*rp++ = (uint8_t)v;
				*rp++ = 0U;
				*rp++ = (uint8_t)v;
			} else {
				*rp++ = 0U;
				*rp++ = 0U;
				*rp++ = 0U;
				*rp++ = 0U;
			}
		}
	}

	/* beautiful png error handling */
	if (setjmp(png_jmpbuf(pp))) {
		goto out;
	}

	png_init_io(pp, fp);
	png_set_rows(pp, ip, rows);
	png_write_png(pp, ip, PNG_TRANSFORM_IDENTITY, NULL);
out:
	png_destroy_write_struct(&pp, &ip);
	fclose(fp);
	return;
}

static void
prnt_mini(const char *sym)
{
	static png_byte __rows[4U * MINI_WIDTH * MINI_WIDTH];
	static png_byte *rows[MINI_WIDTH];
	static png_structp png;
	static png_infop nfo;
	FILE *fp;

	/* construct the file name */
	{
		size_t ssz = strlen(sym);
		memcpy(__rows, sym, ssz);
		memcpy((char*)__rows + ssz, "_mini.png", sizeof("_mini.png"));

		if (UNLIKELY((fp = fopen((char*)__rows, "wb")) == NULL)) {
			return;
		}
	}

	memset(__rows, -1, sizeof(__rows));
	for (size_t i = 0; i < MINI_WIDTH; i++) {
		png_byte *rp;
		double yb = (hmap.bids[HMAP_FACTR * i] - pot.lo) / (pot.hi - pot.lo);
		double ya = (hmap.asks[HMAP_FACTR * i] - pot.lo) / (pot.hi - pot.lo);
		int vb = (int)((MINI_WIDTH - 1) * (1.0 - yb));
		int va = (int)((MINI_WIDTH - 1) * (1.0 - ya));

		if (vb >= (int)MINI_WIDTH) {
			vb = MINI_WIDTH - 1;
		} else if (vb < 0) {
			vb = 0;
		}

		if (va >= (int)MINI_WIDTH) {
			va = MINI_WIDTH - 1;
		} else if (va < 0) {
			va = 0;
		}

		rp = __rows + 4U * (vb * MINI_WIDTH + i);
		rp[0] = 0U;
		rp[1] = 0U;
		rp[2] = 255U;
		rp[3] = 255U;

		rp = __rows + 4U * (va * MINI_WIDTH + i);
		rp[0] = 0U;
		rp[1] = 0U;
		rp[2] = 255U;
		rp[3] = 255U;

		rows[i] = __rows + 4U * i * MINI_WIDTH;
	}

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	nfo = png_create_info_struct(png);

	png_set_IHDR(
		png, nfo, MINI_WIDTH, MINI_WIDTH, 8U/*depth*/,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	/* beautiful png error handling */
	if (setjmp(png_jmpbuf(png))) {
		goto out;
	}

	png_init_io(png, fp);
	png_set_rows(png, nfo, rows);
	png_write_png(png, nfo, PNG_TRANSFORM_IDENTITY, NULL);
out:
	png_destroy_write_struct(&png, &nfo);
	fclose(fp);
	return;
}
#endif	/* HAVE_PNG_H */


/* file wide operations */
static int
anal1(anal_ctx_t ctx)
{
	utectx_t hdl = ctx->u;
	size_t nsyms = ute_nsyms(hdl);

	for (size_t i = 1; i <= nsyms; i++) {
		const char *sym = ute_idx2sym(ctx->u, i);

		UDEBUG("anal'ing %s (%zu) ...\n", sym, i);
		pot.lo = INFINITY;
		pot.hi = -INFINITY;

		for (scom_t ti; (ti = ute_iter(ctx->u)) != NULL;) {
			/* now to what we always do */
			if (scom_thdr_tblidx(ti) == i) {
				anal(ctx, ti);
			}
		}
		/* print the analysis pot */
		pr_pot();

#if defined HAVE_PNG_H
		/* now on to the hold map */
		{
			time_t ref = pot.to;
			time_t inc = (pot.tc - pot.to) / (HMAP_WIDTH - 1U);

			rset_hmap();
			for (scom_t ti; (ti = ute_iter(ctx->u)) != NULL;) {
				/* now to what we always do */
				unsigned int ttf = scom_thdr_tblidx(ti);
				time_t t = scom_thdr_sec(ti);

				if (t >= ref && ttf == i) {
					if (t_anal(ctx, ti) == 0) {
						ref += inc;
					}
				}
			}
			/* construct the hold map */
			fill_hmap();
			/* print the hold map */
			prnt_hmap(sym);
			/* print a mini chart */
			prnt_mini(sym);
		}
#endif	/* HAVE_PNG_H */
	}
	return 0;
}


#if defined STANDALONE
#include "ute-anal.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	struct anal_ctx_s ctx[1] = {0};
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (argi->interval_arg) {
		ctx->intv = strtoul(argi->interval_arg, NULL, 10);
		if (argi->modulus_arg) {
			ctx->modu = strtoul(argi->modulus_arg, NULL, 10);
		}
	} else if (argi->modulus_arg) {
		error("\
warning: --modulus without --interval is not meaningful, ignored");
	}

	for (size_t j = 0U; j < argi->nargs; j++) {
		const char *fn = argi->args[j];
		const int fl = UO_RDONLY | UO_NO_LOAD_TPC;
		utectx_t hdl;

		if ((hdl = ute_open(fn, fl)) == NULL) {
			error("not open file `%s'", fn);
			rc = 1;
			continue;
		}

		/* the actual checking */
		ctx->u = hdl;
		if (anal1(ctx)) {
			rc = 1;
		}

		/* and that's us */
		ute_close(hdl);
	}
out:
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* ute-anal.c ends here */
