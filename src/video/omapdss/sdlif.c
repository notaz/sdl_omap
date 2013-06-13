/*
 * (C) Gražvydas "notaz" Ignotas, 2010-2012
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <X11/XF86keysym.h>

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_x11reuse.h"
#include "linux/xenv.h"
#include "osdl.h"


static int omap_available(void) 
{
	trace();
	return 1;
}

static void omap_free(SDL_VideoDevice *device)
{
	trace();
	free(device);
}

static int omap_VideoInit(SDL_VideoDevice *this, SDL_PixelFormat *vformat)
{
	struct SDL_PrivateVideoData *pdata = this->hidden;
	const char *tmp;
	int w, h, ret;

	trace();

	// default to 16bpp
	vformat->BitsPerPixel = 16;

	omapsdl_input_init();
	omapsdl_config(pdata);

	tmp = getenv("SDL_OMAP_DEFAULT_MODE");
	if (tmp != NULL && sscanf(tmp, "%dx%d", &w, &h) == 2) {
		this->info.current_w = w;
		this->info.current_h = h;
	}
	else if (osdl_video_detect_screen(pdata) == 0) {
		this->info.current_w = pdata->phys_w;
		this->info.current_h = pdata->phys_h;
	}

	this->handles_any_size = 1;
	this->info.hw_available = 1;

	pdata->x11reuse_context = x11reuse_init();

	return 0;
}

static void omap_VideoQuit(SDL_VideoDevice *this)
{
	trace();

	osdl_video_finish(this->hidden);
	this->screen->pixels = NULL;
	omapsdl_input_finish();
}

static SDL_Rect **omap_ListModes(SDL_VideoDevice *this, SDL_PixelFormat *format, Uint32 flags)
{
	static SDL_Rect omap_mode_max = {
		/* with handles_any_size, should accept anything up to this
		 * XXX: possibly set this dynamically based on free vram? */
		0, 0, 1600, 1200
	};
	/* broken API needs this stupidity */
	static SDL_Rect *omap_modes[] = {
		&omap_mode_max,
		NULL
	};

	trace();

	if (format->BitsPerPixel <= 8)
		// not (yet?) supported
		return NULL;

	return omap_modes;
}

static SDL_Surface *omap_SetVideoMode(SDL_VideoDevice *this, SDL_Surface *current, int width,
					int height, int bpp, Uint32 flags)
{
	struct SDL_PrivateVideoData *pdata = this->hidden;
	SDL_PixelFormat *format;
	Uint32 unhandled_flags;
	int doublebuf;
	void *fbmem;

	trace("%d, %d, %d, %08x", width, height, bpp, flags);

	omapsdl_config_from_env(pdata);

	switch (bpp) {
	case 16:
		format = SDL_ReallocFormat(current, 16, 0xf800, 0x07e0, 0x001f, 0);
		break;
	case 24:
		format = SDL_ReallocFormat(current, 24, 0xff0000, 0xff00, 0xff, 0);
		break;
	case 32:
		format = SDL_ReallocFormat(current, 32, 0xff0000, 0xff00, 0xff, 0);
		break;
	default:
		err("SetVideoMode: bpp %d not supported", bpp);
		return NULL;
	}
	if (format == NULL)
		return NULL;

	if (!(flags & SDL_DOUBLEBUF) && pdata->cfg_force_doublebuf) {
		log("forcing SDL_DOUBLEBUF");
		flags |= SDL_DOUBLEBUF;
	}

	if (pdata->border_l | pdata->border_r | pdata->border_t | pdata->border_b) {
		if (pdata->border_l + pdata->border_r >= width
		    || pdata->border_t + pdata->border_b >= height)
		{
			err("specified border too large, ignoring");
			pdata->border_l = pdata->border_r = pdata->border_t = pdata->border_b = 0;
		}
	}

	/* always use doublebuf, when SDL_DOUBLEBUF is not set,
	 * we'll have to blit manually on UpdateRects() */
	doublebuf = 1;

	fbmem = osdl_video_set_mode(pdata,
		pdata->border_l, pdata->border_r, pdata->border_t, pdata->border_b,
		width, height, bpp, &doublebuf, this->wm_title);
	if (fbmem == NULL) {
		err("failing on mode %dx%d@%d, doublebuf %s, border %d,%d,%d,%d",
		    width, height, bpp, (flags & SDL_DOUBLEBUF) ? "on" : "off",
		    pdata->border_l, pdata->border_r, pdata->border_t, pdata->border_b);
		return NULL;
	}
	pdata->front_buffer = osdl_video_get_active_buffer(pdata);
	if (pdata->front_buffer == NULL) {
		err("osdl_video_get_active_buffer failed\n");
		return NULL;
	}

	if (!doublebuf) {
		if (flags & SDL_DOUBLEBUF) {
			log("doublebuffering could not be set\n");
			flags &= ~SDL_DOUBLEBUF;
		}
		/* XXX: could just malloc a back buffer here instead */
		pdata->cfg_force_directbuf = 1;
	}

	if (!(flags & SDL_DOUBLEBUF) && pdata->cfg_force_directbuf)
		fbmem = pdata->front_buffer;

	flags |= SDL_FULLSCREEN | SDL_HWSURFACE;
	unhandled_flags = flags & ~(SDL_FULLSCREEN | SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (unhandled_flags != 0) {
		log("dropping unhandled flags: %08x", unhandled_flags);
		flags &= ~unhandled_flags;
	}

	current->flags = flags;
	current->w = width;
	current->h = height;
	current->pitch = SDL_CalculatePitch(current);
	current->pixels = fbmem;
	pdata->app_uses_flip = 0;

	if (pdata->layer_w != 0 && pdata->layer_h != 0) {
		int v_width  = width  - (pdata->border_l + pdata->border_r);
		int v_height = height - (pdata->border_t + pdata->border_b);
		pdata->ts_xmul = (v_width  << 16) / pdata->layer_w;
		pdata->ts_ymul = (v_height << 16) / pdata->layer_h;
	}

	if (pdata->delayed_icon != NULL) {
		this->SetIcon(this, pdata->delayed_icon,
			pdata->delayed_icon_mask);
		SDL_FreeSurface(pdata->delayed_icon);
		pdata->delayed_icon = NULL;
		if (pdata->delayed_icon_mask) {
			free(pdata->delayed_icon_mask);
			pdata->delayed_icon_mask = NULL;
		}
	}

	return current;
}

static int omap_LockHWSurface(SDL_VideoDevice *this, SDL_Surface *surface)
{
	trace("%p", surface);

	return 0;
}

static void omap_UnlockHWSurface(SDL_VideoDevice *this, SDL_Surface *surface)
{
	trace("%p", surface);
}

static int omap_FlipHWSurface(SDL_VideoDevice *this, SDL_Surface *surface)
{
	struct SDL_PrivateVideoData *pdata = this->hidden;
	static int warned;

	trace("%p", surface);

	if (surface != this->screen) {
		if (!warned) {
			err("flip surface %p which is not screen %p?\n",
				surface, this->screen);
			warned = 1;
		}
		return;
	}

	if (surface->flags & SDL_DOUBLEBUF)
		surface->pixels = osdl_video_flip(pdata);
	else {
		if (surface->pixels != pdata->front_buffer)
			memcpy(surface->pixels, pdata->front_buffer,
				surface->pitch * surface->h);
	}

	pdata->app_uses_flip = 1;

	return 0;
}

/* we can't do hw surfaces (besides screen one) yet */
static int omap_AllocHWSurface(SDL_VideoDevice *this, SDL_Surface *surface)
{
	trace("%p", surface);
	return -1;
}

static void omap_FreeHWSurface(SDL_VideoDevice *this, SDL_Surface *surface)
{
	trace("%p", surface);
}

static int omap_SetColors(SDL_VideoDevice *this, int firstcolor, int ncolors, SDL_Color *colors)
{
	trace("%d, %d, %p", firstcolor, ncolors, colors);
	return 0;
}

static void omap_UpdateRects(SDL_VideoDevice *this, int nrects, SDL_Rect *rects)
{
	struct SDL_PrivateVideoData *pdata = this->hidden;
	SDL_Surface *screen = this->screen;
	int fullscreen_blit = 0;
	int i, Bpp, x, y, w, h;
	char *src, *dst;

	trace("%d, %p", nrects, rects);

	fullscreen_blit =
		nrects == 1 && rects->x == 0 && rects->y == 0
		    && (rects->w == screen->w || rects->w == 0)
		    && (rects->h == screen->h || rects->h == 0);

	if (screen->flags & SDL_DOUBLEBUF) {
		if (fullscreen_blit && !pdata->app_uses_flip)
			screen->pixels = osdl_video_flip(pdata);
		return;
	}

	src = screen->pixels;
	dst = pdata->front_buffer;
	if (src == dst)
		return;

	if (fullscreen_blit) {
		memcpy(dst, src, screen->pitch * screen->h);
		return;
	}

	for (i = 0, Bpp = screen->format->BytesPerPixel; i < nrects; i++) {
		/* this supposedly has no clipping, but we'll do it anyway */
		x = rects[i].x, y = rects[i].y, w = rects[i].w, h = rects[i].h;
		if (x < 0)
			w += x, x = 0;
		else if (x + w > screen->w)
			w = screen->w - x;
		if (w <= 0)
			continue;

		if (y < 0)
			h += y, y = 0;
		else if (y + h > screen->h)
			h = screen->h - y;

		for (; h > 0; y++, h--)
			memcpy(dst + y * screen->pitch + x * Bpp,
			       src + y * screen->pitch + x * Bpp,
			       w * Bpp);
	}
}

static void omap_InitOSKeymap(SDL_VideoDevice *this)
{
	trace();
}

static int key_event_cb(void *cb_arg, int sdl_kc, int sdl_sc, int is_pressed)
{
	SDL_keysym keysym = { 0, };
	int shift = 0;
	SDLMod mod;
	int ret;

	keysym.sym = sdl_kc;
	keysym.scancode = sdl_sc;

	/* 0xff if pandora's Fn, so we exclude it too.. */
	if (is_pressed && sdl_kc < 0xff && SDL_TranslateUNICODE) {
		mod = SDL_GetModState();
		if (!(mod & KMOD_CTRL) && (!!(mod & KMOD_SHIFT) ^ !!(mod & KMOD_CAPS)))
			shift = 1;

		/* prefer X mapping, if that doesn't work use hardcoded one */
		ret = xenv_keycode_to_keysym(sdl_sc, shift);
		if (ret >= 0) {
			keysym.unicode = ret;
			if ((mod & KMOD_CTRL)
			    && 0x60 <= keysym.unicode && keysym.unicode <= 0x7f)
			{
				keysym.unicode -= 0x60;
			}
			/* hmh.. */
			if ((keysym.unicode & 0xff00) == 0xff00)
				keysym.unicode &= ~0xff00;
		}
		else {
			keysym.unicode = sdl_kc;
			if ((mod & KMOD_CTRL) && 0x60 <= sdl_kc && sdl_kc <= 0x7f)
			{
				keysym.unicode = sdl_kc - 0x60;
			}
			else if (shift && 'a' <= sdl_kc && sdl_kc <= 'z')
			{
				keysym.unicode = sdl_kc - 'a' + 'A';
			}
		}
	}

	SDL_PrivateKeyboard(is_pressed, &keysym);
}

/* clamp x to min..max-1 */
#define clamp(x, min, max) \
	if (x < (min)) x = min; \
	if (x >= (max)) x = max

static void translate_mouse(SDL_VideoDevice *this, int *x, int *y)
{
	struct SDL_PrivateVideoData *pdata = this->hidden;

	if (!pdata->cfg_no_ts_translate && pdata->layer_w != 0 && pdata->layer_h != 0) {
		*x = pdata->border_l + ((*x - pdata->layer_x) * pdata->ts_xmul >> 16);
		*y = pdata->border_t + ((*y - pdata->layer_y) * pdata->ts_ymul >> 16);
		clamp(*x, 0, this->screen->w);
		clamp(*y, 0, this->screen->h);
	}
}

static int ts_event_cb(void *cb_arg, int x, int y, unsigned int pressure)
{
	static int was_pressed;
	SDL_VideoDevice *this = cb_arg;
	struct SDL_PrivateVideoData *pdata = this->hidden;

	translate_mouse(this, &x, &y);

	pressure = !!pressure;
	if (pressure != was_pressed) {
		SDL_PrivateMouseButton(pressure ? SDL_PRESSED : SDL_RELEASED, 1, x, y);
		was_pressed = pressure;
	}
	else
		SDL_PrivateMouseMotion(0, 0, x, y);
}

static int xmouseb_event_cb(void *cb_arg, int x, int y, int button, int is_pressed)
{
	SDL_VideoDevice *this = cb_arg;
	struct SDL_PrivateVideoData *pdata = this->hidden;

	translate_mouse(this, &x, &y);
	SDL_PrivateMouseButton(is_pressed ? SDL_PRESSED : SDL_RELEASED, button, x, y);
}

static int xmousem_event_cb(void *cb_arg, int x, int y)
{
	SDL_VideoDevice *this = cb_arg;
	struct SDL_PrivateVideoData *pdata = this->hidden;

	translate_mouse(this, &x, &y);
	SDL_PrivateMouseMotion(0, 0, x, y);
}

static int xkey_cb(void *cb_arg, int kc, int is_pressed)
{
	SDL_VideoDevice *this = cb_arg;
	struct SDL_PrivateVideoData *pdata = this->hidden;
	int ret;

	if (kc == XF86XK_MenuKB && is_pressed) {
		ret = osdl_video_pause(pdata, 1);
		if (ret == 0) {
			xenv_minimize();
			osdl_video_pause(pdata, 0);
			omapsdl_input_get_events(0, NULL, NULL, NULL);
		}
	}
}

static void omap_PumpEvents(SDL_VideoDevice *this) 
{
	struct SDL_PrivateVideoData *pdata = this->hidden;
	int read_tslib = 1;

	trace();

	if (pdata->xenv_up) {
		if (!pdata->cfg_ts_force_tslib) {
			xenv_update(xkey_cb, xmouseb_event_cb, xmousem_event_cb, this);
			if (pdata->xenv_mouse)
				read_tslib = 0;
		}
		else {
			/* just flush X event queue */
			xenv_update(NULL, NULL, NULL, NULL);
		}
	}

	omapsdl_input_get_events(0, key_event_cb,
		read_tslib ? ts_event_cb : NULL, this);
}

static void omap_SetCaption(SDL_VideoDevice *this, const char *title, const char *icon)
{
	void *display = NULL, *window = NULL;
	int screen = 0;
	int ret;

	ret = osdl_video_get_window(&display, &screen, &window);
	if (ret == 0) {
		x11reuse_SetCaption(this->hidden->x11reuse_context,
			display, screen, window, title, icon);
	}
}

static void omap_SetIcon(SDL_VideoDevice *this, SDL_Surface *icon, Uint8 *mask)
{
	struct SDL_PrivateVideoData *pdata = this->hidden;
	void *display = NULL, *window = NULL;
	int screen = 0;
	int ret;

	ret = osdl_video_get_window(&display, &screen, &window);
	if (ret == 0) {
		x11reuse_SetIcon(pdata->x11reuse_context,
			display, screen, window, icon, mask);
	}
	else {
		SDL_Surface *old_icon = pdata->delayed_icon;
		void *old_icon_mask = pdata->delayed_icon_mask;
		int mask_size = ((icon->w + 7) / 8) * icon->h;

		pdata->delayed_icon = SDL_ConvertSurface(icon,
					icon->format, icon->flags);
		if (pdata->delayed_icon != NULL) {
			memcpy(pdata->delayed_icon->pixels, icon->pixels,
				icon->pitch * icon->h);
		}
		if (mask != NULL) {
			pdata->delayed_icon_mask = malloc(mask_size);
			if (pdata->delayed_icon_mask)
				memcpy(pdata->delayed_icon_mask, mask, mask_size);
		}

		if (old_icon != NULL)
			SDL_FreeSurface(old_icon);
		if (old_icon_mask)
			free(old_icon_mask);
	}
}

static SDL_VideoDevice *omap_create(int devindex)
{
	SDL_VideoDevice *this;

	this = calloc(1, sizeof(*this) + sizeof(*this->hidden));
	if (this == NULL) {
		SDL_OutOfMemory();
		return 0;
	}
	this->hidden = (void *)(this + 1);
	this->VideoInit = omap_VideoInit;
	this->ListModes = omap_ListModes;
	this->SetVideoMode = omap_SetVideoMode;
	this->LockHWSurface = omap_LockHWSurface;
	this->UnlockHWSurface = omap_UnlockHWSurface;
	this->FlipHWSurface = omap_FlipHWSurface;
	this->AllocHWSurface = omap_AllocHWSurface;
	this->FreeHWSurface = omap_FreeHWSurface;
	this->SetColors = omap_SetColors;
	this->UpdateRects = omap_UpdateRects;
	this->VideoQuit = omap_VideoQuit;
	this->InitOSKeymap = omap_InitOSKeymap;
	this->PumpEvents = omap_PumpEvents;
	this->SetCaption = omap_SetCaption;
	this->SetIcon = omap_SetIcon;
	this->free = omap_free;

	return this;
}

VideoBootStrap omapdss_bootstrap = {
	"omapdss", "OMAP DSS2 Framebuffer Driver",
	omap_available, omap_create
};

