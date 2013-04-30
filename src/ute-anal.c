/*** ute-anal.c -- simple analyses
 *
 * Copyright (C) 2013 Sebastian Freundt
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
/* for analysis pictures */
#include <png.h>

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


/* helper functions */
static void
__attribute__((format(printf, 1, 2), unused))
verbprf(const char *UNUSED_nodbg(fmt), ...)
{
#if defined DEBUG_FLAG
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
#endif	/* DEBUG_FLAG */
	return;
}

static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


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

static struct {
	double asks[256U];
	double hmap[256U * 256U];
	size_t idx;
} hmap;

static png_structp pp;
static png_infop ip;

static void
init_hmap(void)
{
	return;
}

static void
fini_hmap(void)
{
	png_destroy_write_struct(&pp, &ip);
	return;
}

static void
rset_hmap(void)
{
	pp = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	ip = png_create_info_struct(pp);

	png_set_IHDR(
		pp, ip, 256U, 256U, 8/*depth*/,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);
	memset(&hmap, 0, sizeof(hmap));
	return;
}

static int
t_anal(anal_ctx_t UNUSED(ctx), scom_t ti)
{
	unsigned int ttf = scom_thdr_ttf(ti);
	size_t idx = hmap.idx++;
	double rel_hl = pot.hi / pot.lo - 1.0;
	double p;

	switch (ttf) {
	case SL1T_TTF_BID:
	case SL1T_TTF_BIDASK:
		p = ffff_m30_d((m30_t){.v = AS_CONST_SL1T(ti)->v[0]});
		break;
	default:
		return 0;
	}

	hmap.asks[idx] = p;
	for (size_t i = 0; i < hmap.idx; i++) {
		hmap.hmap[256U * idx + i] = (p / hmap.asks[i] - 1.0) / rel_hl;
	}
	return 0;
}

static void
pr_hmap(const char *sym)
{
	static png_byte *rows[256U];
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

	for (size_t i = 0; i < 256U; i++) {
		png_byte *rp;

		rp = rows[i] = (void*)(hmap.hmap + i * 256U);
		for (size_t j = 0; j < i; j++) {
			double rij = hmap.hmap[256U * i + j];
			int v = (int)(256.0 * rij);

			if (v < 0) {
				uint8_t c = (uint8_t)(256U + v);

				*rp++ = 255U;
				*rp++ = c;
				*rp++ = c;
				*rp++ = (uint8_t)-v;
			} else if (v > 0) {
				uint8_t c = (uint8_t)(256U - v);

				*rp++ = c;
				*rp++ = 255U;
				*rp++ = c;
				*rp++ = (uint8_t)v;
			} else {
				*rp++ = 255U;
				*rp++ = 255U;
				*rp++ = 255U;
				*rp++ = 0;
			}
		}
		memset(rp, 0, (256U - i) * 4U);
	}

	png_init_io(pp, fp);
	png_set_rows(pp, ip, rows);
	png_write_png(pp, ip, PNG_TRANSFORM_IDENTITY, NULL);
	fclose(fp);
	return;
}


/* file wide operations */
static int
anal1(anal_ctx_t ctx)
{
	utectx_t hdl = ctx->u;
	size_t nsyms = ute_nsyms(hdl);

	/* one init to rule them all */
	init_hmap();

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

		{
			time_t ref = pot.to;
			time_t inc = (pot.tc - pot.to) / 255U;

			rset_hmap();
			for (scom_t ti; (ti = ute_iter(ctx->u)) != NULL;) {
				/* now to what we always do */
				uint16_t ttf = scom_thdr_tblidx(ti);
				time_t t = scom_thdr_sec(ti);

				if (t >= ref && ttf == i) {
					t_anal(ctx, ti);
					ref += inc;
				}
			}
			/* print the hold map */
			pr_hmap(sym);
		}
	}

	fini_hmap();
	return 0;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-anal-clo.h"
#include "ute-anal-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct anal_args_info argi[1];
	struct anal_ctx_s ctx[1] = {0};
	int res = 0;

	if (anal_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		anal_parser_print_help();
		res = 0;
		goto out;
	}

	if (argi->interval_given) {
		ctx->intv = argi->interval_arg;
		if (argi->modulus_given) {
			ctx->modu = argi->modulus_arg;
		}
	} else if (argi->modulus_given) {
		fputs("\
warning: --modulus without --interval is not meaningful, ignored\n", stderr);
	}

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *fn = argi->inputs[j];
		const int fl = UO_RDONLY | UO_NO_LOAD_TPC;
		utectx_t hdl;

		if ((hdl = ute_open(fn, fl)) == NULL) {
			error(0, "cannot open file `%s'", fn);
			res = 1;
			continue;
		}

		/* the actual checking */
		ctx->u = hdl;
		if (anal1(ctx)) {
			res = 1;
		}

		/* and that's us */
		ute_close(hdl);
	}
out:
	anal_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-anal.c ends here */
