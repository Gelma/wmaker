/* wrlib_xpm_test.c - regression test for the XPM colour table.
 *
 * A transparent entry ("None") must come out as fully transparent *and*
 * with its RGB bytes defined: both loaders copy the whole colour table
 * into the image, so leaving the colour uninitialized puts indeterminate
 * bytes in the image buffer (clang-analyzer flagged exactly that in
 * load_xpm_normalized.c).  The libXpm back-end has always zeroed them;
 * this pins both back-ends to the same contract.
 *
 * The loader is selected at configure time (USE_XPM): the libXpm back-end
 * needs a display for XParseColor(), so the test takes one from DISPLAY and
 * skips when there is none.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include <wraster.h>

static char *xpm_with_none[] = {
	"2 2 2 1",
	"a c #ff0000",
	"b c None",
	"ab",
	"ba",
	NULL
};

static char *xpm_opaque[] = {
	"2 1 2 1",
	"a c #102030",
	"b c #405060",
	"ab",
	NULL
};

static int check_pixel(const RImage * image, unsigned x, unsigned y,
                       unsigned char r, unsigned char g, unsigned char b,
                       unsigned char a, int alpha)
{
	const unsigned char *p = image->data +
	    ((size_t)y * image->width + x) * (alpha ? 4 : 3);

	if (p[0] != r || p[1] != g || p[2] != b || (alpha && p[3] != a)) {
		fprintf(stderr, "pixel %u,%u: got %u,%u,%u,%u expected %u,%u,%u,%u\n",
		        x, y, p[0], p[1], p[2], alpha ? p[3] : 255, r, g, b, a);
		return 1;
	}
	return 0;
}

int main(void)
{
	Display *dpy;
	RContext *ctx;
	RImage *img;
	int failed = 0;

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		printf("XPM loader: SKIP (no DISPLAY)\n");
		return 0;
	}
	ctx = RCreateContext(dpy, DefaultScreen(dpy), NULL);
	if (!ctx) {
		fprintf(stderr, "cannot create raster context\n");
		XCloseDisplay(dpy);
		return 1;
	}

	img = RGetImageFromXPMData(ctx, xpm_with_none);
	if (!img) {
		fprintf(stderr, "RGetImageFromXPMData failed on the None sample: %s\n",
		        RMessageForError(RErrorCode));
		return 1;
	}
	if (img->format != RRGBAFormat) {
		fprintf(stderr, "expected an alpha image, got format %d\n", img->format);
		return 1;
	}

	/* opaque entry, then transparent entry with defined RGB */
	failed |= check_pixel(img, 0, 0, 0xff, 0x00, 0x00, 0xff, 1);
	failed |= check_pixel(img, 1, 0, 0x00, 0x00, 0x00, 0x00, 1);
	failed |= check_pixel(img, 0, 1, 0x00, 0x00, 0x00, 0x00, 1);
	failed |= check_pixel(img, 1, 1, 0xff, 0x00, 0x00, 0xff, 1);
	RReleaseImage(img);

	img = RGetImageFromXPMData(ctx, xpm_opaque);
	if (!img) {
		fprintf(stderr, "RGetImageFromXPMData failed on the opaque sample: %s\n",
		        RMessageForError(RErrorCode));
		return 1;
	}
	/* The libXpm back-end always builds an alpha image. */
	if (img->format == RRGBAFormat) {
		failed |= check_pixel(img, 0, 0, 0x10, 0x20, 0x30, 0xff, 1);
		failed |= check_pixel(img, 1, 0, 0x40, 0x50, 0x60, 0xff, 1);
	} else {
		failed |= check_pixel(img, 0, 0, 0x10, 0x20, 0x30, 0xff, 0);
		failed |= check_pixel(img, 1, 0, 0x40, 0x50, 0x60, 0xff, 0);
	}
	RReleaseImage(img);

	RDestroyContext(ctx);
	XCloseDisplay(dpy);

	if (failed)
		return 1;

	printf("XPM loader: transparent entries keep zeroed RGB, opaque entries exact\n");
	return 0;
}
