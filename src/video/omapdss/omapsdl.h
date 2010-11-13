
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define err(fmt, ...) fprintf(stderr, "omapsdl: " fmt "\n", ##__VA_ARGS__)
#define not_supported() fprintf(stderr, "omapsdl: %s not supported\n", __FUNCTION__)
#if 0
#define trace(fmt, ...) printf(" %s(" fmt ")\n", __FUNCTION__, ##__VA_ARGS__)
#define dbg err
#else
#define trace(...)
#define dbg(...)
#endif

void omapsdl_input_init(void);
void omapsdl_input_bind(const char *kname, const char *sdlname);
int  omapsdl_input_get_events(int timeout,
		int (*cb)(void *cb_arg, int sdl_kc, int is_pressed), void *cb_arg);

void omapsdl_config(void);

/* functions for standalone */
void do_clut(void *dest, void *src, unsigned short *pal, int count);

/* config */
extern int gcfg_force_vsync;
