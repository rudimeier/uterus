/*** dukas.c -- dukascopy muxer
 *
 * Copyright (C) 2009-2015 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#if defined HAVE_SYS_TYPES_H
/* for ssize_t */
# include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include "boobs.h"
#include "scommon.h"
#include "sl1t.h"
#include "scdl.h"
#include "utefile.h"
#include "ute-mux.h"
#include "nifty.h"
#include "date.h"

#define DEFINE_GORY_STUFF
#include "m30.h"

#include "cmd-aux.c"

typedef union {
	double d;
	int64_t i;
} float64_t;

typedef union {
	float d;
	int32_t i;
} float32_t;

struct dc_s {
	uint64_t ts;
	float64_t ap;
	float64_t bp;
	float64_t aq;
	float64_t bq;
};

struct dcc_s {
	uint64_t ts;
	float64_t o;
	float64_t c;
	float64_t l;
	float64_t h;
	float64_t v;
};

struct dqbi5_s {
	uint32_t ts;
	uint32_t ap;
	uint32_t bp;
	float32_t aq;
	float32_t bq;
};

struct dcbi5_s {
	uint32_t ts;
	uint32_t o;
	uint32_t c;
	uint32_t l;
	uint32_t h;
	float32_t v;
};


/* little helpers */
static m30_t
__m30_get_dukas(uint32_t val)
{
/* turns 131555 into 1.31555000 */
	/* built-in knowledge */
	m30_t res;

	if (val < 536870) {
		/* interpreted as 5 digits beyond the decimal point */
		res.mant = val * 1000;
		res.expo = 0;
	} else {
		/* interpreted as 5 digits beyond the decimal point */
		res.mant = val / 10;
		res.expo = 1;
	}
	return res;
}


static struct sl1t_s t[2];
static struct scdl_s c[1];

static bool
rd1(int f, struct dc_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = be64toh(b->ts);
	b->ap.i = be64toh(b->ap.i);
	b->bp.i = be64toh(b->bp.i);
	b->aq.i = be64toh(b->aq.i);
	b->bq.i = be64toh(b->bq.i);
	return true;
}

static bool
rd1bi5(int f, struct dqbi5_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = be32toh(b->ts);
	b->ap = be32toh(b->ap);
	b->bp = be32toh(b->bp);
	b->aq.i = be32toh(b->aq.i);
	b->bq.i = be32toh(b->bq.i);
	return true;
}

static bool
rd1c(int f, struct dcc_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = be64toh(b->ts);
	b->o.i = be64toh(b->o.i);
	b->c.i = be64toh(b->c.i);
	b->l.i = be64toh(b->l.i);
	b->h.i = be64toh(b->h.i);
	b->v.i = be64toh(b->v.i);
	return true;
}

static bool
rd1cbi5(int f, struct dcbi5_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = be32toh(b->ts);
	b->o = be32toh(b->o);
	b->c = be32toh(b->c);
	b->l = be32toh(b->l);
	b->h = be32toh(b->h);
	b->v.i = be32toh(b->v.i);
	return true;
}

#define DUKAS_VMUL	(1.0e6)
#define DUKAS_VMULF	(1.0e6f)

static void
write_tick(mux_ctx_t ctx, struct dc_s *tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	static struct dc_s last;
	uint32_t ts = tl->ts / 1000;
	uint16_t ms = tl->ts % 1000;

	if (ctx->opts->flags & SUMUX_FLAG_ALL_TICKS ||
	    tl->bp.d != last.bp.d || tl->bq.d != last.bq.d) {
		sl1t_set_stmp_sec(t + 0, ts);
		sl1t_set_stmp_msec(t + 0, ms);
		t[0].bid = ffff_m30_get_d(tl->bp.d).v;
		t[0].bsz = ffff_m30_get_d(tl->bq.d).v;
		/* yup, add him */
		ute_add_tick(ctx->wrr, AS_SCOM(t));
	}
	if (ctx->opts->flags & SUMUX_FLAG_ALL_TICKS ||
	    tl->ap.d != last.ap.d || tl->aq.d != last.aq.d) {
		sl1t_set_stmp_sec(t + 1, ts);
		sl1t_set_stmp_msec(t + 1, ms);
		t[1].ask = ffff_m30_get_d(tl->ap.d).v;
		t[1].asz = ffff_m30_get_d(tl->aq.d).v;
		/* off we go */
		ute_add_tick(ctx->wrr, AS_SCOM(t + 1));
	}
	/* for the record */
	if (LIKELY(!(ctx->opts->flags & SUMUX_FLAG_ALL_TICKS))) {
		last = *tl;
	}
	return;
}

static void
write_tick_bi5(mux_ctx_t ctx, struct dqbi5_s *tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	static struct dqbi5_s last;
	unsigned int ts = tl->ts / 1000;
	unsigned int ms = tl->ts % 1000;

	if (ctx->opts->flags & SUMUX_FLAG_ALL_TICKS ||
	    tl->bp != last.bp || tl->bq.i != last.bq.i) {
		sl1t_set_stmp_sec(t + 0, ts + ctx->opts->tsoff);
		sl1t_set_stmp_msec(t + 0, (uint16_t)ms);
		t[0].bid = __m30_get_dukas(
			tl->bp * ctx->opts->mul / ctx->opts->mag).v;
		t[0].bsz = ffff_m30_get_f(tl->bq.d * DUKAS_VMULF).v;
		/* and off we go to add him */
		ute_add_tick(ctx->wrr, AS_SCOM(t));
	}
	if (ctx->opts->flags & SUMUX_FLAG_ALL_TICKS ||
	    tl->ap != last.ap || tl->aq.i != last.aq.i) {
		sl1t_set_stmp_sec(t + 1, ts + ctx->opts->tsoff);
		sl1t_set_stmp_msec(t + 1, (uint16_t)ms);
		t[1].ask = __m30_get_dukas(
			tl->ap * ctx->opts->mul / ctx->opts->mag).v;
		t[1].asz = ffff_m30_get_f(tl->aq.d * DUKAS_VMULF).v;
		/* and off again */
		ute_add_tick(ctx->wrr, AS_SCOM(t + 1));
	}
	/* for our compressor */
	if (LIKELY(!(ctx->opts->flags & SUMUX_FLAG_ALL_TICKS))) {
		last = *tl;
	}
	return;
}

static void
write_cdl(mux_ctx_t ctx, struct dcc_s *tl, uint32_t cdl_len)
{
/* create one or more sparse ticks, sl1t_t objects */
	uint32_t ts = tl->ts / 1000;
	uint16_t ms = tl->ts % 1000;

	scdl_set_stmp_sec(c + 0, ts + cdl_len);
	scdl_set_stmp_msec(c + 0, ms);

	c[0].h = ffff_m30_get_d(tl->h.d).v;
	c[0].l = ffff_m30_get_d(tl->l.d).v;
	c[0].o = ffff_m30_get_d(tl->o.d).v;
	c[0].c = ffff_m30_get_d(tl->c.d).v;
	c[0].sta_ts = ts;
	c[0].cnt = ffff_m30_get_d(tl->v.d).v;
	/* and shove it */
	ute_add_tick(ctx->wrr, AS_SCOM(c));
	return;
}

static void
write_cdl_bi5(mux_ctx_t ctx, struct dcbi5_s *tl, uint32_t cdl_len)
{
/* create one or more candles, scdl_t objects */
	/* apparently timestamps are in second offsets now */
	unsigned int ts = tl->ts + ctx->opts->tsoff;

	scdl_set_stmp_sec(c + 0, ts + cdl_len);
	scdl_set_stmp_msec(c + 0, 0);

	c[0].h = __m30_get_dukas(tl->h * ctx->opts->mul / ctx->opts->mag).v;
	c[0].l = __m30_get_dukas(tl->l * ctx->opts->mul / ctx->opts->mag).v;
	c[0].o = __m30_get_dukas(tl->o * ctx->opts->mul / ctx->opts->mag).v;
	c[0].c = __m30_get_dukas(tl->c * ctx->opts->mul / ctx->opts->mag).v;
	c[0].sta_ts = ts;
	c[0].cnt = ffff_m30_get_f(tl->v.d * DUKAS_VMULF).v;
	/* and shove it */
	ute_add_tick(ctx->wrr, AS_SCOM(c));
	return;
}

static void
dump_tick_bi5(mux_ctx_t ctx, struct dqbi5_s *tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	static struct dqbi5_s last;
	unsigned int ts = tl->ts / 1000;
	unsigned int ms = tl->ts % 1000;
	int32_t off = ctx->opts->tsoff;

	if (ctx->opts->flags & SUMUX_FLAG_ALL_TICKS ||
	    tl->bp != last.bp || tl->bq.i != last.bq.i) {
		printf("%s\t%u.%u\tb\t%u\t%f\n",
		       ctx->opts->sname, ts + off, ms, tl->bp, tl->bq.d);
	}
	if (ctx->opts->flags & SUMUX_FLAG_ALL_TICKS ||
	    tl->ap != last.ap || tl->aq.i != last.aq.i) {
		printf("%s\t%u.%u\ta\t%u\t%f\n",
		       ctx->opts->sname, ts + off, ms, tl->ap, tl->aq.d);
	}
	if (LIKELY(!(ctx->opts->flags & SUMUX_FLAG_ALL_TICKS))) {
		last = *tl;
	}
	return;
}


static void
prepare(mux_ctx_t ctx)
{
	unsigned int idx = 0;

	if (LIKELY(ctx->opts->sname != NULL)) {
		idx = ute_sym2idx(ctx->wrr, ctx->opts->sname);
	}

	/* t is static, so set the static components here */
	sl1t_set_ttf(t + 0, SL1T_TTF_BID);
	sl1t_set_tblidx(t + 0, (uint16_t)idx);
	sl1t_set_ttf(t + 1, SL1T_TTF_ASK);
	sl1t_set_tblidx(t + 1, (uint16_t)idx);
	/* also c is static (and a pun), set their components here */
	scom_thdr_set_tblidx(c->hdr, (uint16_t)idx);
	return;
}

static void
proc_l1bi5(mux_ctx_t ctx)
{
	union {
		struct dc_s bin[1];
		struct dqbi5_s bi5[2];
	} buf[1];
	ssize_t nrd;
	int fd = ctx->infd;

	/* read a probe */
	if (UNLIKELY((nrd = read(fd, buf->bi5, sizeof(buf->bi5))) <= 0)) {
		return;
	} else if (UNLIKELY(nrd == sizeof(*buf->bi5))) {
		/* only one record then, just go for bi5 format,
		 * make use of the tick compressor and dupe the record
		 * then just proceed normally */
		memcpy(buf->bi5 + 1, buf->bi5 + 0, sizeof(*buf->bi5));
	} else if (UNLIKELY((size_t)nrd < sizeof(buf->bi5))) {
		/* oooh incomplete innit? */
		return;
	}
	/* the only thing we can make assumptions about is the timestamp
	 * we check the two stamps in bi5 and compare their distance */
	{
		uint32_t ts0 = be32toh(buf->bi5[0].ts);
		uint32_t ts1 = be32toh(buf->bi5[1].ts);

		if (ts1 - ts0 > 60/*min*/ * 60/*sec*/ * 1000/*msec*/) {
			/* definitely old_fmt */
			goto old_fmt;
		}

		/* quickly polish the probe */
		buf->bi5[0].ts = ts0;
		buf->bi5[0].ap = be32toh(buf->bi5[0].ap);
		buf->bi5[0].bp = be32toh(buf->bi5[0].bp);
		buf->bi5[0].aq.i = be32toh(buf->bi5[0].aq.i);
		buf->bi5[0].bq.i = be32toh(buf->bi5[0].bq.i);

		buf->bi5[1].ts = ts1;
		buf->bi5[1].ap = be32toh(buf->bi5[1].ap);
		buf->bi5[1].bp = be32toh(buf->bi5[1].bp);
		buf->bi5[1].aq.i = be32toh(buf->bi5[1].aq.i);
		buf->bi5[1].bq.i = be32toh(buf->bi5[1].bq.i);
	}
	/* re-use the probe data */
	write_tick_bi5(ctx, buf->bi5 + 0);
	/* main loop */
	do {
		write_tick_bi5(ctx, buf->bi5 + 1);
	} while (rd1bi5(fd, buf->bi5 + 1));
	return;
old_fmt:
	/* polish the probe */
	buf->bin->ts = be64toh(buf->bin->ts);
	buf->bin->ap.i = be64toh(buf->bin->ap.i);
	buf->bin->bp.i = be64toh(buf->bin->bp.i);
	buf->bin->aq.i = be64toh(buf->bin->aq.i);
	buf->bin->bq.i = be64toh(buf->bin->bq.i);
	/* main loop */
	do {
		write_tick(ctx, buf->bin);
	} while (rd1(fd, buf->bin));
	return;
}

static void
proc_cdbi5(mux_ctx_t ctx)
{
	union {
		struct dcc_s bin[2];
		struct dcbi5_s bi5[2];
	} buf[1];
	unsigned int cdl_len;

	/* rinse */
	memset(&buf, 0, sizeof(buf));

	/* read a probe */
	if (UNLIKELY(read(ctx->infd, buf->bi5, sizeof(buf->bi5)) <= 0)) {
		return;
	}

	/* again we can only make assumptions about the timestamps
	 * check the two stamps in bi5 and compare their distance */
	{
		uint32_t ts0 = be32toh(buf->bi5[0].ts);
		uint32_t ts1 = be32toh(buf->bi5[1].ts);

		if ((cdl_len = ts1 - ts0) > 60/*min*/ * 60/*sec*/) {
			/* definitely old_fmt */
			goto old_fmt;
		}

		/* quickly polish the probe */
		buf->bi5[0].ts = ts0;
		buf->bi5[0].o = be32toh(buf->bi5[0].o);
		buf->bi5[0].c = be32toh(buf->bi5[0].c);
		buf->bi5[0].l = be32toh(buf->bi5[0].l);
		buf->bi5[0].h = be32toh(buf->bi5[0].h);
		buf->bi5[0].v.i = be32toh(buf->bi5[0].v.i);

		buf->bi5[1].ts = ts1;
		buf->bi5[1].o = be32toh(buf->bi5[1].o);
		buf->bi5[1].c = be32toh(buf->bi5[1].c);
		buf->bi5[1].l = be32toh(buf->bi5[1].l);
		buf->bi5[1].h = be32toh(buf->bi5[1].h);
		buf->bi5[1].v.i = be32toh(buf->bi5[1].v.i);
	}
	/* re-use the probe data */
	write_cdl_bi5(ctx, buf->bi5 + 0, cdl_len);
	/* main loop */
	do {
		write_cdl_bi5(ctx, buf->bi5 + 1, cdl_len);
	} while (rd1cbi5(ctx->infd, buf->bi5 + 1));
	return;

old_fmt:
	/* polish the probe */
	buf->bin->ts = be64toh(buf->bin->ts);
	buf->bin->o.i = be64toh(buf->bin->o.i);
	buf->bin->c.i = be64toh(buf->bin->c.i);
	buf->bin->l.i = be64toh(buf->bin->l.i);
	buf->bin->h.i = be64toh(buf->bin->h.i);
	buf->bin->v.i = be64toh(buf->bin->v.i);
	if (!rd1c(ctx->infd, buf->bin + 1)) {
		/* assume a day candle and exit */
		write_cdl(ctx, buf->bin, 86400U);
		return;
	}
	/* otherwise compute the candle length, we know dukascopy will
	 * not give us overlapping candles, so just diff the time stamps */
	cdl_len = (buf->bin[1].ts - buf->bin->ts) / 1000;
	/* and emit the original candle from the probe data */
	write_cdl(ctx, buf->bin, cdl_len);
	/* main loop */
	do {
		write_cdl(ctx, buf->bin + 1, cdl_len);
	} while (rd1c(ctx->infd, buf->bin + 1));
	return;
}

static void
dump_l1bi5(mux_ctx_t ctx)
{
	union {
		struct dc_s bin[1];
		struct dqbi5_s bi5[2];
	} buf[1];

	/* rinse, rinse, rinse */
	memset(buf, 0, sizeof(*buf));

	/* read a probe */
	if (UNLIKELY(read(ctx->infd, buf->bi5, sizeof(buf->bi5)) <= 0)) {
		return;
	}
	/* the only thing we can make assumptions about is the timestamp
	 * we check the two stamps in bi5 and compare their distance */
	{
		uint32_t ts0 = be32toh(buf->bi5[0].ts);
		uint32_t ts1 = be32toh(buf->bi5[1].ts);

		if (ts1 - ts0 > 60/*min*/ * 60/*sec*/ * 1000/*msec*/) {
			/* definitely old_fmt */
			goto old_fmt;
		}

		/* quickly polish the probe */
		buf->bi5[0].ts = ts0;
		buf->bi5[0].ap = be32toh(buf->bi5[0].ap);
		buf->bi5[0].bp = be32toh(buf->bi5[0].bp);
		buf->bi5[0].aq.i = be32toh(buf->bi5[0].aq.i);
		buf->bi5[0].bq.i = be32toh(buf->bi5[0].bq.i);

		buf->bi5[1].ts = ts1;
		buf->bi5[1].ap = be32toh(buf->bi5[1].ap);
		buf->bi5[1].bp = be32toh(buf->bi5[1].bp);
		buf->bi5[1].aq.i = be32toh(buf->bi5[1].aq.i);
		buf->bi5[1].bq.i = be32toh(buf->bi5[1].bq.i);
	}
	/* re-use the probe data */
	dump_tick_bi5(ctx, buf->bi5 + 0);
	/* main loop */
	do {
		dump_tick_bi5(ctx, buf->bi5 + 1);
	} while (rd1bi5(ctx->infd, buf->bi5 + 1));
old_fmt:
	return;
}

static int
guess(mux_ctx_t ctx, const char *fn)
{
/* guess the specs from the filename FN. */
	/* currency abbrev stop-set */
	static char ccy_ss[] = "ABCDEFGHJKNOPRSUXYZ";
	static char dt_ss[] = "0123456789/";
	const char *x, *y;

	/* try to snarf off the ccys first */
	x = fn;
	while ((x = strpbrk(x, ccy_ss)) != NULL) {
		if (strspn(x, ccy_ss) == 6UL) {
			static char sym[8];
			memcpy(sym, x, 6);
			ctx->opts->sname = sym;
			break;
		}
		x++;
	}
	/* date and time should come afterwards */
	while ((x = strpbrk(x, dt_ss)) != NULL) {
		struct tm tm[1];

		if (strspn(x++, dt_ss) >= 1 + 4 + 1 + 2 + 1 + 2 + 1 &&
		    (y = strptime(x, "%Y/%m/%d", tm)) != NULL &&
		    *y == '/') {
			/* found something like /YYYY/mm/dd */
			/* zap to the hour bit */
			while (*++y == '/');
			if (y[2] != 'h') {
				/* not in the form /..h */
				continue;
			}
			/* complete the hour bit */
			tm->tm_hour = atoi(y);
			tm->tm_min = 0;
			tm->tm_sec = 0;
			ctx->opts->tsoff = ffff_timegm(tm);
			break;
		}
	}
	return 0;
}


/* new all in one dukas slabber */
void
mux(mux_ctx_t ctx)
{
	prepare(ctx);
	switch (ctx->opts->tt) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
		scdl_set_ttf(c, ctx->opts->tt);
		proc_cdbi5(ctx);
		break;
	default:
		proc_l1bi5(ctx);
		break;
	}
	return;
}

#include "dukas.yucc"

int
mux_main(mux_ctx_t ctx, int argc, char *argv[])
{
	static struct mux_ctx_s __ctx[1U];
	static struct sumux_opt_s __opts[1U];
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (UNLIKELY(ctx == NULL)) {
		ctx = __ctx;
		ctx->opts = __opts;
	}

	if (argi->human_readable_flag) {
		/* wipe off the stuff ute-mux prepared for us */
		const char *fn;

		if (ctx->wrr != NULL &&
		    ctx->opts->outfile == NULL &&
		    (fn = ute_fn(ctx->wrr)) != NULL) {
			unlink(fn);
			ute_close(ctx->wrr);
			ctx->wrr = NULL;
		}
	}
	if (argi->all_flag) {
		ctx->opts->flags |= SUMUX_FLAG_ALL_TICKS;
	}

	for (size_t j = 0U; j < argi->nargs; j++) {
		const char *f = argi->args[j];
		int fd;

		/* open the infile ... */
		if (f[0] == '-' && f[1] == '\0') {
			ctx->infd = fd = STDIN_FILENO;
			ctx->infn = NULL;
			ctx->badfd = STDERR_FILENO;
		} else if ((fd = open(f, 0)) < 0) {
			ctx->infd = -1;
			error("cannot open file '%s'", f);
			/* just try the next bloke */
			continue;
		} else if (argi->guess_flag && guess(ctx, f) < 0) {
			error("cannot guess info from '%s'", f);
			/* well try to do the actual muxing anyway */
			;
		} else {
			/* huuuuge success */
			ctx->infd = fd;
			ctx->infn = f;
			ctx->badfd = STDERR_FILENO;
		}
		/* ... and now mux it */
		if (argi->human_readable_flag) {
			dump_l1bi5(ctx);
		} else {
			mux(ctx);
		}
		/* close the infile */
		close(fd);
	}

out:
	yuck_free(argi);
	return rc;
}


#if defined STANDALONE
int
main(int argc, char *argv[])
{
	return mux_main(NULL, argc, argv);
}
#endif	/* STANDALONE */

/* dukas.c ends here */
