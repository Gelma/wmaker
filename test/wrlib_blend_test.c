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

	return 0;
}
