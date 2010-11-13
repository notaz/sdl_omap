/*
 * (C) Gražvydas "notaz" Ignotas, 2009-2010
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/matroxfb.h>

#include "fbdev.h"

#define FBDEV_MAX_BUFFERS 3

struct vout_fbdev {
	int	fd;
	void	*mem;
	size_t	mem_size;
	struct	fb_var_screeninfo fbvar_old;
	struct	fb_var_screeninfo fbvar_new;
	int	buffer_write;
	int	fb_size;
	int	buffer_count;
	int	top_border, bottom_border;
};

void *vout_fbdev_flip(struct vout_fbdev *fbdev)
{
	int draw_buf;

	if (fbdev->buffer_count < 2)
		return fbdev->mem;

	draw_buf = fbdev->buffer_write;
	fbdev->buffer_write++;
	if (fbdev->buffer_write >= fbdev->buffer_count)
		fbdev->buffer_write = 0;

	fbdev->fbvar_new.yoffset = 
		(fbdev->top_border + fbdev->fbvar_new.yres + fbdev->bottom_border) * draw_buf +
		fbdev->top_border;

	ioctl(fbdev->fd, FBIOPAN_DISPLAY, &fbdev->fbvar_new);

	return (char *)fbdev->mem + fbdev->fb_size * fbdev->buffer_write;
}

void vout_fbdev_wait_vsync(struct vout_fbdev *fbdev)
{
	int arg = 0;
	ioctl(fbdev->fd, FBIO_WAITFORVSYNC, &arg);
}

int vout_fbdev_resize(struct vout_fbdev *fbdev, int w, int h,
		      int left_border, int right_border, int top_border, int bottom_border, int no_dblbuf)
{
	int w_total = left_border + w + right_border;
	int h_total = top_border + h + bottom_border;
	size_t mem_size;
	int ret;

	// unblank to be sure the mode is really accepted
	ioctl(fbdev->fd, FBIOBLANK, FB_BLANK_UNBLANK);

	if (fbdev->fbvar_new.bits_per_pixel != 16 ||
			w != fbdev->fbvar_new.xres ||
			h != fbdev->fbvar_new.yres ||
			w_total != fbdev->fbvar_new.xres_virtual ||
			h_total > fbdev->fbvar_new.yres_virtual ||
			left_border != fbdev->fbvar_new.xoffset) {
		fbdev->fbvar_new.xres = w;
		fbdev->fbvar_new.yres = h;
		fbdev->fbvar_new.xres_virtual = w_total;
		fbdev->fbvar_new.yres_virtual = h_total;
		fbdev->fbvar_new.xoffset = left_border;
		fbdev->fbvar_new.bits_per_pixel = 16;
		printf(" switching to %dx%d@16\n", w, h);
		ret = ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_new);
		if (ret == -1) {
			perror("FBIOPUT_VSCREENINFO ioctl");
			return -1;
		}
	}

	fbdev->buffer_count = FBDEV_MAX_BUFFERS; // be optimistic
	if (no_dblbuf)
		fbdev->buffer_count = 1;

	if (fbdev->fbvar_new.yres_virtual < h_total * fbdev->buffer_count) {
		fbdev->fbvar_new.yres_virtual = h_total * fbdev->buffer_count;
		ret = ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_new);
		if (ret == -1) {
			fbdev->buffer_count = 1;
			fprintf(stderr, "Warning: failed to increase virtual resolution, "
					"doublebuffering disabled\n");
		}
	}

	fbdev->fb_size = w_total * h_total * 2;
	fbdev->top_border = top_border;
	fbdev->bottom_border = bottom_border;

	mem_size = fbdev->fb_size * fbdev->buffer_count;
	if (fbdev->mem_size >= mem_size)
		return 0;

	if (fbdev->mem != NULL)
		munmap(fbdev->mem, fbdev->mem_size);

	fbdev->mem = mmap(0, mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev->fd, 0);
	if (fbdev->mem == MAP_FAILED && fbdev->buffer_count > 1) {
		fprintf(stderr, "Warning: can't map %zd bytes, doublebuffering disabled\n", fbdev->mem_size);
		fbdev->buffer_count = 1;
		mem_size = fbdev->fb_size;
		fbdev->mem = mmap(0, mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev->fd, 0);
	}
	if (fbdev->mem == MAP_FAILED) {
		fbdev->mem = NULL;
		fbdev->mem_size = 0;
		perror("mmap framebuffer");
		return -1;
	}

	fbdev->mem_size = mem_size;
	return 0;
}

void vout_fbdev_clear(struct vout_fbdev *fbdev)
{
	memset(fbdev->mem, 0, fbdev->mem_size);
}

void vout_fbdev_clear_lines(struct vout_fbdev *fbdev, int y, int count)
{
	int stride = fbdev->fbvar_new.xres_virtual * fbdev->fbvar_new.bits_per_pixel / 8;
	int i;

	if (y + count > fbdev->top_border + fbdev->fbvar_new.yres)
		count = fbdev->top_border + fbdev->fbvar_new.yres - y;

	if (y >= 0 && count > 0)
		for (i = 0; i < fbdev->buffer_count; i++)
			memset((char *)fbdev->mem + fbdev->fb_size * i + y * stride, 0, stride * count);
}

int vout_fbdev_get_fd(struct vout_fbdev *fbdev)
{
	return fbdev->fd;
}

struct vout_fbdev *vout_fbdev_init(const char *fbdev_name, int *w, int *h, int no_dblbuf)
{
	struct vout_fbdev *fbdev;
	int req_w, req_h;
	int ret;

	fbdev = calloc(1, sizeof(*fbdev));
	if (fbdev == NULL)
		return NULL;

	fbdev->fd = open(fbdev_name, O_RDWR);
	if (fbdev->fd == -1) {
		fprintf(stderr, "%s: ", fbdev_name);
		perror("open");
		goto fail_open;
	}

	ret = ioctl(fbdev->fd, FBIOGET_VSCREENINFO, &fbdev->fbvar_old);
	if (ret == -1) {
		perror("FBIOGET_VSCREENINFO ioctl");
		goto fail;
	}

	fbdev->fbvar_new = fbdev->fbvar_old;

	req_w = fbdev->fbvar_new.xres;
	if (*w != 0)
		req_w = *w;
	req_h = fbdev->fbvar_new.yres;
	if (*h != 0)
		req_h = *h;

	ret = vout_fbdev_resize(fbdev, req_w, req_h, 0, 0, 0, 0, no_dblbuf);
	if (ret != 0)
		goto fail;

	printf("%s: %ix%i@%d\n", fbdev_name, fbdev->fbvar_new.xres, fbdev->fbvar_new.yres,
		fbdev->fbvar_new.bits_per_pixel);
	*w = fbdev->fbvar_new.xres;
	*h = fbdev->fbvar_new.yres;

	memset(fbdev->mem, 0, fbdev->mem_size);

	// some checks
	ret = 0;
	ret = ioctl(fbdev->fd, FBIO_WAITFORVSYNC, &ret);
	if (ret != 0)
		fprintf(stderr, "Warning: vsync doesn't seem to be supported\n");

	if (fbdev->buffer_count > 1) {
		fbdev->buffer_write = 0;
		fbdev->fbvar_new.yoffset = fbdev->fbvar_new.yres * (fbdev->buffer_count - 1);
		ret = ioctl(fbdev->fd, FBIOPAN_DISPLAY, &fbdev->fbvar_new);
		if (ret != 0) {
			fbdev->buffer_count = 1;
			fprintf(stderr, "Warning: can't pan display, doublebuffering disabled\n");
		}
	}

	printf("fbdev initialized.\n");
	return fbdev;

fail:
	close(fbdev->fd);
fail_open:
	free(fbdev);
	return NULL;
}

void vout_fbdev_finish(struct vout_fbdev *fbdev)
{
	ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_old);
	if (fbdev->mem != MAP_FAILED)
		munmap(fbdev->mem, fbdev->mem_size);
	if (fbdev->fd >= 0)
		close(fbdev->fd);
	fbdev->mem = NULL;
	fbdev->fd = -1;
	free(fbdev);
}

