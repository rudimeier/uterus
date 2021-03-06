/** fileutils.c -- file utility functions
 *
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
 * Copyright (C) 2010-2015 Sebastian Freundt  <hroptatyr@unserding.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * fileutils.h - File utility functions
 * Created: 2006-08-15
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <string.h>
#include <stdlib.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */

#include "fileutils.h"

#if !defined DEFUN
# define DEFUN
#endif	/* !DEFUN */

/* trick part to mimick FILE like operations on a mem blob */
static void
fm_realloc(fmcmb_t stream, size_t new_size)
{
/* expand the buffer to at least NEW_SIZE bytes */
	size_t offs = __mtell(stream);

	/* this should be a callback to allow for customised reallocators */
	stream->mem = realloc(stream->mem, new_size);

	if (offs < new_size) {
		stream->m = stream->mem + offs;
	} else {
		/* that prick of user made the buffer considerably smaller */
		stream->m = stream->mem + new_size;
	}
	stream->msz = new_size;
	return;
}

static inline size_t
fm_free_space(fmcmb_t stream)
{
	return stream->msz - __mtell(stream);
}

static ssize_t
fmread(void *data, size_t size, size_t count, fmcmb_t stream)
{
	ssize_t res;

	if (stream->msz == 0) {
		res = fread(data, size, count, stream->f);
	} else {
		size_t spc_left = fm_free_space(stream);
		size_t rdb = size * count;

		if (rdb > spc_left) {
			count = spc_left / size;
			rdb = size * count;
		}
		memcpy(data, stream->m, rdb);
		stream->m += rdb;
		res = rdb / size;
	}
	return res;
}

static ssize_t
fmwrite(const void *data, size_t size, size_t count, fmcmb_t stream)
{
#define P	(4096)
	ssize_t res;

	if (stream->msz == 0) {
		res = fwrite(data, size, count, stream->f);
	} else {
		size_t spc_left = fm_free_space(stream);
		size_t rdb = size * count;

		if (rdb > spc_left) {
			/* realloc, we need at least (rdb - spc_left) bytes,
			 * round to nearest multiple of P (4096) */
			size_t spc_needed = (rdb - spc_left) / P;
			size_t new_size = P + P * spc_needed + stream->msz;
			fm_realloc(stream, new_size);
		}
		memcpy(stream->m, data, rdb);
		stream->m += rdb;
		res = rdb / size;
	}
	return res;
#undef P
}


/*--------------------------------*
 *    FUNCTIONS IMPLEMENTATIONS   *
 *--------------------------------*/

DEFUN int
fm_read_uint32(fmcmb_t filemem, void *o_val)
{
	unsigned char buff[4];
	uint32_t *tmp = o_val;

	if (fmread(buff, sizeof(buff), 1, filemem) == 1) {
		*tmp = (buff[0] << 24) | (buff[1] << 16) |
			(buff[2] << 8) | buff[3];
		return 0;
	}
	return -1;
}

DEFUN int
fm_write_uint32(fmcmb_t filemem, uint32_t val)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)((val >> 24) & 0xff);
	buff[1] = (unsigned char)((val >> 16) & 0xff);
	buff[2] = (unsigned char)((val >> 8) & 0xff);
	buff[3] = (unsigned char)((val >> 0) & 0xff);
	return fmwrite(buff, sizeof(buff), 1, filemem) == 1 ? 0 : -1;
}

DEFUN int
fm_read_uint16(fmcmb_t filemem, void *o_val)
{
	unsigned char buff[2];
	uint16_t *tmp = o_val;

	if (fmread(buff, sizeof(buff), 1, filemem) == 1) {
		*tmp = (uint16_t)((buff[0] << 8) | buff[1]);
		return 0;
	}
	return -1;
}

DEFUN int
fm_write_uint16(fmcmb_t filemem, uint16_t val)
{
	unsigned char buff[2];

	buff[0] = (unsigned char)((val >> 8) & 0xff);
	buff[1] = (unsigned char)((val >> 0) & 0xff);
	return fmwrite(buff, sizeof(buff), 1, filemem) == 1 ? 0 : -1;
}

DEFUN int
fm_read_uint8(fmcmb_t filemem, void *o_val)
{
	return fmread(o_val, sizeof(uint8_t), 1, filemem) == 1 ? 0 : -1;
}

DEFUN int
fm_write_uint8(fmcmb_t filemem, uint8_t val)
{
	return fmwrite(&val, sizeof(val), 1, filemem) == 1 ? 0 : -1;
}

DEFUN int
fm_read_chars(fmcmb_t filemem, void *buff, size_t len)
{
	const ssize_t slen = len;
	return fmread(buff, sizeof(char), len, filemem) == slen ? 0 : -1;
}

DEFUN int
fm_write_chars(fmcmb_t filemem, const char *buff, size_t len)
{
	const ssize_t slen = len;
	return fmwrite(buff, sizeof(*buff), len, filemem) == slen ? 0 : -1;
}

/* fileutils.c ends here */
