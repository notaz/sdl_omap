/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <SDL/SDL.h>

#include "omapsdl.h"
#include "common/input.h"
#include "linux/fbdev.h"
#include "linux/oshide.h"

static SDL_Surface *g_screen;
static void *g_screen_fbp;
static Uint16 g_8bpp_pal[256];
static Uint32 g_start_ticks;

static inline int min(int v1, int v2)
{
	return v1 < v2 ? v1 : v2;
}

static SDL_Surface *alloc_surface(int w, int h, int bpp)
{
	// SDL has some pointer overuse, every surface has a format,
	// why make format accessible through pointer?
	struct {
		SDL_Surface s;
		SDL_PixelFormat f;
	} *ret;

	ret = calloc(1, sizeof(*ret));
	if (ret == NULL)
		return NULL;

	ret->s.format = &ret->f;
	ret->f.BitsPerPixel = bpp;
	ret->f.BytesPerPixel = (bpp + 7) / 8;

	ret->s.w = w;
	ret->s.h = h;
	ret->s.pitch = w * ret->f.BytesPerPixel;
	// XXX: more fields

	return &ret->s;
}

DECLSPEC int SDLCALL
SDL_Init(Uint32 flags)
{
	trace("%08x", flags);

	if (g_start_ticks == 0) {
		omapsdl_input_init();
		oshide_init();
		omapsdl_config();
	}

	g_start_ticks = 0;
	g_start_ticks = SDL_GetTicks();

	return 0;
}

DECLSPEC void SDLCALL
SDL_Quit(void)
{
	trace("");

	if (g_start_ticks != 0) {
		oshide_finish();
		g_start_ticks = 0;
	}
}

DECLSPEC int SDLCALL
SDL_InitSubSystem(Uint32 flags)
{
	trace("%08x", flags);
	return 0;
}

DECLSPEC void SDLCALL
SDL_QuitSubSystem(Uint32 flags)
{
	trace("%08x", flags);
}

DECLSPEC Uint32 SDLCALL
SDL_GetTicks(void)
{
	struct timeval tv;
	Uint32 ret;

	gettimeofday(&tv, NULL);
	ret = tv.tv_sec * 1000;
	ret += tv.tv_usec * 1048 >> 20;

	ret -= g_start_ticks;
	dbg(" SDL_GetTicks %d", ret);
	return ret;
}

DECLSPEC SDL_Surface * SDLCALL
SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags)
{
	struct vout_fbdev *fbdev;

	trace("%d, %d, %d, %08x", width, height, bpp, flags);

	if (bpp == 0)
		bpp = 16;

	if (bpp != 8 && bpp != 16) {
		err("unsupported bpp: %d\n", bpp);
		return NULL;
	}

	// XXX: destroy old g_screen?

	g_screen = alloc_surface(width, height, bpp);
	if (g_screen == NULL)
		return NULL;

	fbdev = vout_fbdev_init("/dev/fb0", &width, &height, 0);
	if (fbdev == NULL)
		goto fail_fbdev_init;

	g_screen_fbp = vout_fbdev_flip(fbdev);
	if (bpp == 16)
		g_screen->pixels = g_screen_fbp;
	else
		// we have to fake this for now..
		g_screen->pixels = calloc(g_screen->pitch * height, 1);

	if (g_screen->pixels == NULL) {
		err("fb NULL");
		goto fail_pixels;
	}
	g_screen->hwdata = (void *)fbdev;

	dbg("returning %p", g_screen);
	return g_screen;

fail_pixels:
	vout_fbdev_finish(fbdev);
fail_fbdev_init:
	free(g_screen);
	g_screen = NULL;
	return NULL;
}

DECLSPEC int SDLCALL
SDL_Flip(SDL_Surface *screen)
{
	struct vout_fbdev *fbdev;

	trace("%p", screen);

	if (screen != g_screen) {
		err("flip not on screen surface?");
		return -1;
	}

	if (screen->format->BitsPerPixel == 8) {
		int l = screen->pitch * screen->h;
#ifdef __arm__
		do_clut(g_screen_fbp, screen->pixels, g_8bpp_pal, l);
#else
		Uint16 *d = g_screen_fbp;
		Uint8 *s = screen->pixels;

		// XXX: perhaps optimize this sh*t
		for (; l > 0; d++, s++, l--)
			*d = g_8bpp_pal[*s];
#endif
	}

	fbdev = (void *)screen->hwdata;
	g_screen_fbp = vout_fbdev_flip(fbdev);

	if (screen->format->BitsPerPixel != 8)
		screen->pixels = g_screen_fbp;

	return 0;
}

static int do_rect_clip(const SDL_Surface *s, SDL_Rect *r)
{
	int x = r->x, y = r->y, w = r->w, h = r->h;

	if (x < 0) {
		w += x;
		if (w < 0)
			w = 0;
		x = 0;
	}
	if (y < 0) {
		h += y;
		if (h < 0)
			h = 0;
		y = 0;
	}
	if (x + w > s->w) {
		w = s->w - x;
		if (w < 0)
			w = 0;
	}
	if (y + h > s->h) {
		h = s->h - y;
		if (h < 0)
			h = 0;
	}

	r->x = x; r->y = y; r->w = w; r->h = h;
	return (w > 0 && h > 0) ? 0 : -1;
}

DECLSPEC int SDLCALL
SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect,
		SDL_Surface *dst, SDL_Rect *dstrect)
{
	int sx = 0, sy = 0, sw, sh;
	int dx = 0, dy = 0;
	SDL_Rect tmprect = { 0, };

	trace("%p, %p, %p, %p", src, srcrect, dst, dstrect);

	if (src == NULL || dst == NULL)
		return -1;

	if (srcrect != NULL) {
		// XXX: dst pos may need to be adjusted in some corner cases
		if (do_rect_clip(src, srcrect) < 0)
			return -1;
		sx = srcrect->x;
		sy = srcrect->y;
		sw = srcrect->w;
		sh = srcrect->h;
	} else {
		sw = src->w;
		sh = src->h;
	}

	if (dstrect == NULL)
		dstrect = &tmprect;

	// SDL just uses source w and h
	dstrect->w = sw;
	dstrect->h = sh;
	if (do_rect_clip(dst, dstrect) < 0)
		return -1;
	dx = dstrect->x;
	dy = dstrect->y;
	sw = dstrect->w;
	sh = dstrect->h;

	dbg("(%d %d %d %d) -> (%d %d %d %d)", sx, sy, sw, sh, dx, dy, sw, sh);

	if (src->format->BitsPerPixel == dst->format->BitsPerPixel) {
		int Bpp = src->format->BytesPerPixel;
		int dpitch = dst->pitch;
		int spitch = src->pitch;
		Uint8 *d = (Uint8 *)dst->pixels + dpitch * dy + dx * Bpp;
		Uint8 *s = (Uint8 *)src->pixels + spitch * sy + sx * Bpp;

		for (sw *= Bpp; sh > 0; d += dpitch, s += spitch, sh--)
			memcpy(d, s, sw);
	}
	else
		not_supported();

	return 0;
}

DECLSPEC SDL_Surface * SDLCALL
SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
		     Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
	SDL_Surface *ret;

	trace("%08x, %d, %d, %d, %04x, %04x, %04x, %04x",
		flags, width, height, depth, Rmask, Gmask, Bmask, Amask);

	ret = alloc_surface(width, height, depth);
	if (ret == NULL)
		return NULL;

	ret->pixels = calloc(ret->pitch * height, 1);
	if (ret->pixels == NULL)
		goto fail;

	dbg("returning %p", ret);
	return ret;

fail:
	free(ret);
	return NULL;
}

DECLSPEC int SDLCALL
SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors)
{
	int i, to;

	trace("%p, %p, %d, %d", surface, colors, firstcolor, ncolors);

	if (surface != g_screen)
		return 0;

	to = min(ARRAY_SIZE(g_8bpp_pal), firstcolor + ncolors);
	for (i = firstcolor; i < to; i++) {
		SDL_Color *c = &colors[i - firstcolor];
		int r = c->r, g = c->g, b = c->b;
		// noiz2sa gets a good deal darker that real SDL,
		// don't know what's going on there
/*
		if (r < 0xf8 && (r & 7) > 3) r += 8;
		if (g < 0xfc && (g & 3) > 1) g += 4;
		if (b < 0xf8 && (b & 7) > 3) b += 8;
*/
		g_8bpp_pal[i] = ((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | (b >> 3);
//		g_8bpp_pal[i] += 0x0821;
	}

	return 0;
}

DECLSPEC int SDLCALL
SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
	trace("%p, %p, %04x", dst, dstrect, color);

	if (dst->format->BytesPerPixel != 1) {
		not_supported();
		return -1;
	}
	else
		memset(dst->pixels, color, dst->pitch * dst->h);

	return 0;
}

/*
DECLSPEC SDL_Surface * SDLCALL
SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch,
			 Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
}
*/

DECLSPEC SDL_RWops * SDLCALL
SDL_RWFromFile(const char *file, const char *mode)
{
	SDL_RWops *ret;
	FILE *f;

	trace("%s, %s", file, mode);

	f = fopen(file, mode);
	if (f == NULL)
		return NULL;

	ret = calloc(1, sizeof(*ret));
	if (ret == NULL) {
		fclose(f);
		return NULL;
	}
	ret->hidden.stdio.fp = f;

	return ret;
}

DECLSPEC void SDLCALL
SDL_FreeRW(SDL_RWops *area)
{
	if (area->hidden.stdio.fp) {
		fclose(area->hidden.stdio.fp);
		area->hidden.stdio.fp = NULL;
	}
	free(area);
}

DECLSPEC SDL_Surface * SDLCALL
SDL_LoadBMP_RW(SDL_RWops *src, int freesrc)
{
	struct bmp_dib_v3 {
		unsigned char magic[2];
		uint32_t filesz;
		uint16_t creator1;
		uint16_t creator2;
		uint32_t bmp_offset;
		uint32_t header_sz;
		int32_t width;
		int32_t height;
		uint16_t nplanes;
		uint16_t bitspp;
		uint32_t compress_type;
		uint32_t bmp_bytesz;
		int32_t hres;
		int32_t vres;
		uint32_t ncolors;
		uint32_t nimpcolors;
	} __attribute__((packed)) bmp;
	SDL_Surface *ret = NULL;
	int data_size, read_size;
	uint8_t *tmp_buf = NULL;
	int i, bytespp;
	FILE *f;

	trace("%p, %d", src, freesrc);

	f = src->hidden.stdio.fp;
	if (f == NULL)
		goto out;

	if (fread(&bmp, 1, sizeof(bmp), f) != sizeof(bmp)) {
		err("bmp read error");
		goto out;
	}

	if (bmp.magic[0] != 'B' || bmp.magic[1] != 'M' || bmp.header_sz != 40) {
		err("unhandled BMP format, sz=%d", bmp.header_sz);
		goto out;
	}

	if ((unsigned)bmp.width > 0xffff || (unsigned)bmp.height > 0xffff) {
		err("BMP size %dx%d out of range", bmp.width, bmp.height);
		goto out;
	}

	if (bmp.bitspp != 4) {
		err("BMP unhandled bpp: %d", bmp.bitspp);
		goto out;
	}

	bytespp = bmp.bitspp < 8 ? 1 : (bmp.bitspp + 7) / 8;
	data_size = bmp.width * bmp.height * bytespp;
	tmp_buf = malloc(data_size);
	if (tmp_buf == NULL)
		goto out;

	if (fseek(f, bmp.bmp_offset, SEEK_SET) != 0) {
		err("BMP seek error");
		goto out;
	}

	read_size = bmp.width * bmp.bitspp / 8;
	for (i = bmp.height - 1; i > 0; i--) {
		if (fread((char *)tmp_buf + i * read_size, 1, read_size, f) != read_size) {
			err("BMP read error");
			goto out;
		}
	}

	if (bmp.bitspp == 4) {
		// just convert to 8bpp now
		int c = bmp.width * bmp.height / 2;
		uint8_t *p = tmp_buf;
		for (i = c - 1; i >= 0; i--) {
			p[i * 2 + 1] = p[i] & 0xf;
			p[i * 2] = p[i] >> 4;
		}
	}

	ret = alloc_surface(bmp.width, bmp.height, 8);
	if (ret == NULL)
		goto out;

	ret->pixels = tmp_buf;
	tmp_buf = NULL;

out:
	if (tmp_buf != NULL)
		free(tmp_buf);
	if (freesrc)
		SDL_FreeRW(src);

	return ret;
}

DECLSPEC SDL_Surface * SDLCALL
SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, Uint32 flags)
{
	SDL_Surface *ret;
	void *data;

	trace("%p, %p, %08x", src, fmt, flags);

	if (src->format->BitsPerPixel != fmt->BitsPerPixel) {
		err("convert: not implemented");
		return NULL;
	}

	data = malloc(src->pitch * src->h);
	if (data == NULL)
		return NULL;
	memcpy(data, src->pixels, src->pitch * src->h);

	ret = alloc_surface(src->w, src->h, fmt->BitsPerPixel);
	if (ret == NULL) {
		free(data);
		return NULL;
	}
	ret->pixels = data;
	return ret;
}

DECLSPEC int SDLCALL
SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key)
{
	trace("%p, %08x, %04x", surface, flag, key);
	return 0;
}

DECLSPEC void SDLCALL
SDL_WM_SetCaption(const char *title, const char *icon)
{
	trace("%s, %s", title, icon);
}

DECLSPEC int SDLCALL
SDL_ShowCursor(int toggle)
{
	trace("%d", toggle);
	return 0;
}

DECLSPEC char * SDLCALL
SDL_GetError(void)
{
	trace("");

	return "";
}

