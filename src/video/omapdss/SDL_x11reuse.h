
struct x11reuse_context *x11reuse_init(void);
void x11reuse_SetCaption(struct x11reuse_context *context,
	void *display, int screen, void *window,
	const char *title, const char *icon);
void x11reuse_SetIcon(struct x11reuse_context *context,
	void *display, int screen, void *window,
	void *icon, void *mask);
