/*
 * (C) Gražvydas "notaz" Ignotas, 2010-2012
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define err(fmt, ...) \
	fprintf(stderr, "omapsdl: " fmt "\n", ##__VA_ARGS__)
#define err_perror(fmt, ...) do { \
	fprintf(stderr, "omapsdl: " fmt ": ", ##__VA_ARGS__); \
	perror(NULL); \
} while (0)
#define log(fmt, ...) \
	fprintf(stdout, "omapsdl: " fmt "\n", ##__VA_ARGS__)
#define not_supported() \
	fprintf(stderr, "omapsdl: %s not supported\n", __FUNCTION__)
#if 0
#define trace(fmt, ...) printf(" %s(" fmt ")\n", __FUNCTION__, ##__VA_ARGS__)
#define dbg err
#else
#define trace(...)
#define dbg(...)
#endif

struct SDL_PrivateVideoData {
	struct vout_fbdev *fbdev;
	void *front_buffer;
	void *saved_layer;
	/* physical screen size, should match touchscreen */
	int phys_w, phys_h;
	/* layer */
	int layer_x, layer_y, layer_w, layer_h;
	/* SDL surface borders to hide */
	int border_l, border_r, border_t, border_b;
	/* phys -> layer coord multipliers (16.16) */
	int ts_xmul, ts_ymul;
	/* misc/config */
	unsigned int xenv_up:1;
	unsigned int xenv_mouse:1;
	unsigned int app_uses_flip:1;
	unsigned int cfg_force_vsync:1;
	unsigned int cfg_force_doublebuf:1;
	unsigned int cfg_force_directbuf:1;
	unsigned int cfg_no_ts_translate:1;
	unsigned int cfg_ts_force_tslib:1;
};

void *osdl_video_set_mode(struct SDL_PrivateVideoData *pdata,
			  int border_l, int border_r, int border_t, int border_b,
			  int width, int height, int bpp, int *doublebuf,
			  const char *wm_title);
void *osdl_video_flip(struct SDL_PrivateVideoData *pdata);
void *osdl_video_get_active_buffer(struct SDL_PrivateVideoData *pdata);
int   osdl_video_detect_screen(struct SDL_PrivateVideoData *pdata);
int   osdl_video_pause(struct SDL_PrivateVideoData *pdata, int is_pause);
void  osdl_video_finish(struct SDL_PrivateVideoData *pdata);

void omapsdl_input_init(void);
void omapsdl_input_bind(const char *kname, const char *sdlname);
int  omapsdl_input_get_events(int timeout_ms,
		int (*key_cb)(void *cb_arg, int sdl_kc, int sdl_sc, int is_pressed),
		int (*ts_cb)(void *cb_arg, int x, int y, unsigned int pressure),
		void *cb_arg);
void omapsdl_input_finish(void);

void omapsdl_config(struct SDL_PrivateVideoData *pdata);
void omapsdl_config_from_env(struct SDL_PrivateVideoData *pdata);

/* functions for standalone */
void do_clut(void *dest, void *src, unsigned short *pal, int count);

