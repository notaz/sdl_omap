/*
 * (C) Gražvydas "notaz" Ignotas, 2010, 2011
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
	void *saved_layer;
	int screen_w, screen_h;
	unsigned int xenv_up:1;
};

int   osdl_video_set_mode(struct SDL_PrivateVideoData *pdata,
			  int width, int height, int bpp, int doublebuf);
void *osdl_video_flip(struct SDL_PrivateVideoData *pdata);
int   osdl_video_detect_screen(struct SDL_PrivateVideoData *pdata);
void  osdl_video_finish(struct SDL_PrivateVideoData *pdata);

void omapsdl_input_init(void);
void omapsdl_input_bind(const char *kname, const char *sdlname);
int  omapsdl_input_get_events(int timeout_ms,
		int (*key_cb)(void *cb_arg, int sdl_kc, int is_pressed),
		int (*ts_cb)(void *cb_arg, int x, int y, unsigned int pressure),
		void *cb_arg);
void omapsdl_input_finish(void);

void omapsdl_config(void);
void omapsdl_config_from_env(void);

/* functions for standalone */
void do_clut(void *dest, void *src, unsigned short *pal, int count);

/* config */
extern int gcfg_force_vsync;
extern int gcfg_force_doublebuf;
