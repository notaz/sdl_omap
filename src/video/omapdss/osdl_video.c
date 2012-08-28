/*
 * (C) Gražvydas "notaz" Ignotas, 2010-2012
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fb.h>

#include "osdl.h"
#include "omapfb.h"
#include "linux/fbdev.h"
#include "linux/xenv.h"

#define MIN(a, b) ( ((a) < (b)) ? (a) : (b) )

struct omapfb_state {
	struct omapfb_plane_info pi;
	struct omapfb_mem_info mi;
	struct omapfb_plane_info pi_old;
	struct omapfb_mem_info mi_old;
};

static const char *get_fb_device(void)
{
	const char *fbname = getenv("SDL_FBDEV");
	if (fbname == NULL)
		fbname = "/dev/fb1";

	return fbname;
}

static int osdl_setup_omapfb(struct omapfb_state *ostate, int fd, int enabled,
	int x, int y, int w, int h, int mem, int *buffer_count)
{
	struct omapfb_plane_info pi;
	struct omapfb_mem_info mi;
	int mem_blocks = *buffer_count;
	unsigned int size_cur;
	int ret;

	memset(&pi, 0, sizeof(pi));
	memset(&mi, 0, sizeof(mi));

	ret = ioctl(fd, OMAPFB_QUERY_PLANE, &pi);
	if (ret != 0) {
		err_perror("QUERY_PLANE");
		return -1;
	}

	ret = ioctl(fd, OMAPFB_QUERY_MEM, &mi);
	if (ret != 0) {
		err_perror("QUERY_MEM");
		return -1;
	}
	size_cur = mi.size;

	/* must disable when changing stuff */
	if (pi.enabled) {
		pi.enabled = 0;
		ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
		if (ret != 0)
			err_perror("SETUP_PLANE");
	}

	/* allocate more mem, if needed */
	for (; size_cur < mem * mem_blocks && mem_blocks > 0; mem_blocks--) {
		mi.size = mem * mem_blocks;
		ret = ioctl(fd, OMAPFB_SETUP_MEM, &mi);
		if (ret == 0)
			break;
		mi.size = size_cur;
	}
	if (mem_blocks <= 0) {
		err("failed to allocate at least %d bytes of vram:\n", mem);
		err_perror("SETUP_MEM");
		return -1;
	}

	*buffer_count = mem_blocks;

	pi.pos_x = x;
	pi.pos_y = y;
	pi.out_width = w;
	pi.out_height = h;
	pi.enabled = enabled;

	ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
	if (ret != 0) {
		err_perror("SETUP_PLANE");
		err("(%d %d %d %d)\n", x, y, w, h);
		return -1;
	}

	ostate->pi = pi;
	ostate->mi = mi;
	return 0;
}

static int read_sysfs(const char *fname, char *buff, size_t size)
{
	FILE *f;
	int ret;

	f = fopen(fname, "r");
	if (f == NULL) {
		err_perror("open %s: ", fname);
		return -1;
	}

	ret = fread(buff, 1, size - 1, f);
	fclose(f);
	if (ret <= 0) {
		err_perror("read %s: ", fname);
		return -1;
	}

	buff[ret] = 0;
	for (ret--; ret >= 0 && isspace(buff[ret]); ret--)
		buff[ret] = 0;

	return 0;
}

int read_vscreeninfo(const char *fbname, int *w, int *h)
{
	struct fb_var_screeninfo fbvar;
	int ret, fd;

	fd = open(fbname, O_RDWR);
	if (fd == -1) {
		err_perror("open %s", fbname);
		return -1;
	}

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &fbvar);
	close(fd);

	if (ret == -1) {
		err_perror("ioctl %s", fbname);
		return -1;
	}

	if (fbvar.xres == 0 || fbvar.yres == 0)
		return -1;

	*w = fbvar.xres;
	*h = fbvar.yres;
	return 0;
}

int osdl_video_detect_screen(struct SDL_PrivateVideoData *pdata)
{
	int fb_id, overlay_id = -1, screen_id = -1;
	char buff[64], screen_name[64];
	const char *fbname;
	struct stat status;
	int fd, i, ret;
	int w, h;
	FILE *f;

	pdata->phys_w = pdata->phys_h = 0;

	fbname = get_fb_device();

	/* Figure out screen resolution, we need to know default resolution
	 * to report to SDL and for centering stuff.
	 * The only way to achieve this seems to be walking some sysfs files.. */
	ret = stat(fbname, &status);
	if (ret != 0) {
		err_perror("can't stat %s", fbname);
		goto skip_screen;
	}
	fb_id = minor(status.st_rdev);

	snprintf(buff, sizeof(buff), "/sys/class/graphics/fb%d/overlays", fb_id);
	f = fopen(buff, "r");
	if (f == NULL) {
		err("can't open %s, skip screen detection", buff);
		goto skip_screen;
	}

	ret = fscanf(f, "%d", &overlay_id);
	fclose(f);
	if (ret != 1) {
		err("can't parse %s, skip screen detection", buff);
		goto skip_screen;
	}

	snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/overlay%d/manager", overlay_id);
	ret = read_sysfs(buff, screen_name, sizeof(screen_name));
	if (ret < 0) {
		err("skip screen detection");
		goto skip_screen;
	}

	for (i = 0; ; i++) {
		snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/display%d/name", i);
		ret = read_sysfs(buff, buff, sizeof(buff));
		if (ret < 0)
			break;

		if (strcmp(screen_name, buff) == 0) {
			screen_id = i;
			break;
		}
	}

	if (screen_id < 0) {
		err("could not find screen");
		goto skip_screen;
	}

	snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/display%d/timings", screen_id);
	f = fopen(buff, "r");
	if (f == NULL) {
		err("can't open %s, skip screen detection", buff);
		goto skip_screen;
	}

	ret = fscanf(f, "%*d,%d/%*d/%*d/%*d,%d/%*d/%*d/%*d", &w, &h);
	fclose(f);
	if (ret != 2) {
		err("can't parse %s (%d), skip screen detection", buff, ret);
		goto skip_screen;
	}

	log("detected %dx%d '%s' (%d) screen attached to fb %d and overlay %d",
		w, h, screen_name, screen_id, fb_id, overlay_id);

	pdata->phys_w = w;
	pdata->phys_h = h;
	return 0;

skip_screen:
	/* attempt to extract this from FB then */
	ret = read_vscreeninfo(fbname, &pdata->phys_w, &pdata->phys_h);
	if (ret != 0 && strcmp(fbname, "/dev/fb0") != 0) {
		/* last resort */
		ret = read_vscreeninfo("/dev/fb0", &pdata->phys_w, &pdata->phys_h);
	}

	if (ret != 0) {
		err("VSCREENINFO has nothing meaningful");
		return -1;
	}

	return 0;
}

static int osdl_setup_omap_layer(struct SDL_PrivateVideoData *pdata,
		const char *fbname, int width, int height, int bpp, int *buffer_count)
{
	int x = 0, y = 0, w = width, h = height; /* layer size and pos */
	int screen_w = w, screen_h = h;
	int tmp_w, tmp_h;
	const char *tmp;
	int retval = -1;
	int ret, fd;

	pdata->layer_x = pdata->layer_y = pdata->layer_w = pdata->layer_h = 0;

	if (pdata->phys_w != 0)
		screen_w = pdata->phys_w;
	if (pdata->phys_h != 0)
		screen_h = pdata->phys_h;

	fd = open(fbname, O_RDWR);
	if (fd == -1) {
		err_perror("open %s", fbname);
		return -1;
	}

	/* FIXME: assuming layer doesn't change here */
	if (pdata->saved_layer == NULL) {
		struct omapfb_state *slayer;
		slayer = calloc(1, sizeof(*slayer));
		if (slayer == NULL)
			goto out;

		ret = ioctl(fd, OMAPFB_QUERY_PLANE, &slayer->pi_old);
		if (ret != 0) {
			err_perror("QUERY_PLANE");
			goto out;
		}

		ret = ioctl(fd, OMAPFB_QUERY_MEM, &slayer->mi_old);
		if (ret != 0) {
			err_perror("QUERY_MEM");
			goto out;
		}

		pdata->saved_layer = slayer;
	}

	tmp = getenv("SDL_OMAP_LAYER_SIZE");
	if (tmp != NULL) {
		if (strcasecmp(tmp, "fullscreen") == 0) {
			w = screen_w, h = screen_h;
		}
		else if (strcasecmp(tmp, "scaled") == 0) {
			float factor = MIN(((float)screen_w) / width, ((float)screen_h) / height);
			w = (int)(factor*width), h = (int)(factor*height);
		}
		else if (strcasecmp(tmp, "pixelperfect") == 0) {
			float factor = MIN(((float)screen_w) / width, ((float)screen_h) / height);
			w = ((int)factor) * width, h = ((int)factor) * height;
			/* factor < 1.f => 0x0 layer, so fall back to 'scaled' */
			if (!w || !h) {
				w = (int)(factor * width), h = (int)(factor * height);
			}
		}
		else if (sscanf(tmp, "%dx%d", &tmp_w, &tmp_h) == 2) {
			w = tmp_w, h = tmp_h;
		}
		else {
			err("layer size specified incorrectly, "
				"should be like 800x480");
		}
	}

	/* the layer can't be set larger than screen */
	tmp_w = w, tmp_h = h;
	if (w > screen_w)
		w = screen_w;
	if (h > screen_h)
		h = screen_h;
	if (w != tmp_w || h != tmp_h)
		log("layer resized %dx%d -> %dx%d to fit screen", tmp_w, tmp_h, w, h);

	x = screen_w / 2 - w / 2;
	y = screen_h / 2 - h / 2;
	ret = osdl_setup_omapfb(pdata->saved_layer, fd, 1, x, y, w, h,
				width * height * ((bpp + 7) / 8), buffer_count);
	if (ret == 0) {
		pdata->layer_x = x;
		pdata->layer_y = y;
		pdata->layer_w = w;
		pdata->layer_h = h;
	}

	retval = ret;
out:
	close(fd);
	return retval;
}

void *osdl_video_set_mode(struct SDL_PrivateVideoData *pdata,
			  int border_l, int border_r, int border_t, int border_b,
			  int width, int height, int bpp, int *doublebuf,
			  const char *wm_title)
{
	int buffers_try, buffers_set;
	const char *fbname;
	void *result;
	int ret;

	fbname = get_fb_device();

	if (pdata->fbdev != NULL) {
		vout_fbdev_finish(pdata->fbdev);
		pdata->fbdev = NULL;
	}

	buffers_try = buffers_set = 1;
	if (*doublebuf)
		/* actually try tripple buffering for reduced chance of tearing */
		buffers_try = buffers_set = 3;

	ret = osdl_setup_omap_layer(pdata, fbname, width, height, bpp, &buffers_set);
	if (ret < 0)
		goto fail;

	pdata->fbdev = vout_fbdev_init(fbname, &width, &height, bpp, buffers_set);
	if (pdata->fbdev == NULL)
		goto fail;

	if (border_l | border_r | border_t | border_b) {
		width -= border_l + border_r;
		height -= border_t + border_b;
		result = vout_fbdev_resize(pdata->fbdev, width, height, bpp,
				border_l, border_r, border_t, border_b, buffers_set);
	}
	else {
		result = osdl_video_flip(pdata);
	}
	if (result == NULL)
		goto fail;

	if (!pdata->xenv_up) {
		int xenv_flags = XENV_CAP_KEYS | XENV_CAP_MOUSE;
		ret = xenv_init(&xenv_flags, wm_title);
		if (ret == 0) {
			pdata->xenv_up = 1;
			pdata->xenv_mouse = (xenv_flags & XENV_CAP_MOUSE) ? 1 : 0;
		}
	}

	if (buffers_try != buffers_set) {
		log("only %d/%d buffers available, expect tearing\n",
			buffers_set, buffers_try);
		*doublebuf = buffers_set > 1;
	}

	return result;

fail:
	osdl_video_finish(pdata);
	return NULL;
}

void *osdl_video_flip(struct SDL_PrivateVideoData *pdata)
{
	void *ret;

	if (pdata->fbdev == NULL)
		return NULL;

	ret = vout_fbdev_flip(pdata->fbdev);

	if (pdata->cfg_force_vsync)
		vout_fbdev_wait_vsync(pdata->fbdev);

	return ret;
}

int osdl_video_pause(struct SDL_PrivateVideoData *pdata, int is_pause)
{
	struct omapfb_state *state = pdata->saved_layer;
	struct omapfb_plane_info pi;
	struct omapfb_mem_info mi;
	int enabled;
	int fd = -1;
	int ret;

	if (pdata->fbdev != NULL)
		fd = vout_fbdev_get_fd(pdata->fbdev);
	if (fd == -1) {
		err("bad fd %d", fd);
		return -1;
	}
	if (state == NULL) {
		err("missing layer state\n");
		return -1;
	}

	if (is_pause) {
		ret = vout_fbdev_save(pdata->fbdev);
		if (ret != 0)
			return ret;
		pi = state->pi_old;
		mi = state->mi_old;
		enabled = pi.enabled;
	} else {
		pi = state->pi;
		mi = state->mi;
		enabled = 1;
	}
	pi.enabled = 0;
	ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
	if (ret != 0) {
		err_perror("SETUP_PLANE");
		return -1;
	}

	ret = ioctl(fd, OMAPFB_SETUP_MEM, &mi);
	if (ret != 0)
		err_perror("SETUP_MEM");

	if (!is_pause) {
		ret = vout_fbdev_restore(pdata->fbdev);
		if (ret != 0) {
			err("fbdev_restore failed\n");
			return ret;
		}
	}

	if (enabled) {
		pi.enabled = 1;
		ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
		if (ret != 0) {
			err_perror("SETUP_PLANE");
			return -1;
		}
	}

	return 0;
}

void osdl_video_finish(struct SDL_PrivateVideoData *pdata)
{
	static const char *fbname;

	fbname = get_fb_device();
	if (pdata->fbdev != NULL) {
		vout_fbdev_finish(pdata->fbdev);
		pdata->fbdev = NULL;
	}

	/* restore the OMAP layer */
	if (pdata->saved_layer != NULL) {
		struct omapfb_state *slayer = pdata->saved_layer;
		int fd;

		fd = open(fbname, O_RDWR);
		if (fd != -1) {
			int enabled = slayer->pi_old.enabled;

			/* be sure to disable while setting up */
			slayer->pi_old.enabled = 0;
			ioctl(fd, OMAPFB_SETUP_PLANE, &slayer->pi_old);
			ioctl(fd, OMAPFB_SETUP_MEM, &slayer->mi_old);
			if (enabled) {
				slayer->pi_old.enabled = enabled;
				ioctl(fd, OMAPFB_SETUP_PLANE, &slayer->pi_old);
			}
			close(fd);
		}
		free(slayer);
		pdata->saved_layer = NULL;
	}

	if (pdata->xenv_up) {
		xenv_finish();
		pdata->xenv_up = 0;
	}
}

