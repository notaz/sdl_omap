/*
 * (C) Gražvydas "notaz" Ignotas, 2010
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

#include "omapsdl.h"
#include "omapfb.h"
#include "linux/fbdev.h"
#include "linux/oshide.h"

struct omapfb_saved_layer {
	struct omapfb_plane_info pi;
	struct omapfb_mem_info mi;
};

static int osdl_setup_omapfb(int fd, int enabled, int x, int y, int w, int h, int mem)
{
	struct omapfb_plane_info pi;
	struct omapfb_mem_info mi;
	int ret;

	ret = ioctl(fd, OMAPFB_QUERY_PLANE, &pi);
	if (ret != 0) {
		perror("QUERY_PLANE");
		return -1;
	}

	ret = ioctl(fd, OMAPFB_QUERY_MEM, &mi);
	if (ret != 0) {
		perror("QUERY_MEM");
		return -1;
	}

	/* must disable when changing stuff */
	if (pi.enabled) {
		pi.enabled = 0;
		ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
		if (ret != 0)
			perror("SETUP_PLANE");
	}

	mi.size = mem;
	ret = ioctl(fd, OMAPFB_SETUP_MEM, &mi);
	if (ret != 0) {
		perror("SETUP_MEM");
		return -1;
	}

	pi.pos_x = x;
	pi.pos_y = y;
	pi.out_width = w;
	pi.out_height = h;
	pi.enabled = enabled;

	ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
	if (ret != 0) {
		perror("SETUP_PLANE");
		return -1;
	}

	return 0;
}

static int read_sysfs(const char *fname, char *buff, size_t size)
{
	FILE *f;
	int ret;

	f = fopen(fname, "r");
	if (f == NULL) {
		fprintf(stderr, "open %s: ", fname);
		perror(NULL);
		return -1;
	}

	ret = fread(buff, 1, size - 1, f);
	fclose(f);
	if (ret <= 0) {
		fprintf(stderr, "read %s: ", fname);
		perror(NULL);
		return -1;
	}

	buff[ret] = 0;
	for (ret--; ret >= 0 && isspace(buff[ret]); ret--)
		buff[ret] = 0;

	return 0;
}

static int osdl_setup_omap_layer(struct SDL_PrivateVideoData *pdata,
		const char *fbname, int width, int height, int bpp)
{
	int x = 0, y = 0, w = width, h = height; /* layer size and pos */
	int screen_w = w, screen_h = h;
	int fb_id, overlay_id = -1, screen_id = -1;
	char buff[64], screen_name[64];
	struct stat status;
	const char *tmp;
	int i, ret, fd;
	FILE *f;

	fd = open(fbname, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "open %s: ", fbname);
		perror(NULL);
		return -1;
	}

	/* FIXME: assuming layer doesn't change here */
	if (pdata->saved_layer == NULL) {
		struct omapfb_saved_layer *slayer;
		slayer = calloc(1, sizeof(*slayer));
		if (slayer == NULL)
			return -1;

		ret = ioctl(fd, OMAPFB_QUERY_PLANE, &slayer->pi);
		if (ret != 0) {
			perror("QUERY_PLANE");
			return -1;
		}

		ret = ioctl(fd, OMAPFB_QUERY_MEM, &slayer->mi);
		if (ret != 0) {
			perror("QUERY_MEM");
			return -1;
		}

		pdata->saved_layer = slayer;
	}

	/* Figure out screen resolution, we will want to center if scaling is not enabled.
	 * The only way to achieve this seems to be walking some sysfs files.. */
	ret = stat(fbname, &status);
	if (ret != 0) {
		fprintf(stderr, "can't stat %s: ", fbname);
		perror(NULL);
		return -1;
	}
	fb_id = minor(status.st_rdev);

	snprintf(buff, sizeof(buff), "/sys/class/graphics/fb%d/overlays", fb_id);
	f = fopen(buff, "r");
	if (f == NULL) {
		fprintf(stderr, "can't open %s, skip screen detection\n", buff);
		goto skip_screen;
	}

	ret = fscanf(f, "%d", &overlay_id);
	fclose(f);
	if (ret != 1) {
		fprintf(stderr, "can't parse %s, skip screen detection\n", buff);
		goto skip_screen;
	}

	snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/overlay%d/manager", overlay_id);
	ret = read_sysfs(buff, screen_name, sizeof(screen_name));
	if (ret < 0) {
		fprintf(stderr, "skip screen detection\n");
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
		fprintf(stderr, "could not find screen\n");
		goto skip_screen;
	}

	snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/display%d/timings", screen_id);
	f = fopen(buff, "r");
	if (f == NULL) {
		fprintf(stderr, "can't open %s, skip screen detection\n", buff);
		goto skip_screen;
	}

	ret = fscanf(f, "%*d,%d/%*d/%*d/%*d,%d/%*d/%*d/%*d", &screen_w, &screen_h);
	fclose(f);
	if (ret != 2) {
		fprintf(stderr, "can't parse %s (%d), skip screen detection\n", buff, ret);
		goto skip_screen;
	}

	printf("detected %dx%d '%s' (%d) screen attached to fb %d and overlay %d\n",
		screen_w, screen_h, screen_name, screen_id, fb_id, overlay_id);

skip_screen:
	tmp = getenv("SDL_OMAP_LAYER_SIZE");
	if (tmp != NULL) {
		int w_, h_;
		if (strcasecmp(tmp, "fullscreen") == 0)
			w = screen_w, h = screen_h;
		else if (sscanf(tmp, "%dx%d", &w_, &h_) == 2)
			w = w_, h = h_;
		else
			fprintf(stderr, "layer size specified incorrectly, "
					"should be like 800x480");
	}

	x = screen_w / 2 - w / 2;
	y = screen_h / 2 - h / 2;
	ret = osdl_setup_omapfb(fd, 1, x, y, w, h, width * height * ((bpp + 7) / 8) * 3);
	close(fd);

	return ret;
}

int osdl_video_set_mode(struct SDL_PrivateVideoData *pdata, int width, int height, int bpp)
{
	const char *fbname;
	int ret;

	bpp = 16; // FIXME

	fbname = getenv("SDL_FBDEV");
	if (fbname == NULL)
		fbname = "/dev/fb1";

	if (pdata->fbdev != NULL) {
		vout_fbdev_finish(pdata->fbdev);
		pdata->fbdev = NULL;
	}

	omapsdl_config_from_env();

	ret = osdl_setup_omap_layer(pdata, fbname, width, height, bpp);
	if (ret < 0)
		return -1;

	pdata->fbdev = vout_fbdev_init(fbname, &width, &height, 0);
	if (pdata->fbdev == NULL)
		return -1;

	if (!pdata->oshide_done) {
		oshide_init();
		pdata->oshide_done = 1;
	}

	return 0;
}

void *osdl_video_flip(struct SDL_PrivateVideoData *pdata)
{
	if (pdata->fbdev == NULL)
		return NULL;

	if (gcfg_force_vsync)
		vout_fbdev_wait_vsync(pdata->fbdev);

	return vout_fbdev_flip(pdata->fbdev);
}

void osdl_video_finish(struct SDL_PrivateVideoData *pdata)
{
	static const char *fbname;

	fbname = getenv("SDL_FBDEV");
	if (fbname == NULL)
		fbname = "/dev/fb1";

	if (pdata->fbdev != NULL) {
		vout_fbdev_finish(pdata->fbdev);
		pdata->fbdev = NULL;
	}

	/* restore the OMAP layer */
	if (pdata->saved_layer != NULL) {
		struct omapfb_saved_layer *slayer = pdata->saved_layer;
		int fd;

		fd = open(fbname, O_RDWR);
		if (fd != -1) {
			int enabled = slayer->pi.enabled;

			/* be sure to disable while setting up */
			slayer->pi.enabled = 0;
			ioctl(fd, OMAPFB_SETUP_PLANE, &slayer->pi);
			ioctl(fd, OMAPFB_SETUP_MEM, &slayer->mi);
			if (enabled) {
				slayer->pi.enabled = enabled;
				ioctl(fd, OMAPFB_SETUP_PLANE, &slayer->pi);
			}
			close(fd);
		}
		free(slayer);
		pdata->saved_layer = NULL;
	}

	if (pdata->oshide_done) {
		oshide_finish();
		pdata->oshide_done = 0;
	}
}
