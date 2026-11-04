/* convert.c - convert RImage to Pixmap
 *
 * Raster graphics library
 *
 * Copyright (c) 1997-2003 Alfredo K. Kojima
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

/* Problems:
 *   1. Using Grayscale visual with Dithering crashes wmaker
 *   2. Ghost dock/appicon is wrong in Pseudocolor, Staticgray, Grayscale
 */

#include <config.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "wraster.h"
#include "convert.h"
#include "xutil.h"
#include "wr_i18n.h"


#define NFREE(n)  if (n) free(n)

#define HAS_ALPHA(I)	((I)->format == RRGBAFormat)

typedef struct RConversionTable {
	unsigned short table[256];
	unsigned short index;

	struct RConversionTable *next;
} RConversionTable;

static RConversionTable *conversionTable = NULL;

static void release_conversion_table(void)
{
	RConversionTable *tmp = conversionTable;

	while (tmp) {
		RConversionTable *tmp_to_delete = tmp;

		tmp = tmp->next;
		free(tmp_to_delete);
	}
	conversionTable = NULL;
}

void r_destroy_conversion_tables(void)
{
	release_conversion_table();
}

static unsigned short *computeTable(unsigned short mask)
{
	RConversionTable *tmp = conversionTable;
	int i;

	while (tmp) {
		if (tmp->index == mask)
			break;
		tmp = tmp->next;
	}

	if (tmp)
		return tmp->table;

	tmp = (RConversionTable *) malloc(sizeof(RConversionTable));
	if (tmp == NULL)
		return NULL;

	for (i = 0; i < 256; i++)
		tmp->table[i] = (i * mask + 0x7f) / 0xff;

	tmp->index = mask;
	tmp->next = conversionTable;
	conversionTable = tmp;
	return tmp->table;
}

/***************************************************************************/

static void
convertTrueColor_generic(RXImage * ximg, RImage * image,
			 signed char *err, signed char *nerr,
			 const unsigned short *rtable,
			 const unsigned short *gtable,
			 const unsigned short *btable,
			 const int dr, const int dg, const int db,
			 const unsigned short roffs, const unsigned short goffs, const unsigned short boffs)
{
	signed char *terr;
	int x, y, r, g, b;
	int pixel;
	int rer, ger, ber;
	unsigned char *ptr = image->data;
	int channels = (HAS_ALPHA(image) ? 4 : 3);

	/* convert and dither the image to XImage */
	for (y = 0; y < image->height; y++) {
		nerr[0] = 0;
		nerr[1] = 0;
		nerr[2] = 0;
		for (x = 0; x < image->width; x++, ptr += channels) {

			/* reduce pixel */
			pixel = *ptr + err[x];
			if (pixel < 0)
				pixel = 0;
			else if (pixel > 0xff)
				pixel = 0xff;
			r = rtable[pixel];
			/* calc error */
			rer = pixel - r * dr;

			/* reduce pixel */
			pixel = *(ptr + 1) + err[x + 1];
			if (pixel < 0)
				pixel = 0;
			else if (pixel > 0xff)
				pixel = 0xff;
			g = gtable[pixel];
			/* calc error */
			ger = pixel - g * dg;

			/* reduce pixel */
			pixel = *(ptr + 2) + err[x + 2];
			if (pixel < 0)
				pixel = 0;
			else if (pixel > 0xff)
				pixel = 0xff;
			b = btable[pixel];
			/* calc error */
			ber = pixel - b * db;

			pixel = (r << roffs) | (g << goffs) | (b << boffs);
			XPutPixel(ximg->image, x, y, pixel);

			/* distribute error */
			r = (rer * 3) / 8;
			g = (ger * 3) / 8;
			b = (ber * 3) / 8;
			/* x+1, y */
			err[x + 3 * 1] += r;
			err[x + 1 + 3 * 1] += g;
			err[x + 2 + 3 * 1] += b;
			/* x, y+1 */
			nerr[x] += r;
			nerr[x + 1] += g;
			nerr[x + 2] += b;
			/* x+1, y+1 */
			nerr[x + 3 * 1] = rer - 2 * r;
			nerr[x + 1 + 3 * 1] = ger - 2 * g;
			nerr[x + 2 + 3 * 1] = ber - 2 * b;
		}
		/* skip to next line */
		terr = err;
		err = nerr;
		nerr = terr;
	}

	/* redither the 1st line to distribute error better */
	ptr = image->data;
	y = 0;
	nerr[0] = 0;
	nerr[1] = 0;
	nerr[2] = 0;
	for (x = 0; x < image->width; x++, ptr += channels) {

		/* reduce pixel */
		pixel = *ptr + err[x];
		if (pixel < 0)
			pixel = 0;
		else if (pixel > 0xff)
			pixel = 0xff;
		r = rtable[pixel];
		/* calc error */
		rer = pixel - r * dr;

		/* reduce pixel */
		pixel = *(ptr + 1) + err[x + 1];
		if (pixel < 0)
			pixel = 0;
		else if (pixel > 0xff)
			pixel = 0xff;
		g = gtable[pixel];
		/* calc error */
		ger = pixel - g * dg;

		/* reduce pixel */
		pixel = *(ptr + 2) + err[x + 2];
		if (pixel < 0)
			pixel = 0;
		else if (pixel > 0xff)
			pixel = 0xff;
		b = btable[pixel];
		/* calc error */
		ber = pixel - b * db;

		pixel = (r << roffs) | (g << goffs) | (b << boffs);
		XPutPixel(ximg->image, x, y, pixel);

		/* distribute error */
		r = (rer * 3) / 8;
		g = (ger * 3) / 8;
		b = (ber * 3) / 8;
		/* x+1, y */
		err[x + 3 * 1] += r;
		err[x + 1 + 3 * 1] += g;
		err[x + 2 + 3 * 1] += b;
		/* x, y+1 */
		nerr[x] += r;
		nerr[x + 1] += g;
		nerr[x + 2] += b;
		/* x+1, y+1 */
		nerr[x + 3 * 1] = rer - 2 * r;
		nerr[x + 1 + 3 * 1] = ger - 2 * g;
		nerr[x + 2 + 3 * 1] = ber - 2 * b;
	}
}

static RXImage *image2TrueColor(RContext * ctx, RImage * image)
{
	RXImage *ximg;
	unsigned short rmask, gmask, bmask;
	unsigned short roffs, goffs, boffs;
	unsigned short *rtable, *gtable, *btable;
	int channels = (HAS_ALPHA(image) ? 4 : 3);

	ximg = RCreateXImage(ctx, ctx->depth, image->width, image->height);
	if (!ximg) {
		return NULL;
	}

	roffs = ctx->red_offset;
	goffs = ctx->green_offset;
	boffs = ctx->blue_offset;

	rmask = ctx->visual->red_mask >> roffs;
	gmask = ctx->visual->green_mask >> goffs;
	bmask = ctx->visual->blue_mask >> boffs;

	rtable = computeTable(rmask);
	gtable = computeTable(gmask);
	btable = computeTable(bmask);

	if (rtable == NULL || gtable == NULL || btable == NULL) {
		RErrorCode = RERR_NOMEMORY;
		RDestroyXImage(ctx, ximg);
		return NULL;
	}

	if (ctx->attribs->render_mode == RBestMatchRendering) {
		int ofs;
		unsigned long r, g, b;
		int x, y;
		unsigned long pixel;
		unsigned char *ptr = image->data;
		static const int test_endian = 1;
		int native_order = (*(const char *)&test_endian == 1) ? LSBFirst : MSBFirst;
		int direct32 = (ximg->image->bits_per_pixel == 32 && ximg->image->byte_order == native_order);

		/* fake match */
#ifdef WRLIB_DEBUG
		fputs("true color match\n", stderr);
#endif
		if (rmask == 0xff && gmask == 0xff && bmask == 0xff) {
			if (direct32) {
				int bpl = ximg->image->bytes_per_line;
				char *imdata = ximg->image->data;
				for (y = 0; y < image->height; y++) {
					uint32_t *line = (uint32_t *)(imdata + y * bpl);
					for (x = 0; x < image->width; x++, ptr += channels) {
						line[x] = ((uint32_t)ptr[0] << roffs) |
						          ((uint32_t)ptr[1] << goffs) |
						          ((uint32_t)ptr[2] << boffs);
					}
				}
			} else {
				for (y = 0; y < image->height; y++) {
					for (x = 0; x < image->width; x++, ptr += channels) {
						/* reduce pixel */
						r = ptr[0];
						g = ptr[1];
						b = ptr[2];
						pixel = (r << roffs) | (g << goffs) | (b << boffs);
						XPutPixel(ximg->image, x, y, pixel);
					}
				}
			}
		} else {
			if (direct32) {
				int bpl = ximg->image->bytes_per_line;
				char *imdata = ximg->image->data;
				for (y = 0, ofs = 0; y < image->height; y++) {
					uint32_t *line = (uint32_t *)(imdata + y * bpl);
					for (x = 0; x < image->width; x++, ofs += channels - 3) {
						r = rtable[ptr[ofs++]];
						g = gtable[ptr[ofs++]];
						b = btable[ptr[ofs++]];
						line[x] = (r << roffs) | (g << goffs) | (b << boffs);
					}
				}
			} else {
				for (y = 0, ofs = 0; y < image->height; y++) {
					for (x = 0; x < image->width; x++, ofs += channels - 3) {
						/* reduce pixel */
						r = rtable[ptr[ofs++]];
						g = gtable[ptr[ofs++]];
						b = btable[ptr[ofs++]];
						pixel = (r << roffs) | (g << goffs) | (b << boffs);
						XPutPixel(ximg->image, x, y, pixel);
					}
				}
			}
		}
	} else {
		/* dither */
		const int dr = 0xff / rmask;
		const int dg = 0xff / gmask;
		const int db = 0xff / bmask;

#ifdef WRLIB_DEBUG
		fputs("true color dither\n", stderr);
#endif

		{
			signed char *err;
			signed char *nerr;
			int ch = (HAS_ALPHA(image) ? 4 : 3);

			err = malloc(ch * (image->width + 2));
			nerr = malloc(ch * (image->width + 2));
			if (!err || !nerr) {
				NFREE(err);
				NFREE(nerr);
				RErrorCode = RERR_NOMEMORY;
				RDestroyXImage(ctx, ximg);
				return NULL;
			}

			memset(err, 0, ch * (image->width + 2));
			memset(nerr, 0, ch * (image->width + 2));

			convertTrueColor_generic(ximg, image, err, nerr,
						 rtable, gtable, btable, dr, dg, db, roffs, goffs, boffs);
			free(err);
			free(nerr);
		}

	}

	return ximg;
}

/***************************************************************************/

static RXImage *image2Bitmap(RContext * ctx, RImage * image, int threshold)
{
	RXImage *ximg;
	unsigned char *alpha;
	int x, y;

	ximg = RCreateXImage(ctx, 1, image->width, image->height);
	if (!ximg) {
		return NULL;
	}
	alpha = image->data + 3;

	for (y = 0; y < image->height; y++) {
		for (x = 0; x < image->width; x++) {
			XPutPixel(ximg->image, x, y, (*alpha <= threshold ? 0 : 1));
			alpha += 4;
		}
	}

	return ximg;
}

int RConvertImage(RContext * context, RImage * image, Pixmap * pixmap)
{
	RXImage *ximg = NULL;
#ifdef USE_XSHM
	Pixmap tmp;
#endif

	assert(context != NULL);
	assert(image != NULL);
	assert(pixmap != NULL);

	/* Only TrueColor visuals are supported: the 8-bit paletted conversion
	 * paths were removed together with the quantization tables. */
	if (context->vclass != TrueColor) {
		RErrorCode = RERR_BADVISUALID;
		return False;
	}
	ximg = image2TrueColor(context, image);

	if (!ximg) {
		return False;
	}

	*pixmap = XCreatePixmap(context->dpy, context->drawable, image->width, image->height, context->depth);

#ifdef USE_XSHM
	if (context->flags.use_shared_pixmap && ximg->is_shared)
		tmp = R_CreateXImageMappedPixmap(context, ximg);
	else
		tmp = None;
	if (tmp) {
		/*
		 * We have to copy the shm Pixmap into a normal Pixmap because
		 * otherwise, we would have to control when Pixmaps are freed so
		 * that we can detach their shm segments. This is a problem if the
		 * program crash, leaving stale shared memory segments in the
		 * system (lots of them). But with some work, we can optimize
		 * things and remove this XCopyArea. This will require
		 * explicitly freeing all pixmaps when exiting or restarting
		 * wmaker.
		 */
		XCopyArea(context->dpy, tmp, *pixmap, context->copy_gc, 0, 0, image->width, image->height, 0, 0);
		XFreePixmap(context->dpy, tmp);
	} else {
		RPutXImage(context, *pixmap, context->copy_gc, ximg, 0, 0, 0, 0, image->width, image->height);
	}
#else				/* !USE_XSHM */
	RPutXImage(context, *pixmap, context->copy_gc, ximg, 0, 0, 0, 0, image->width, image->height);
#endif				/* !USE_XSHM */

	RDestroyXImage(context, ximg);

	return True;
}

/* make the gc permanent (create with context creation).
 * GC creation is very expensive. altering its properties is not. -Dan
 */
int RConvertImageMask(RContext * context, RImage * image, Pixmap * pixmap, Pixmap * mask, int threshold)
{
	GC gc;
	XGCValues gcv;
	RXImage *ximg = NULL;

	assert(context != NULL);
	assert(image != NULL);
	assert(pixmap != NULL);
	assert(mask != NULL);

	if (!RConvertImage(context, image, pixmap))
		return False;

	if (image->format == RRGBFormat) {
		*mask = None;
		return True;
	}

	ximg = image2Bitmap(context, image, threshold);

	if (!ximg) {
		return False;
	}
	*mask = XCreatePixmap(context->dpy, context->drawable, image->width, image->height, 1);
	gcv.foreground = context->black;
	gcv.background = context->white;
	gcv.graphics_exposures = False;
	gc = XCreateGC(context->dpy, *mask, GCForeground | GCBackground | GCGraphicsExposures, &gcv);
	RPutXImage(context, *mask, gc, ximg, 0, 0, 0, 0, image->width, image->height);
	RDestroyXImage(context, ximg);
	XFreeGC(context->dpy, gc);

	return True;
}

Bool RGetClosestXColor(RContext * context, const RColor * color, XColor * retColor)
{
	if (context->vclass == TrueColor) {
		unsigned short rmask, gmask, bmask;
		unsigned short roffs, goffs, boffs;
		unsigned short *rtable, *gtable, *btable;

		roffs = context->red_offset;
		goffs = context->green_offset;
		boffs = context->blue_offset;

		rmask = context->visual->red_mask >> roffs;
		gmask = context->visual->green_mask >> goffs;
		bmask = context->visual->blue_mask >> boffs;

		rtable = computeTable(rmask);
		gtable = computeTable(gmask);
		btable = computeTable(bmask);

		retColor->pixel = (((unsigned long) rtable[color->red])   << roffs)
		               |  (((unsigned long) gtable[color->green]) << goffs)
		               |  (((unsigned long) btable[color->blue])  << boffs);

		retColor->red = color->red << 8;
		retColor->green = color->green << 8;
		retColor->blue = color->blue << 8;
		retColor->flags = DoRed | DoGreen | DoBlue;

	} else {
		/* Only TrueColor is supported; 8-bit visuals were removed. */
		RErrorCode = RERR_BADVISUALID;
		return False;
	}

	return True;
}
