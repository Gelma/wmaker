/* wrlib_blend_test.c - regression test for the RGB blend arithmetic.
 *
 * RCombineImagesWithOpaqueness blends two RGB images with
 *     d = (d * (255 - op) + s * op) / 256
 * per byte.  The scalar loops are auto-vectorized by the compiler, so this
 * test pins the exact arithmetic against a local reference implementation:
 * any future change to the loop (SIMD or otherwise) must stay bit-identical.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wraster.h>

#define WIDTH  960
#define HEIGHT 540

static void fill(RImage * image, unsigned int seed)
{
	size_t i, n = (size_t)image->width * image->height * 3;
	unsigned char *p = image->data;

	for (i = 0; i < n; i++) {
		seed ^= seed << 13;
		seed ^= seed >> 17;
		seed ^= seed << 5;
		p[i] = (unsigned char)(seed >> 7);
	}
}

/* Uniform image: the 1 2 1 / 1 1 1 kernel averages to the same value, so a
 * blur must leave every pixel untouched. */
static int blur_uniform(int alpha)
{
	unsigned char pattern[4] = { 40, 90, 170, 200 };
	unsigned chars = (unsigned)(alpha ? 4 : 3);
	RImage *img;
	unsigned char *copy;
	size_t i, n;
	int rc;

	img = RCreateImage(16, 16, alpha);
	if (!img) {
		fprintf(stderr, "cannot create blur test image\n");
		return 1;
	}
	n = (size_t)16 * 16 * chars;
	for (i = 0; i < 16 * 16; i++)
		memcpy(img->data + i * chars, pattern, chars);

	copy = malloc(n);
	if (!copy) {
		fprintf(stderr, "out of memory\n");
		RReleaseImage(img);
		return 1;
	}
	memcpy(copy, img->data, n);

	rc = RBlurImage(img);
	if (!rc) {
		fprintf(stderr, "RBlurImage failed\n");
		free(copy);
		RReleaseImage(img);
		return 1;
	}
	for (i = 0; i < n; i++) {
		if (img->data[i] != copy[i]) {
			fprintf(stderr, "blur changed byte %zu of a uniform %s image: %u\n",
			        i, alpha ? "RGBA" : "RGB", img->data[i]);
			free(copy);
			RReleaseImage(img);
			return 1;
		}
	}

	free(copy);
	RReleaseImage(img);
	return 0;
}

int main(void)
{
	RImage *dst, *src;
	unsigned char *expect;
	size_t i, n;
	int op;

	dst = RCreateImage(WIDTH, HEIGHT, 0);
	src = RCreateImage(WIDTH, HEIGHT, 0);
	if (!dst || !src) {
		fprintf(stderr, "cannot create test images\n");
		return 1;
	}
	n = (size_t)WIDTH * HEIGHT * 3;

	expect = malloc(n);
	if (!expect) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	/* Every weight, including the extremes. */
	for (op = 0; op <= 255; op++) {
		fill(dst, 0x1234u + (unsigned)op);
		fill(src, 0x5678u + (unsigned)op);

		for (i = 0; i < n; i++)
			expect[i] = (unsigned char)((((int)dst->data[i] * (255 - op)) +
			                             ((int)src->data[i] * op)) / 256);

		RCombineImagesWithOpaqueness(dst, src, op);

		if (memcmp(dst->data, expect, n) != 0) {
			for (i = 0; i < n; i++)
				if (dst->data[i] != expect[i])
					break;
			fprintf(stderr, "mismatch at byte %zu (opaqueness %d): got %u, expected %u\n",
			        i, op, dst->data[i], expect[i]);
			return 1;
		}
	}

	printf("RCombineImagesWithOpaqueness: bit-exact for opaqueness 0..255\n");

	RReleaseImage(dst);
	RReleaseImage(src);
	free(expect);

	/* RBlurImage() is public API with no caller in the tree, and cppcheck's
	 * sibling (clang-analyzer) flags its border arithmetic: run it once on a
	 * uniform image of each depth, where the 3x3 kernel must be a no-op.
	 * The value is both the invariance check and the memory access pattern
	 * (this is the test that runs under the sanitizer build). */
	if (blur_uniform(0) != 0 || blur_uniform(1) != 0)
		return 1;
	printf("RBlurImage: uniform image unchanged, RGB and RGBA\n");

	return 0;
}
