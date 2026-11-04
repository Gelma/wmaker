/* wrlib_scale_bench.c - timing and bit-exactness probe for RSmoothScaleImage.
 *
 * Two questions, both answered with numbers instead of guesses:
 *
 *   1. what does a whole RSmoothScaleImage() call cost at the sizes the WM
 *      actually asks for (window mini previews, switch-panel thumbnails,
 *      wallpaper);
 *   2. is the horizontal filter pass - the one the roadmap proposes to
 *      vectorize - worth a hand-written kernel?  The scalar loop is
 *      reproduced here exactly as scale.c writes it, and compared against a
 *      row-parallel AVX2 kernel that keeps the per-lane accumulation order
 *      identical (so the output can be compared byte by byte).
 *
 * The kernel is a probe, not library code: nothing in wrlib changes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <immintrin.h>

#include <wraster.h>

typedef struct {
	int pixel;
	double weight;
} Contrib;

typedef struct {
	int n;
	Contrib *p;
} CList;

#define CatmullRom_support (2.0)
#define CLAMP(v, l, h) ((v) < (l) ? (l) : (v) > (h) ? (h) : (v))

static double catmullrom(double t)
{
	double tt;

	tt = t * t;
	if (t < 0)
		t = -t;
	if (t < 1.0) {
		return ((1.5 * t * tt) - (2.5 * tt) + 1.0);
	} else if (t < 2.0) {
		return ((-0.5 * t * tt) + (2.5 * tt) - (4.0 * t) + 2.0);
	}
	return (0.0);
}

static double now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static void fill(unsigned char *p, size_t n, unsigned int seed)
{
	size_t i;

	for (i = 0; i < n; i++) {
		seed ^= seed << 13;
		seed ^= seed >> 17;
		seed ^= seed << 5;
		p[i] = (unsigned char)(seed >> 7);
	}
}

/* Same construction as the xscale < 1.0 branch of RSmoothScaleImage(). */
static CList *build_contribs(unsigned src_width, unsigned new_width, int sch)
{
	double xscale = (double)new_width / (double)src_width;
	double width = CatmullRom_support / xscale;
	double fscale = 1.0 / xscale;
	CList *contrib = calloc(new_width, sizeof(CList));
	unsigned i;

	if (!contrib)
		return NULL;

	for (i = 0; i < new_width; i++) {
		double center = (double)i / xscale;
		double left = ceil(center - width);
		double right = floor(center + width);
		double j;

		contrib[i].p = calloc((int)ceil(width * 2 + 1), sizeof(Contrib));
		if (!contrib[i].p)
			return NULL;

		for (j = left; j <= right; j++) {
			double w = catmullrom((center - j) / fscale) / fscale;
			int n;

			if (j < 0)
				n = -j;
			else if (j >= src_width)
				n = (src_width - j) + src_width - 1;
			else
				n = (int)j;
			if (n < 0)
				n = 0;
			else if (n >= src_width)
				n = src_width - 1;

			contrib[i].p[contrib[i].n].pixel = n * sch;
			contrib[i].p[contrib[i].n].weight = w;
			contrib[i].n++;
		}
	}
	return contrib;
}

/* The scalar loop of scale.c, verbatim in shape. */
static void horiz_scalar(unsigned char *dst, const unsigned char *src,
                         unsigned src_width, unsigned rows, CList *contrib,
                         unsigned new_width, int sch)
{
	unsigned char *p = dst;
	unsigned k, i;
	int j;

	for (k = 0; k < rows; k++) {
		const unsigned char *sp = src + (size_t)src_width * k * sch;

		for (i = 0; i < new_width; i++) {
			double rw = 0.0, gw = 0.0, bw = 0.0;
			const Contrib *pp = contrib[i].p;

			for (j = 0; j < contrib[i].n; j++) {
				rw += sp[pp[j].pixel] * pp[j].weight;
				gw += sp[pp[j].pixel + 1] * pp[j].weight;
				bw += sp[pp[j].pixel + 2] * pp[j].weight;
			}
			*p++ = CLAMP(rw, 0, 255);
			*p++ = CLAMP(gw, 0, 255);
			*p++ = CLAMP(bw, 0, 255);
		}
	}
}

/* Row-parallel kernel: four rows per iteration, one lane each.
 *
 * The contributions (pixel index and weight) of an output column are the
 * same for every row, so lanes only differ in the source pointer.  Each
 * lane accumulates its j in the same order as the scalar loop, which is
 * what keeps the result comparable byte by byte.
 */
static void horiz_simd(unsigned char *dst, const unsigned char *src,
                       unsigned src_width, unsigned rows, CList *contrib,
                       unsigned new_width, int sch)
{
	unsigned k = 0;

	for (; k + 4 <= rows; k += 4) {
		const unsigned char *r0 = src + (size_t)src_width * k * sch;
		const unsigned char *r1 = r0 + (size_t)src_width * sch;
		const unsigned char *r2 = r1 + (size_t)src_width * sch;
		const unsigned char *r3 = r2 + (size_t)src_width * sch;
		unsigned char *out = dst + (size_t)k * new_width * 3;
		unsigned i;

		for (i = 0; i < new_width; i++) {
			const Contrib *pp = contrib[i].p;
			int n = contrib[i].n;
			__m256d ar = _mm256_setzero_pd();
			__m256d ag = _mm256_setzero_pd();
			__m256d ab = _mm256_setzero_pd();
			int j;

			for (j = 0; j < n; j++) {
				int o = pp[j].pixel;
				__m256d w = _mm256_set1_pd(pp[j].weight);

				ar = _mm256_add_pd(ar, _mm256_mul_pd(w, _mm256_set_pd((double)r3[o], (double)r2[o], (double)r1[o], (double)r0[o])));
				ag = _mm256_add_pd(ag, _mm256_mul_pd(w, _mm256_set_pd((double)r3[o + 1], (double)r2[o + 1], (double)r1[o + 1], (double)r0[o + 1])));
				ab = _mm256_add_pd(ab, _mm256_mul_pd(w, _mm256_set_pd((double)r3[o + 2], (double)r2[o + 2], (double)r1[o + 2], (double)r0[o + 2])));
			}

			{
				double vr[4], vg[4], vb[4];

				_mm256_storeu_pd(vr, ar);
				_mm256_storeu_pd(vg, ag);
				_mm256_storeu_pd(vb, ab);

				for (j = 0; j < 4; j++) {
					out[(size_t)j * new_width * 3 + i * 3] = CLAMP(vr[j], 0, 255);
					out[(size_t)j * new_width * 3 + i * 3 + 1] = CLAMP(vg[j], 0, 255);
					out[(size_t)j * new_width * 3 + i * 3 + 2] = CLAMP(vb[j], 0, 255);
				}
			}
		}
	}

	if (k < rows)
		horiz_scalar(dst + (size_t)k * new_width * 3, src + (size_t)src_width * k * sch,
		             src_width, rows - k, contrib, new_width, sch);
}

/* Same idea with eight rows and two independent accumulator chains, to see
 * whether the 4-row version is limited by loop overhead or by the loads. */
static void horiz_simd8(unsigned char *dst, const unsigned char *src,
                        unsigned src_width, unsigned rows, CList *contrib,
                        unsigned new_width, int sch)
{
	unsigned k = 0;

	for (; k + 8 <= rows; k += 8) {
		const unsigned char *r0 = src + (size_t)src_width * k * sch;
		const unsigned char *r1 = r0 + (size_t)src_width * sch;
		const unsigned char *r2 = r1 + (size_t)src_width * sch;
		const unsigned char *r3 = r2 + (size_t)src_width * sch;
		const unsigned char *r4 = r3 + (size_t)src_width * sch;
		const unsigned char *r5 = r4 + (size_t)src_width * sch;
		const unsigned char *r6 = r5 + (size_t)src_width * sch;
		const unsigned char *r7 = r6 + (size_t)src_width * sch;
		unsigned char *out = dst + (size_t)k * new_width * 3;
		unsigned i;
		int j;

		for (i = 0; i < new_width; i++) {
			const Contrib *pp = contrib[i].p;
			int n = contrib[i].n;
			__m256d ar0 = _mm256_setzero_pd(), ag0 = ar0, ab0 = ar0;
			__m256d ar1 = ar0, ag1 = ar0, ab1 = ar0;

			for (j = 0; j < n; j++) {
				int o = pp[j].pixel;
				__m256d w = _mm256_set1_pd(pp[j].weight);

				ar0 = _mm256_add_pd(ar0, _mm256_mul_pd(w, _mm256_set_pd((double)r3[o], (double)r2[o], (double)r1[o], (double)r0[o])));
				ag0 = _mm256_add_pd(ag0, _mm256_mul_pd(w, _mm256_set_pd((double)r3[o + 1], (double)r2[o + 1], (double)r1[o + 1], (double)r0[o + 1])));
				ab0 = _mm256_add_pd(ab0, _mm256_mul_pd(w, _mm256_set_pd((double)r3[o + 2], (double)r2[o + 2], (double)r1[o + 2], (double)r0[o + 2])));
				ar1 = _mm256_add_pd(ar1, _mm256_mul_pd(w, _mm256_set_pd((double)r7[o], (double)r6[o], (double)r5[o], (double)r4[o])));
				ag1 = _mm256_add_pd(ag1, _mm256_mul_pd(w, _mm256_set_pd((double)r7[o + 1], (double)r6[o + 1], (double)r5[o + 1], (double)r4[o + 1])));
				ab1 = _mm256_add_pd(ab1, _mm256_mul_pd(w, _mm256_set_pd((double)r7[o + 2], (double)r6[o + 2], (double)r5[o + 2], (double)r4[o + 2])));
			}

			{
				double vr0[4], vg0[4], vb0[4], vr1[4], vg1[4], vb1[4];
				unsigned lane;

				_mm256_storeu_pd(vr0, ar0);
				_mm256_storeu_pd(vg0, ag0);
				_mm256_storeu_pd(vb0, ab0);
				_mm256_storeu_pd(vr1, ar1);
				_mm256_storeu_pd(vg1, ag1);
				_mm256_storeu_pd(vb1, ab1);

				for (lane = 0; lane < 4; lane++) {
					out[(size_t)lane * new_width * 3 + i * 3] = CLAMP(vr0[lane], 0, 255);
					out[(size_t)lane * new_width * 3 + i * 3 + 1] = CLAMP(vg0[lane], 0, 255);
					out[(size_t)lane * new_width * 3 + i * 3 + 2] = CLAMP(vb0[lane], 0, 255);
					out[(size_t)(lane + 4) * new_width * 3 + i * 3] = CLAMP(vr1[lane], 0, 255);
					out[(size_t)(lane + 4) * new_width * 3 + i * 3 + 1] = CLAMP(vg1[lane], 0, 255);
					out[(size_t)(lane + 4) * new_width * 3 + i * 3 + 2] = CLAMP(vb1[lane], 0, 255);
				}
			}
		}
	}

	if (k < rows)
		horiz_scalar(dst + (size_t)k * new_width * 3, src + (size_t)src_width * k * sch,
		             src_width, rows - k, contrib, new_width, sch);
}

static void bench_call(const char *label, unsigned sw, unsigned sh, unsigned dw, unsigned dh)
{
	RImage *src = RCreateImage(sw, sh, 0);
	int iters = 5, i;
	double best = 1e9;

	if (!src) {
		fprintf(stderr, "cannot create source image\n");
		exit(1);
	}
	fill(src->data, (size_t)sw * sh * 3, 0x9e3779b9u);

	for (i = 0; i < iters; i++) {
		double t0 = now_ms();
		RImage *out = RSmoothScaleImage(src, dw, dh);
		double t1 = now_ms();

		if (!out) {
			fprintf(stderr, "RSmoothScaleImage failed\n");
			exit(1);
		}
		if (t1 - t0 < best)
			best = t1 - t0;
		RReleaseImage(out);
	}

	printf("%-28s %4ux%-5u -> %4ux%-5u  best %7.2f ms\n", label, sw, sh, dw, dh, best);
	RReleaseImage(src);
}

static void bench_kernel(unsigned sw, unsigned sh, unsigned dw)
{
	CList *contrib = build_contribs(sw, dw, 3);
	unsigned char *src, *a, *b;
	double t0, t1, ts, t4, t8;
	size_t srcbytes = (size_t)sw * sh * 3;
	size_t dstbytes = (size_t)dw * sh * 3;
	int i, iters = 30;
	long diff4 = 0, diff8 = 0;

	if (!contrib) {
		fprintf(stderr, "cannot build contributions\n");
		exit(1);
	}
	src = malloc(srcbytes);
	a = malloc(dstbytes);
	b = malloc(dstbytes);
	if (!src || !a || !b) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	fill(src, srcbytes, 0x1234567u);

	t0 = now_ms();
	for (i = 0; i < iters; i++)
		horiz_scalar(a, src, sw, sh, contrib, dw, 3);
	t1 = now_ms();
	ts = (t1 - t0) / iters;

	horiz_simd(b, src, sw, sh, contrib, dw, 3);
	for (i = 0; i < (int)dstbytes; i++)
		if (a[i] != b[i])
			diff4++;

	t0 = now_ms();
	for (i = 0; i < iters; i++)
		horiz_simd(b, src, sw, sh, contrib, dw, 3);
	t1 = now_ms();
	t4 = (t1 - t0) / iters;

	horiz_simd8(b, src, sw, sh, contrib, dw, 3);
	for (i = 0; i < (int)dstbytes; i++)
		if (a[i] != b[i])
			diff8++;

	t0 = now_ms();
	for (i = 0; i < iters; i++)
		horiz_simd8(b, src, sw, sh, contrib, dw, 3);
	t1 = now_ms();
	t8 = (t1 - t0) / iters;

	printf("horizontal pass %ux%u -> %ux%u (%d contributions/px)\n",
	       sw, sh, dw, sh, contrib[0].n);
	printf("  scalar        %7.2f ms/iter\n", ts);
	printf("  4 rows/AVX2   %7.2f ms/iter  (%.2fx, %ld bytes differ)\n", t4, ts / t4, diff4);
	printf("  8 rows/AVX2   %7.2f ms/iter  (%.2fx, %ld bytes differ)\n", t8, ts / t8, diff8);

	free(src);
	free(a);
	free(b);
	for (i = 0; i < (int)dw; i++)
		free(contrib[i].p);
	free(contrib);
}

int main(void)
{
	printf("--- RSmoothScaleImage, whole call ---\n");
	/* window mini preview: a 400x300 snapshot down to an icon-sized strip */
	bench_call("mini preview", 400, 300, 64, 48);
	/* switch panel: a full desktop down to a thumbnail */
	bench_call("switch panel", 1920, 1080, 300, 170);
	/* workspace map / screen.c: width/10 */
	bench_call("workspace map", 1920, 1080, 192, 108);
	/* WPrefs configuration thumbnail */
	bench_call("wprefs thumbnail", 128, 128, 61, 61);
	/* wallpaper upscale */
	bench_call("wallpaper upscale", 1920, 1080, 2560, 1440);

	printf("\n--- horizontal filter pass, kernel probe ---\n");
	bench_kernel(1920, 1080, 192);
	bench_kernel(400, 300, 64);

	return 0;
}
