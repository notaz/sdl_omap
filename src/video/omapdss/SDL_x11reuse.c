/* this file is all aobut reusing stock SDL's x11 functions */

#include "SDL_x11reuse.h"
#define SDL_PrivateVideoData SDL_x11_PrivateVideoData
#include "../x11/SDL_x11video.h"

/* mess from SDL_x11video.h */
#undef SDL_Display
#undef GFX_Display
#undef SDL_Screen
#undef WMwindow
#undef SDL_iconcolors

struct x11reuse_context {
	SDL_VideoDevice fake_device;
	struct SDL_x11_PrivateVideoData fake_x11private;
};

struct x11reuse_context *x11reuse_init(void)
{
	struct x11reuse_context *retval;
	int ret;

	ret = SDL_X11_LoadSymbols();
	if (ret == 0) {
		fprintf(stderr, "omapsdl: SDL_X11_LoadSymbols failed\n");
		return NULL;
	}

	retval = calloc(1, sizeof(*retval));
	if (retval == NULL)
		return NULL;

	retval->fake_device.hidden = &retval->fake_x11private;
	return retval;
}

void x11reuse_SetCaption(struct x11reuse_context *context,
	void *display, int screen, void *window,
	const char *title, const char *icon)
{
	if (context == NULL)
		return;

	context->fake_x11private.X11_Display =
	context->fake_x11private.GFX_Display = display;
	context->fake_x11private.WMwindow = (Window)window;
	X11_SetCaptionNoLock(&context->fake_device, title, icon);
}

void x11reuse_SetIcon(struct x11reuse_context *context,
	void *display, int screen, void *window,
	void *icon, void *mask)
{
	if (context == NULL)
		return;

	context->fake_x11private.X11_Display =
	context->fake_x11private.GFX_Display = display;
	context->fake_x11private.WMwindow = (Window)window;
	X11_SetIcon(&context->fake_device, icon, mask);
}
