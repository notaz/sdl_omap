
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define err(fmt, ...) fprintf(stderr, "psdl: " fmt "\n", ##__VA_ARGS__)
#define not_supported() fprintf(stderr, "psdl: %s not supported\n", __FUNCTION__)
#if 0
#define trace(fmt, ...) printf(" %s(" fmt ")\n", __FUNCTION__, ##__VA_ARGS__)
#define dbg err
#else
#define trace(...)
#define dbg(...)
#endif

void pmsdl_input_init(void);
void pmsdl_input_bind(const char *kname, const char *sdlname);

void do_clut(void *dest, void *src, unsigned short *pal, int count);
