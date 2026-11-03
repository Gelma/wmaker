
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "wconfig.h"

#include "WINGsP.h"

#include <wraster.h>
#include <assert.h>
#include <X11/Xlocale.h>

#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>

#ifdef USE_PANGO
#include <pango/pango.h>
#include <pango/pangofc-fontmap.h>
#include <pango/pangoxft.h>
#endif

#define FONT_WIDTH_CACHE_SIZE 65536
#define FONT_WIDTH_CACHE_MASK (FONT_WIDTH_CACHE_SIZE - 1)
#define FONT_INLINE_STR_LEN 64

typedef struct WFontWidthCacheEntry {
	WMFont *font;
	uint32_t hash;
	int length;
	int width;
	char text[FONT_INLINE_STR_LEN];
	char *dyn_text;
} WFontWidthCacheEntry;

static WFontWidthCacheEntry width_cache[FONT_WIDTH_CACHE_SIZE];

#ifdef USE_PANGO
#define FONT_LAYOUT_CACHE_SIZE 2048
#define FONT_LAYOUT_CACHE_MASK (FONT_LAYOUT_CACHE_SIZE - 1)

typedef struct WFontLayoutCacheEntry {
	WMFont *font;
	uint32_t hash;
	int length;
	char text[FONT_INLINE_STR_LEN];
	char *dyn_text;
	PangoLayout *layout;
} WFontLayoutCacheEntry;

static WFontLayoutCacheEntry layout_cache[FONT_LAYOUT_CACHE_SIZE];
#endif

static inline uint32_t font_str_hash(const WMFont *font, const char *text, int len)
{
	uint32_t h = (uint32_t)(uintptr_t)font;
	int i;

	h ^= (h >> 16);
	h = h * 0x85ebca6b + (uint32_t)len;
	for (i = 0; i < len; i++) {
		h = (h * 31) ^ (unsigned char)text[i];
	}
	return h;
}

#define DEFAULT_FONT "sans serif:pixelsize=12"

#define DEFAULT_SIZE WINGsConfiguration.defaultFontSize

static FcPattern *xlfdToFcPattern(const char *xlfd)
{
	FcPattern *pattern;
	char *fname, *ptr;

	/* Just skip old font names that contain %d in them.
	 * We don't support that anymore. */
	if (strchr(xlfd, '%') != NULL)
		return FcNameParse((FcChar8 *) DEFAULT_FONT);

	fname = wstrdup(xlfd);
	if ((ptr = strchr(fname, ','))) {
		*ptr = 0;
	}
	pattern = XftXlfdParse(fname, False, False);
	wfree(fname);

	if (!pattern) {
		wwarning(_("invalid font: %s. Trying '%s'"), xlfd, DEFAULT_FONT);
		pattern = FcNameParse((FcChar8 *) DEFAULT_FONT);
	}

	return pattern;
}

static char *xlfdToFcName(const char *xlfd)
{
	FcPattern *pattern;
	char *fname;

	pattern = xlfdToFcPattern(xlfd);
	fname = (char *)FcNameUnparse(pattern);
	FcPatternDestroy(pattern);

	return fname;
}

static Bool hasProperty(FcPattern * pattern, const char *property)
{
	FcValue val;

	if (FcPatternGet(pattern, property, 0, &val) == FcResultMatch) {
		return True;
	}

	return False;
}

static char *makeFontOfSize(const char *font, int size, const char *fallback)
{
	FcPattern *pattern = NULL;
	char *result;

	if (font && font[0] == '-') {
		pattern = xlfdToFcPattern(font);
	} else {
		pattern = FcNameParse((const FcChar8 *) font);
	}

	if (!pattern) {
		wwarning(_("could not load font spec: %s."), font);
		if (!fallback)
			return NULL;
		pattern = FcPatternCreate();
		if (!pattern)
			return NULL;
		if (!FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *) fallback)) {
			wfatal(_("could not load default font spec: %s."), fallback);
			FcPatternDestroy(pattern);
			return NULL;
		}
	}

	if (size > 0) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, (double)size);
	} else if (size == 0 && !hasProperty(pattern, "size") && !hasProperty(pattern, FC_PIXEL_SIZE)) {
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, (double)DEFAULT_SIZE);
	}

	result = (char *)FcNameUnparse(pattern);
	FcPatternDestroy(pattern);

	return result;
}

WMFont *WMCreateFont(WMScreen * scrPtr, const char *fontName)
{
	Display *display = scrPtr->display;
	WMFont *font;
	char *fname;
	int i;
#ifdef USE_PANGO
	PangoFontMap *fontmap;
	PangoContext *context;
	PangoLayout *layout;
	FcPattern *pattern;
	PangoFontDescription *description;
	double size;
#endif

	if (fontName && fontName[0] == '-') {
		fname = xlfdToFcName(fontName);
	} else {
		fname = wstrdup(fontName);
	}

	if (!WINGsConfiguration.antialiasedText && !strstr(fname, ":antialias=")) {
		fname = wstrappend(fname, ":antialias=false");
	}

	font = WMHashGet(scrPtr->fontCache, fname);
	if (font) {
		WMRetainFont(font);
		wfree(fname);
		return font;
	}

	font = wmalloc(sizeof(WMFont));

	font->screen = scrPtr;

	font->font = XftFontOpenName(display, scrPtr->screen, fname);
	if (!font->font) {
		wfree(font);
		wfree(fname);
		return NULL;
	}

	font->height = font->font->ascent + font->font->descent;
	font->y = font->font->ascent;

	for (i = 0; i < 256; i++)
		font->ascii_widths[i] = -1;

	font->refCount = 1;

	font->name = fname;

#ifdef USE_PANGO
	fontmap = pango_xft_get_font_map(scrPtr->display, scrPtr->screen);
	context = pango_font_map_create_context(fontmap);
	layout = pango_layout_new(context);

	pattern = FcNameParse((FcChar8 *) font->name);
	description = pango_fc_font_description_from_pattern(pattern, FALSE);

	/* Pango examines FC_SIZE but not FC_PIXEL_SIZE of the patten, but
	 * font-name has only "pixelsize", so set the size manually here.
	 */
	if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &size) == FcResultMatch)
		pango_font_description_set_absolute_size(description, size * PANGO_SCALE);

	pango_layout_set_font_description(layout, description);

	font->layout = layout;

	pango_font_description_free(description);
	g_object_unref(context);
	FcPatternDestroy(pattern);
#endif

	assert(WMHashInsert(scrPtr->fontCache, font->name, font) == NULL);

	return font;
}

WMFont *WMRetainFont(WMFont * font)
{
	wassertrv(font != NULL, NULL);

	font->refCount++;

	return font;
}

void WMReleaseFont(WMFont * font)
{
	int i;

	wassertr(font != NULL);

	font->refCount--;
	if (font->refCount < 1) {
		for (i = 0; i < FONT_WIDTH_CACHE_SIZE; i++) {
			if (width_cache[i].font == font) {
				if (width_cache[i].dyn_text) {
					wfree(width_cache[i].dyn_text);
					width_cache[i].dyn_text = NULL;
				}
				width_cache[i].font = NULL;
			}
		}
#ifdef USE_PANGO
		for (i = 0; i < FONT_LAYOUT_CACHE_SIZE; i++) {
			if (layout_cache[i].font == font) {
				if (layout_cache[i].layout) {
					g_object_unref(layout_cache[i].layout);
					layout_cache[i].layout = NULL;
				}
				if (layout_cache[i].dyn_text) {
					wfree(layout_cache[i].dyn_text);
					layout_cache[i].dyn_text = NULL;
				}
				layout_cache[i].font = NULL;
			}
		}
#endif
		XftFontClose(font->screen->display, font->font);
#ifdef USE_PANGO
		if (font->layout) {
			g_object_unref(font->layout);
		}
#endif
		if (font->name) {
			WMHashRemove(font->screen->fontCache, font->name);
			wfree(font->name);
		}
		wfree(font);
	}
}

Bool WMIsAntialiasingEnabled(WMScreen * scrPtr)
{
	return scrPtr->antialiasedText;
}

unsigned int WMFontHeight(WMFont * font)
{
	wassertrv(font != NULL, 0);

	return font->height;
}

char *WMGetFontName(WMFont * font)
{
	wassertrv(font != NULL, NULL);

	return font->name;
}

void WMGetScaleBaseFromSystemFont(WMScreen *scrPtr, int *alphabetWidth, int *fontHeight)
{
	WMFont *font;

	font = WMDefaultSystemFont(scrPtr);
	*alphabetWidth = WMWidthOfString(font, "abcdefghijklmnopqrstuvwxyz", 26);
	*fontHeight = WMFontHeight(font);
	WMReleaseFont(font);
}

WMFont *WMDefaultSystemFont(WMScreen * scrPtr)
{
	return WMRetainFont(scrPtr->normalFont);
}

WMFont *WMDefaultBoldSystemFont(WMScreen * scrPtr)
{
	return WMRetainFont(scrPtr->boldFont);
}

WMFont *WMSystemFontOfSize(WMScreen * scrPtr, int size)
{
	WMFont *font;
	char *fontSpec;

	fontSpec = makeFontOfSize(WINGsConfiguration.systemFont, size, DEFAULT_FONT);

	if (!fontSpec) {
		return NULL;
	}

	font = WMCreateFont(scrPtr, fontSpec);

	if (!font) {
		wwarning(_("could not load font: %s."), fontSpec);
	}

	wfree(fontSpec);

	return font;
}

WMFont *WMBoldSystemFontOfSize(WMScreen * scrPtr, int size)
{
	WMFont *font;
	char *fontSpec;

	fontSpec = makeFontOfSize(WINGsConfiguration.boldSystemFont, size, DEFAULT_FONT);

	if (!fontSpec) {
		return NULL;
	}

	font = WMCreateFont(scrPtr, fontSpec);

	if (!font) {
		wwarning(_("could not load font: %s."), fontSpec);
	}

	wfree(fontSpec);

	return font;
}

#ifdef USE_PANGO
static PangoLayout *get_or_create_cached_layout(WMFont *font, const char *text, int length)
{
	uint32_t h;
	uint32_t idx;
	WFontLayoutCacheEntry *entry;

	h = font_str_hash(font, text, length);
	idx = h & FONT_LAYOUT_CACHE_MASK;
	entry = &layout_cache[idx];

	if (entry->font == font && entry->hash == h && entry->length == length) {
		const char *cached = entry->dyn_text ? entry->dyn_text : entry->text;
		if (memcmp(cached, text, length) == 0) {
			return entry->layout;
		}
	}

	/* Evict old entry if present */
	if (entry->layout) {
		g_object_unref(entry->layout);
		entry->layout = NULL;
	}
	if (entry->dyn_text) {
		wfree(entry->dyn_text);
		entry->dyn_text = NULL;
	}

	entry->font = font;
	entry->hash = h;
	entry->length = length;
	if (length < FONT_INLINE_STR_LEN) {
		memcpy(entry->text, text, length);
		entry->text[length] = '\0';
	} else {
		entry->dyn_text = wmalloc(length + 1);
		memcpy(entry->dyn_text, text, length);
		entry->dyn_text[length] = '\0';
	}

	entry->layout = pango_layout_copy(font->layout);
	pango_layout_set_text(entry->layout, text, length);

	return entry->layout;
}
#endif

int WMWidthOfString(WMFont * font, const char *text, int length)
{
#ifdef USE_PANGO
	const char *previous_text;
#else
	XGlyphInfo extents;
#endif
	int width;
	uint32_t h, idx;
	WFontWidthCacheEntry *entry;

	wassertrv(font != NULL && text != NULL, 0);

	if (length <= 0)
		return 0;

	/* Tier 1: Single ASCII character fast path */
	if (length == 1) {
		unsigned char c = (unsigned char)text[0];
		if (font->ascii_widths[c] >= 0)
			return font->ascii_widths[c];
	}

	/* Tier 2: Hash cache lookup for strings */
	h = font_str_hash(font, text, length);
	idx = h & FONT_WIDTH_CACHE_MASK;
	entry = &width_cache[idx];

	if (entry->font == font && entry->hash == h && entry->length == length) {
		const char *cached = entry->dyn_text ? entry->dyn_text : entry->text;
		if (memcmp(cached, text, length) == 0)
			return entry->width;
	}

	/* Cache miss: compute width via layout/Xft */
#ifdef USE_PANGO
	previous_text = pango_layout_get_text(font->layout);
	if ((previous_text == NULL) || (strncmp(text, previous_text, length) != 0) || previous_text[length] != '\0')
		pango_layout_set_text(font->layout, text, length);
	pango_layout_get_pixel_size(font->layout, &width, NULL);
#else
	XftTextExtentsUtf8(font->screen->display, font->font, (XftChar8 *) text, length, &extents);
	width = extents.xOff;
#endif

	/* If single char, populate Tier 1 */
	if (length == 1) {
		unsigned char c = (unsigned char)text[0];
		font->ascii_widths[c] = (short)width;
	}

	/* Populate Tier 2 cache slot */
	if (entry->dyn_text) {
		wfree(entry->dyn_text);
		entry->dyn_text = NULL;
	}
	entry->font = font;
	entry->hash = h;
	entry->length = length;
	entry->width = width;
	if (length < FONT_INLINE_STR_LEN) {
		memcpy(entry->text, text, length);
		entry->text[length] = '\0';
	} else {
		entry->dyn_text = wmalloc(length + 1);
		memcpy(entry->dyn_text, text, length);
		entry->dyn_text[length] = '\0';
	}

	return width;
}

void WMDrawString(WMScreen * scr, Drawable d, WMColor * color, WMFont * font, int x, int y, const char *text, int length)
{
	XftColor xftcolor;

	wassertr(font != NULL && text != NULL);
	if (length <= 0)
		return;

	xftcolor.color.red = color->color.red;
	xftcolor.color.green = color->color.green;
	xftcolor.color.blue = color->color.blue;
	xftcolor.color.alpha = color->alpha;
	xftcolor.pixel = W_PIXEL(color);

	XftDrawChange(scr->xftdraw, d);

#ifdef USE_PANGO
	{
		PangoLayout *layout = get_or_create_cached_layout(font, text, length);
		pango_xft_render_layout(scr->xftdraw, &xftcolor, layout, x * PANGO_SCALE, y * PANGO_SCALE);
	}
#else
	XftDrawStringUtf8(scr->xftdraw, &xftcolor, font->font, x, y + font->y, (XftChar8 *) text, length);
#endif
}

void
WMDrawImageString(WMScreen * scr, Drawable d, WMColor * color, WMColor * background,
		  WMFont * font, int x, int y, const char *text, int length)
{
	XftColor textColor;
	XftColor bgColor;

	wassertr(font != NULL && text != NULL);
	if (length <= 0)
		return;

	textColor.color.red = color->color.red;
	textColor.color.green = color->color.green;
	textColor.color.blue = color->color.blue;
	textColor.color.alpha = color->alpha;
	textColor.pixel = W_PIXEL(color);

	bgColor.color.red = background->color.red;
	bgColor.color.green = background->color.green;
	bgColor.color.blue = background->color.blue;
	bgColor.color.alpha = background->alpha;
	bgColor.pixel = W_PIXEL(background);

	XftDrawChange(scr->xftdraw, d);

	XftDrawRect(scr->xftdraw, &bgColor, x, y, WMWidthOfString(font, text, length), font->height);

#ifdef USE_PANGO
	{
		PangoLayout *layout = get_or_create_cached_layout(font, text, length);
		pango_xft_render_layout(scr->xftdraw, &textColor, layout, x * PANGO_SCALE, y * PANGO_SCALE);
	}
#else
	XftDrawStringUtf8(scr->xftdraw, &textColor, font->font, x, y + font->y, (XftChar8 *) text, length);
#endif
}

WMFont *WMCopyFontWithStyle(WMScreen * scrPtr, WMFont * font, WMFontStyle style)
{
	FcPattern *pattern;
	WMFont *copy;
	char *name;

	if (!font)
		return NULL;

	/* It's enough to add italic to slant, even if the font has no italic
	 * variant, but only oblique. This is because fontconfig will actually
	 * return the closest match font to what we requested which is the
	 * oblique font. Same goes for using bold for weight.
	 */
	pattern = FcNameParse((FcChar8 *) WMGetFontName(font));
	switch (style) {
	case WFSNormal:
		FcPatternDel(pattern, FC_WEIGHT);
		FcPatternDel(pattern, FC_SLANT);
		break;
	case WFSBold:
		FcPatternDel(pattern, FC_WEIGHT);
		FcPatternAddString(pattern, FC_WEIGHT, (FcChar8 *) "bold");
		break;
	case WFSItalic:
		FcPatternDel(pattern, FC_SLANT);
		FcPatternAddString(pattern, FC_SLANT, (FcChar8 *) "italic");
		break;
	case WFSBoldItalic:
		FcPatternDel(pattern, FC_WEIGHT);
		FcPatternDel(pattern, FC_SLANT);
		FcPatternAddString(pattern, FC_WEIGHT, (FcChar8 *) "bold");
		FcPatternAddString(pattern, FC_SLANT, (FcChar8 *) "italic");
		break;
	}

	name = (char *)FcNameUnparse(pattern);
	copy = WMCreateFont(scrPtr, name);
	FcPatternDestroy(pattern);
	wfree(name);

	return copy;
}

void W_FontShutdown(void)
{
	int i;

	for (i = 0; i < FONT_WIDTH_CACHE_SIZE; i++) {
		if (width_cache[i].dyn_text) {
			wfree(width_cache[i].dyn_text);
			width_cache[i].dyn_text = NULL;
		}
		width_cache[i].font = NULL;
	}
#ifdef USE_PANGO
	for (i = 0; i < FONT_LAYOUT_CACHE_SIZE; i++) {
		if (layout_cache[i].layout) {
			g_object_unref(layout_cache[i].layout);
			layout_cache[i].layout = NULL;
		}
		if (layout_cache[i].dyn_text) {
			wfree(layout_cache[i].dyn_text);
			layout_cache[i].dyn_text = NULL;
		}
		layout_cache[i].font = NULL;
	}
#endif
	FcFini();
}
