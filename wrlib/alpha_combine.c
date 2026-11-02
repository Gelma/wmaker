/* alpha_combine.c - Alpha channel combination, based on Gimp 1.1.24
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#include <string.h>
#include "wraster.h"
#include "wr_i18n.h"


void RCombineAlpha(unsigned char *d, unsigned char *s, int s_has_alpha,
                    int width, int height, int dwi, int swi, int opacity)
{
	int x, y;
	unsigned char *dst = d;
	unsigned char *src = s;

	if (__builtin_expect(opacity == 255, 1)) {
		if (!s_has_alpha) {
			/* Fast path: source is fully opaque (RGB), destination gets fully replaced */
			for (y = 0; y < height; y++) {
				for (x = 0; x < width; x++) {
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = 255;
					dst += 4;
					src += 3;
				}
				dst += dwi;
				src += swi;
			}
			return;
		}

		/* Fast path: opacity == 255 with RGBA source */
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				int sa = src[3];

				if (__builtin_expect(sa == 0, 0)) {
					/* Fully transparent source: destination unchanged */
					dst += 4;
					src += 4;
					continue;
				}

				if (__builtin_expect(sa == 255, 1)) {
					/* Fully opaque source: overwrite destination directly */
					memcpy(dst, src, 4);
					dst += 4;
					src += 4;
					continue;
				}

				/* Semi-transparent source pixel */
				int t = dst[3] * (255 - sa) + 0x80;
				int alpha = sa + (((t >> 8) + t) >> 8);

				if (__builtin_expect(alpha == 0, 0)) {
					dst[3] = 0;
				} else if (sa == alpha) {
					memcpy(dst, src, 4);
				} else {
					int ratio = (sa << 8) / alpha;
					int inv_ratio = 256 - ratio;

					dst[0] = (dst[0] * inv_ratio + src[0] * ratio) >> 8;
					dst[1] = (dst[1] * inv_ratio + src[1] * ratio) >> 8;
					dst[2] = (dst[2] * inv_ratio + src[2] * ratio) >> 8;
					dst[3] = alpha;
				}

				dst += 4;
				src += 4;
			}
			dst += dwi;
			src += swi;
		}
		return;
	}

	/* General path: opacity != 255 */
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int sa = s_has_alpha ? src[3] : 255;
			int t, alpha;

			t = sa * opacity + 0x80;
			sa = ((t >> 8) + t) >> 8;

			if (__builtin_expect(sa == 0, 0)) {
				/* sa became 0 after opacity scaling: destination unchanged */
				dst += 4;
				src += s_has_alpha ? 4 : 3;
				continue;
			}

			t = dst[3] * (255 - sa) + 0x80;
			alpha = sa + (((t >> 8) + t) >> 8);

			if (alpha == 0) {
				dst[3] = 0;
			} else if (sa == alpha) {
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = alpha;
			} else {
				int ratio = (sa << 8) / alpha;
				int inv_ratio = 256 - ratio;

				dst[0] = (dst[0] * inv_ratio + src[0] * ratio) >> 8;
				dst[1] = (dst[1] * inv_ratio + src[1] * ratio) >> 8;
				dst[2] = (dst[2] * inv_ratio + src[2] * ratio) >> 8;
				dst[3] = alpha;
			}

			dst += 4;
			src += s_has_alpha ? 4 : 3;
		}
		dst += dwi;
		src += swi;
	}
}