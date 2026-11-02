/* flip.c - image flip
 *
 * Raster graphics library
 *
 * Copyright (c) 2014-2021 Window Maker Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, see
 *  <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include "wraster.h"
#include "rotate.h"
#include "wr_i18n.h"


static RImage *r_flip_vertically(RImage *source);
static RImage *r_flip_horizontally(RImage *source);

/* Flip an image in the direction(s) specified */
RImage *RFlipImage(RImage *source, int mode)
{
	/* Security */
	if (source == NULL)
		return NULL;

	switch (mode & (RVerticalFlip | RHorizontalFlip)) {
	case RHorizontalFlip:
		return r_flip_horizontally(source);

	case RVerticalFlip:
		return r_flip_vertically(source);

	case RHorizontalFlip | RVerticalFlip:
		return wraster_rotate_image_180(source);

	default:
		return RRetainImage(source);
	}
}

RImage *r_flip_vertically(RImage *source)
{
	RImage *target;
	int nwidth, nheight;
	int y;
	size_t row_stride;

	nwidth = source->width;
	nheight = source->height;

	target = RCreateImage(nwidth, nheight, (source->format != RRGBFormat));
	if (!target)
		return NULL;

	row_stride = (size_t)nwidth * (source->format == RRGBFormat ? 3 : 4);

	for (y = 0; y < nheight; y++) {
		memcpy(target->data + (size_t)(nheight - 1 - y) * row_stride,
		       source->data + (size_t)y * row_stride,
		       row_stride);
	}

	return target;
}

RImage *r_flip_horizontally(RImage *source)
{
	RImage *target;
	int nwidth, nheight;
	int x, y;

	nwidth = source->width;
	nheight = source->height;

	target = RCreateImage(nwidth, nheight, (source->format != RRGBFormat));
	if (!target)
		return NULL;

	if (source->format == RRGBFormat) {
		for (y = 0; y < nheight; y++) {
			const unsigned char *s_row = source->data + (size_t)y * nwidth * 3;
			unsigned char *d_row = target->data + (size_t)y * nwidth * 3;

			for (x = 0; x < nwidth; x++) {
				int sx = x * 3;
				int dx = (nwidth - 1 - x) * 3;
				d_row[dx] = s_row[sx];
				d_row[dx + 1] = s_row[sx + 1];
				d_row[dx + 2] = s_row[sx + 2];
			}
		}
	} else {
		const uint32_t *src32 = (const uint32_t *)source->data;
		uint32_t *dst32 = (uint32_t *)target->data;

		for (y = 0; y < nheight; y++) {
			const uint32_t *s_row = src32 + (size_t)y * nwidth;
			uint32_t *d_row = dst32 + (size_t)y * nwidth;

			for (x = 0; x < nwidth; x++) {
				d_row[nwidth - 1 - x] = s_row[x];
			}
		}
	}

	return target;
}
