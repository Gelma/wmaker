/* coverity-model.c - Coverity modeling file for the WINGs allocation layer.
 *
 * This file is NOT part of the build: it is parsed by Coverity's own front
 * end (the compiler defines __COVERITY__) and loaded as a modeling file in
 * the analysis settings.  The #else branch exists only so that a normal
 * compiler can syntax-check the signatures:
 *
 *     gcc -fsyntax-only -Wall -Wextra tools/coverity-model.c
 *
 * What it claims, and why it is true in this tree:
 *
 *   wmalloc(n)      returns a fresh block of n bytes (WINGs/memory.c:89).
 *   wstrdup(s)      returns a fresh copy of s, length unknown here.
 *   wstrndup(s, n)  returns a fresh block of at most n+1 bytes.
 *   wfree(p)        releases p (WINGs/memory.c:215; NULL is accepted).
 *
 * What it deliberately does NOT model, and why a "more complete" model
 * would be worse than no model at all:
 *
 *   - wrealloc(p, n): a model can only say "frees p and allocates" or
 *     "allocates and keeps p".  The first invents a use-after-free on the
 *     failure path (realloc keeps the block), the second invents a leak on
 *     every successful call.  Coverity's own heuristic is no worse, so the
 *     function is left out on purpose.
 *
 *   - WMReleasePropList(), RReleaseImage(), WMDestroyWidget(): reference
 *     counted.  They free only when the count reaches zero, so modelling
 *     them as __coverity_free__() would produce a use-after-free report for
 *     every released-but-still-referenced object.  Do not add them.
 *
 *   - WMCreatePLArray() and the other variadic factories: the interesting
 *     property is that they *retain* their arguments (a leak when the caller
 *     uses them inline), and a model cannot express "keeps a reference"
 *     without also hiding the real leak.  The code was fixed instead: see
 *     CoverityDeepseek.md, section 3.2.
 *
 *   - Exit(), wAbort(), EventLoop(): already declared noreturn in
 *     src/main.h and src/event.h, so the analyzer knows.
 *
 * How to read the result: run one analysis with the file and one without,
 * and compare the per-checker counts.  The expected effect is small (the
 * checker already infers allocation from the names); anything that appears
 * only with the model is a modelling error, not a new defect.  If the model
 * changes nothing, drop it: an empty win is better than a file that has to be
 * maintained against the headers.
 *
 * unsigned long is size_t on the LP64 targets this tree is analysed on; if a
 * 32-bit analysis is ever added, change these signatures with it.
 */

#if defined(__COVERITY__)

void *wmalloc(unsigned long size)
{
	return __coverity_alloc__(size);
}

char *wstrdup(const char *str)
{
	(void)str;
	return __coverity_alloc_nosize__();
}

char *wstrndup(const char *str, unsigned long len)
{
	(void)str;
	return __coverity_alloc__(len + 1);
}

void wfree(void *ptr)
{
	__coverity_free__(ptr);
}

#else /* !__COVERITY__ */

#include <stddef.h>

/* Declarations only, so that a normal compiler can check that the signatures
 * above match WINGs/WINGs/WUtil.h. */
void *wmalloc(size_t size);
char *wstrdup(const char *str);
char *wstrndup(const char *str, size_t len);
void wfree(void *ptr);

#endif /* !__COVERITY__ */
