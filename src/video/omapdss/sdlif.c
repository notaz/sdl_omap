/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdlib.h>

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "omapsdl.h"


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
	trace();

	// default to 16bpp
	vformat->BitsPerPixel = 16;

	omapsdl_input_init();
	omapsdl_config();

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
	static SDL_Rect omap_mode_list[] = {
		// XXX: we are not really restricted to fixed modes
		// FIXME: should really check the display for max supported
		{ 0, 0, 1600, 1200 },
		{ 0, 0, 1408, 1056 },
		{ 0, 0, 1280, 1024 },
		{ 0, 0, 1152,  864 },
		{ 0, 0, 1024,  768 },
		{ 0, 0,  960,  720 },
		{ 0, 0,  800,  600 },
		{ 0, 0,  768,  576 },
		{ 0, 0,  720,  576 },
		{ 0, 0,  800,  480 },
		{ 0, 0,  720,  480 },
		{ 0, 0,  640,  480 },
		{ 0, 0,  640,  400 },
		{ 0, 0,  512,  384 },
		{ 0, 0,  320,  240 },
		{ 0, 0,  320,  200 },
	};
	// broken API needs this
	static SDL_Rect *omap_modes[] = {
		&omap_mode_list[0],
		&omap_mode_list[1],
		&omap_mode_list[2],
		&omap_mode_list[3],
		&omap_mode_list[4],
		&omap_mode_list[5],
		&omap_mode_list[6],
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
	trace("%d, %d, %d, %08x", width, height, bpp, flags);

	if (osdl_video_set_mode(this->hidden, width, height, bpp) < 0)
		return NULL;

	if (!SDL_ReallocFormat(current, 16, 0xf800, 0x07e0, 0x001f, 0))
		return NULL;

	current->flags = SDL_FULLSCREEN | SDL_DOUBLEBUF | SDL_HWSURFACE;
	current->w = width;
	current->h = height;
	current->pitch = SDL_CalculatePitch(current);

	current->pixels = osdl_video_flip(this->hidden);

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
	trace("%p", surface);

	surface->pixels = osdl_video_flip(this->hidden);

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
	trace("%d, %p", nrects, rects);

	if (nrects != 1 || rects->x != 0 || rects->y != 0 ||
			rects->w != this->screen->w || rects->h != this->screen->h) {
		static int warned = 0;
		if (!warned) {
			not_supported();
			warned = 1;
		}
	}

	this->screen->pixels = osdl_video_flip(this->hidden);
}

static void omap_InitOSKeymap(SDL_VideoDevice *this)
{
	trace();
}

static int key_event_cb(void *cb_arg, int sdl_kc, int is_pressed)
{
	SDL_keysym keysym = { 0, };

	keysym.sym = sdl_kc;
	SDL_PrivateKeyboard(is_pressed, &keysym);
}

static int ts_event_cb(void *cb_arg, int x, int y, unsigned int pressure)
{
	static int was_pressed;

	SDL_PrivateMouseMotion(0, 0, x, y);

	pressure = !!pressure;
	if (pressure != was_pressed) {
		SDL_PrivateMouseButton(pressure ? SDL_PRESSED : SDL_RELEASED, 1, 0, 0);
		was_pressed = pressure;
	}
}

static void omap_PumpEvents(SDL_VideoDevice *this) 
{
	trace();

	omapsdl_input_get_events(0, key_event_cb, ts_event_cb, NULL);
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
	this->free = omap_free;

	return this;
}

VideoBootStrap omapdss_bootstrap = {
	"omapdss", "OMAP DSS2 Framebuffer Driver",
	omap_available, omap_create
};

