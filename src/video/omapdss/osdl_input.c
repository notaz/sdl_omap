/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <linux/input.h>
#ifdef STANDALONE
#include <SDL/SDL.h>
#else
#include "SDL.h"
#endif
#if SDL_INPUT_TSLIB
#include <tslib.h>
#endif

#include "osdl.h"

/* XXX: these should go to private data */
static int osdl_evdev_devs[32];
static int osdl_evdev_dev_count;
static int osdl_tslib_fd;
static struct tsdev *osdl_tslib_dev;

static short osdl_evdev_map[KEY_CNT] = {
	/*       normal                              fn                      */
	[KEY_0]         = SDLK_0,         [KEY_F10]       = SDLK_F10,
	[KEY_1]         = SDLK_1,         [KEY_F1]        = SDLK_F1,
	[KEY_2]         = SDLK_2,         [KEY_F2]        = SDLK_F2,
	[KEY_3]         = SDLK_3,         [KEY_F3]        = SDLK_F3,
	[KEY_4]         = SDLK_4,         [KEY_F4]        = SDLK_F4,
	[KEY_5]         = SDLK_5,         [KEY_F5]        = SDLK_F5,
	[KEY_6]         = SDLK_6,         [KEY_F6]        = SDLK_F6,
	[KEY_7]         = SDLK_7,         [KEY_F7]        = SDLK_F7,
	[KEY_8]         = SDLK_8,         [KEY_F8]        = SDLK_F8,
	[KEY_9]         = SDLK_9,         [KEY_F9]        = SDLK_F9,
	[KEY_A]         = SDLK_a,         [KEY_APOSTROPHE]= SDLK_QUOTE,     /* ' */
	[KEY_B]         = SDLK_b,         [KEY_F14]       = 124,            /* | */
	[KEY_C]         = SDLK_c,         [KEY_BACKSLASH] = SDLK_BACKSLASH, /* \ */
	[KEY_D]         = SDLK_d,         [KEY_KPMINUS]   = SDLK_MINUS,
	[KEY_E]         = SDLK_e,         [KEY_LEFTBRACE] = SDLK_LEFTPAREN,
	[KEY_F]         = SDLK_f,         [KEY_KPPLUS]    = SDLK_PLUS,
	[KEY_G]         = SDLK_g,         [KEY_EQUAL]     = SDLK_EQUALS,
	[KEY_H]         = SDLK_h,         [KEY_GRAVE]     = SDLK_BACKQUOTE, /* ` */
	[KEY_I]         = SDLK_i,      /* [KEY_BRIGHTNESSUP] */
	[KEY_J]         = SDLK_j,         [KEY_F13]       = SDLK_WORLD_4,   /* ’ (not on def SDL) */
	[KEY_K]         = SDLK_k,         [KEY_F18]       = SDLK_WORLD_3,   /* £ (pound) */
	[KEY_L]         = SDLK_l,         [KEY_YEN]       = SDLK_WORLD_5,
	[KEY_M]         = SDLK_m,         [KEY_F23]       = SDLK_EURO,
	[KEY_N]         = SDLK_n,         [KEY_F22]       = SDLK_DOLLAR,    /* $ */
	[KEY_O]         = SDLK_o,         [KEY_F11]       = SDLK_F11,
	[KEY_P]         = SDLK_p,         [KEY_F12]       = SDLK_F12,
	[KEY_Q]         = SDLK_q,         [KEY_ESC]       = SDLK_ESCAPE,
	[KEY_R]         = SDLK_r,         [KEY_RIGHTBRACE]= SDLK_RIGHTPAREN,
	[KEY_S]         = SDLK_s,         [KEY_F19]       = SDLK_QUOTEDBL,  /* " */
	[KEY_T]         = SDLK_t,         [KEY_F17]       = SDLK_EXCLAIM,   /* ! */
	[KEY_U]         = SDLK_u,      /* [KEY_BRIGHTNESSDOWN] */
	[KEY_V]         = SDLK_v,         [KEY_F16]       = SDLK_HASH,      /* # (pound/hash) */
	[KEY_W]         = SDLK_w,         [KEY_F20]       = SDLK_AT,        /* @ */
	[KEY_X]         = SDLK_x,         [KEY_QUESTION]  = SDLK_QUESTION,  /* ? */
	[KEY_Y]         = SDLK_y,         [KEY_F15]       = SDLK_UNDERSCORE,/* _ */
	[KEY_Z]         = SDLK_z,         [KEY_SLASH]     = SDLK_SLASH,     /* / */
	[KEY_SPACE]     = SDLK_SPACE,     [KEY_TAB]       = SDLK_TAB,
	[KEY_BACKSPACE] = SDLK_BACKSPACE, [KEY_INSERT]    = SDLK_INSERT,
	[KEY_FN]        = SDLK_WORLD_95,
	[KEY_DOT]       = SDLK_PERIOD,    [KEY_F21]       = SDLK_COLON,     /* : */
	[KEY_ENTER]     = SDLK_RETURN,
	[KEY_LEFTSHIFT] = SDLK_LSHIFT,    [KEY_CAPSLOCK]  = SDLK_CAPSLOCK,
	[KEY_COMMA]     = SDLK_COMMA,     [KEY_SEMICOLON] = SDLK_SEMICOLON, /* ; */

	[KEY_UP]	= SDLK_UP,
	[KEY_DOWN]	= SDLK_DOWN,
	[KEY_LEFT]	= SDLK_LEFT,
	[KEY_RIGHT]	= SDLK_RIGHT,
	[KEY_HOME]	= SDLK_HOME,
	[KEY_END]	= SDLK_END,
	[KEY_PAGEUP]	= SDLK_PAGEUP,
	[KEY_PAGEDOWN]	= SDLK_PAGEDOWN,
	[KEY_LEFTALT]	= SDLK_LALT,
	[KEY_LEFTCTRL]	= SDLK_LCTRL,
	[KEY_MENU]	= 147,		/* match default SDL here */
	[KEY_RIGHTSHIFT]= SDLK_RSHIFT,
	[KEY_RIGHTCTRL]	= SDLK_RCTRL,
};

static const char * const osdl_evdev_keys[KEY_CNT] = {
	[KEY_RESERVED] = "Reserved",		[KEY_ESC] = "Esc",
	[KEY_1] = "1",				[KEY_2] = "2",
	[KEY_3] = "3",				[KEY_4] = "4",
	[KEY_5] = "5",				[KEY_6] = "6",
	[KEY_7] = "7",				[KEY_8] = "8",
	[KEY_9] = "9",				[KEY_0] = "0",
	[KEY_MINUS] = "Minus",			[KEY_EQUAL] = "Equal",
	[KEY_BACKSPACE] = "Backspace",		[KEY_TAB] = "Tab",
	[KEY_Q] = "Q",				[KEY_W] = "W",
	[KEY_E] = "E",				[KEY_R] = "R",
	[KEY_T] = "T",				[KEY_Y] = "Y",
	[KEY_U] = "U",				[KEY_I] = "I",
	[KEY_O] = "O",				[KEY_P] = "P",
	[KEY_LEFTBRACE] = "LeftBrace",		[KEY_RIGHTBRACE] = "RightBrace",
	[KEY_ENTER] = "Enter",			[KEY_LEFTCTRL] = "LeftControl",
	[KEY_A] = "A",				[KEY_S] = "S",
	[KEY_D] = "D",				[KEY_F] = "F",
	[KEY_G] = "G",				[KEY_H] = "H",
	[KEY_J] = "J",				[KEY_K] = "K",
	[KEY_L] = "L",				[KEY_SEMICOLON] = "Semicolon",
	[KEY_APOSTROPHE] = "Apostrophe",	[KEY_GRAVE] = "Grave",
	[KEY_LEFTSHIFT] = "LeftShift",		[KEY_BACKSLASH] = "BackSlash",
	[KEY_Z] = "Z",				[KEY_X] = "X",
	[KEY_C] = "C",				[KEY_V] = "V",
	[KEY_B] = "B",				[KEY_N] = "N",
	[KEY_M] = "M",				[KEY_COMMA] = "Comma",
	[KEY_DOT] = "Dot",			[KEY_SLASH] = "Slash",
	[KEY_RIGHTSHIFT] = "RightShift",	[KEY_KPASTERISK] = "KPAsterisk",
	[KEY_LEFTALT] = "LeftAlt",		[KEY_SPACE] = "Space",
	[KEY_CAPSLOCK] = "CapsLock",		[KEY_F1] = "F1",
	[KEY_F2] = "F2",			[KEY_F3] = "F3",
	[KEY_F4] = "F4",			[KEY_F5] = "F5",
	[KEY_F6] = "F6",			[KEY_F7] = "F7",
	[KEY_F8] = "F8",			[KEY_F9] = "F9",
	[KEY_F10] = "F10",			[KEY_NUMLOCK] = "NumLock",
	[KEY_SCROLLLOCK] = "ScrollLock",	[KEY_KP7] = "KP7",
	[KEY_KP8] = "KP8",			[KEY_KP9] = "KP9",
	[KEY_KPMINUS] = "KPMinus",		[KEY_KP4] = "KP4",
	[KEY_KP5] = "KP5",			[KEY_KP6] = "KP6",
	[KEY_KPPLUS] = "KPPlus",		[KEY_KP1] = "KP1",
	[KEY_KP2] = "KP2",			[KEY_KP3] = "KP3",
	[KEY_KP0] = "KP0",			[KEY_KPDOT] = "KPDot",
	[KEY_ZENKAKUHANKAKU] = "Zenkaku/Hankaku", [KEY_102ND] = "102nd",
	[KEY_F11] = "F11",			[KEY_F12] = "F12",
	[KEY_KPJPCOMMA] = "KPJpComma",		[KEY_KPENTER] = "KPEnter",
	[KEY_RIGHTCTRL] = "RightCtrl",		[KEY_KPSLASH] = "KPSlash",
	[KEY_SYSRQ] = "SysRq",			[KEY_RIGHTALT] = "RightAlt",
	[KEY_LINEFEED] = "LineFeed",		[KEY_HOME] = "Home",
	[KEY_UP] = "Up",			[KEY_PAGEUP] = "PageUp",
	[KEY_LEFT] = "Left",			[KEY_RIGHT] = "Right",
	[KEY_END] = "End",			[KEY_DOWN] = "Down",
	[KEY_PAGEDOWN] = "PageDown",		[KEY_INSERT] = "Insert",
	[KEY_DELETE] = "Delete",		[KEY_MACRO] = "Macro",
	[KEY_HELP] = "Help",			[KEY_MENU] = "Menu",
	[KEY_COFFEE] = "Coffee",		[KEY_DIRECTION] = "Direction",
	[BTN_0] = "Btn0",			[BTN_1] = "Btn1",
	[BTN_2] = "Btn2",			[BTN_3] = "Btn3",
	[BTN_4] = "Btn4",			[BTN_5] = "Btn5",
	[BTN_6] = "Btn6",			[BTN_7] = "Btn7",
	[BTN_8] = "Btn8",			[BTN_9] = "Btn9",
	[BTN_LEFT] = "LeftBtn",			[BTN_RIGHT] = "RightBtn",
	[BTN_MIDDLE] = "MiddleBtn",		[BTN_SIDE] = "SideBtn",
	[BTN_EXTRA] = "ExtraBtn",		[BTN_FORWARD] = "ForwardBtn",
	[BTN_BACK] = "BackBtn",			[BTN_TASK] = "TaskBtn",
	[BTN_TRIGGER] = "Trigger",		[BTN_THUMB] = "ThumbBtn",
	[BTN_THUMB2] = "ThumbBtn2",		[BTN_TOP] = "TopBtn",
	[BTN_TOP2] = "TopBtn2",			[BTN_PINKIE] = "PinkieBtn",
	[BTN_BASE] = "BaseBtn",			[BTN_BASE2] = "BaseBtn2",
	[BTN_BASE3] = "BaseBtn3",		[BTN_BASE4] = "BaseBtn4",
	[BTN_BASE5] = "BaseBtn5",		[BTN_BASE6] = "BaseBtn6",
	[BTN_DEAD] = "BtnDead",			[BTN_A] = "BtnA",
	[BTN_B] = "BtnB",			[BTN_C] = "BtnC",
	[BTN_X] = "BtnX",			[BTN_Y] = "BtnY",
	[BTN_Z] = "BtnZ",			[BTN_TL] = "BtnTL",
	[BTN_TR] = "BtnTR",			[BTN_TL2] = "BtnTL2",
	[BTN_TR2] = "BtnTR2",			[BTN_SELECT] = "BtnSelect",
	[BTN_START] = "BtnStart",		[BTN_MODE] = "BtnMode",
	[BTN_THUMBL] = "BtnThumbL",		[BTN_THUMBR] = "BtnThumbR",
	[BTN_TOUCH] = "Touch",			[BTN_STYLUS] = "Stylus",
	[BTN_STYLUS2] = "Stylus2",		[BTN_TOOL_DOUBLETAP] = "Tool Doubletap",
	[BTN_TOOL_TRIPLETAP] = "Tool Tripletap", [BTN_GEAR_DOWN] = "WheelBtn",
	[BTN_GEAR_UP] = "Gear up",		[KEY_OK] = "Ok",
};

#define DNKEY(x) [SDLK_##x] = #x
static const char * const sdl_keynames[SDLK_LAST] = {
	DNKEY(BACKSPACE),
	DNKEY(TAB),
	DNKEY(RETURN),
	DNKEY(ESCAPE),
	DNKEY(SPACE),
	DNKEY(EXCLAIM),
	DNKEY(QUOTEDBL),
	DNKEY(HASH),
	DNKEY(DOLLAR),
	DNKEY(AMPERSAND),
	DNKEY(QUOTE),
	DNKEY(LEFTPAREN),
	DNKEY(RIGHTPAREN),
	DNKEY(ASTERISK),
	DNKEY(PLUS),
	DNKEY(COMMA),
	DNKEY(MINUS),
	DNKEY(PERIOD),
	DNKEY(SLASH),
	DNKEY(0),
	DNKEY(1),
	DNKEY(2),
	DNKEY(3),
	DNKEY(4),
	DNKEY(5),
	DNKEY(6),
	DNKEY(7),
	DNKEY(8),
	DNKEY(9),
	DNKEY(COLON),
	DNKEY(SEMICOLON),
	DNKEY(LESS),
	DNKEY(EQUALS),
	DNKEY(GREATER),
	DNKEY(QUESTION),
	DNKEY(AT),
	DNKEY(LEFTBRACKET),
	DNKEY(BACKSLASH),
	DNKEY(RIGHTBRACKET),
	DNKEY(CARET),
	DNKEY(UNDERSCORE),
	DNKEY(BACKQUOTE),
	DNKEY(a),
	DNKEY(b),
	DNKEY(c),
	DNKEY(d),
	DNKEY(e),
	DNKEY(f),
	DNKEY(g),
	DNKEY(h),
	DNKEY(i),
	DNKEY(j),
	DNKEY(k),
	DNKEY(l),
	DNKEY(m),
	DNKEY(n),
	DNKEY(o),
	DNKEY(p),
	DNKEY(q),
	DNKEY(r),
	DNKEY(s),
	DNKEY(t),
	DNKEY(u),
	DNKEY(v),
	DNKEY(w),
	DNKEY(x),
	DNKEY(y),
	DNKEY(z),
	DNKEY(DELETE),
	DNKEY(WORLD_0),
	DNKEY(WORLD_1),
	DNKEY(WORLD_2),
	DNKEY(WORLD_3),
	DNKEY(WORLD_4),
	DNKEY(WORLD_5),
	DNKEY(WORLD_6),
	DNKEY(WORLD_7),
	DNKEY(WORLD_8),
	DNKEY(WORLD_9),
	DNKEY(WORLD_10),
	DNKEY(WORLD_11),
	DNKEY(WORLD_12),
	DNKEY(WORLD_13),
	DNKEY(WORLD_14),
	DNKEY(WORLD_15),
	DNKEY(WORLD_16),
	DNKEY(WORLD_17),
	DNKEY(WORLD_18),
	DNKEY(WORLD_19),
	DNKEY(WORLD_20),
	DNKEY(WORLD_21),
	DNKEY(WORLD_22),
	DNKEY(WORLD_23),
	DNKEY(WORLD_24),
	DNKEY(WORLD_25),
	DNKEY(WORLD_26),
	DNKEY(WORLD_27),
	DNKEY(WORLD_28),
	DNKEY(WORLD_29),
	DNKEY(WORLD_30),
	DNKEY(WORLD_31),
	DNKEY(WORLD_32),
	DNKEY(WORLD_33),
	DNKEY(WORLD_34),
	DNKEY(WORLD_35),
	DNKEY(WORLD_36),
	DNKEY(WORLD_37),
	DNKEY(WORLD_38),
	DNKEY(WORLD_39),
	DNKEY(WORLD_40),
	DNKEY(WORLD_41),
	DNKEY(WORLD_42),
	DNKEY(WORLD_43),
	DNKEY(WORLD_44),
	DNKEY(WORLD_45),
	DNKEY(WORLD_46),
	DNKEY(WORLD_47),
	DNKEY(WORLD_48),
	DNKEY(WORLD_49),
	DNKEY(WORLD_50),
	DNKEY(WORLD_51),
	DNKEY(WORLD_52),
	DNKEY(WORLD_53),
	DNKEY(WORLD_54),
	DNKEY(WORLD_55),
	DNKEY(WORLD_56),
	DNKEY(WORLD_57),
	DNKEY(WORLD_58),
	DNKEY(WORLD_59),
	DNKEY(WORLD_60),
	DNKEY(WORLD_61),
	DNKEY(WORLD_62),
	DNKEY(WORLD_63),
	DNKEY(WORLD_64),
	DNKEY(WORLD_65),
	DNKEY(WORLD_66),
	DNKEY(WORLD_67),
	DNKEY(WORLD_68),
	DNKEY(WORLD_69),
	DNKEY(WORLD_70),
	DNKEY(WORLD_71),
	DNKEY(WORLD_72),
	DNKEY(WORLD_73),
	DNKEY(WORLD_74),
	DNKEY(WORLD_75),
	DNKEY(WORLD_76),
	DNKEY(WORLD_77),
	DNKEY(WORLD_78),
	DNKEY(WORLD_79),
	DNKEY(WORLD_80),
	DNKEY(WORLD_81),
	DNKEY(WORLD_82),
	DNKEY(WORLD_83),
	DNKEY(WORLD_84),
	DNKEY(WORLD_85),
	DNKEY(WORLD_86),
	DNKEY(WORLD_87),
	DNKEY(WORLD_88),
	DNKEY(WORLD_89),
	DNKEY(WORLD_90),
	DNKEY(WORLD_91),
	DNKEY(WORLD_92),
	DNKEY(WORLD_93),
	DNKEY(WORLD_94),
	DNKEY(WORLD_95),
	DNKEY(KP0),
	DNKEY(KP1),
	DNKEY(KP2),
	DNKEY(KP3),
	DNKEY(KP4),
	DNKEY(KP5),
	DNKEY(KP6),
	DNKEY(KP7),
	DNKEY(KP8),
	DNKEY(KP9),
	DNKEY(KP_PERIOD),
	DNKEY(KP_DIVIDE),
	DNKEY(KP_MULTIPLY),
	DNKEY(KP_MINUS),
	DNKEY(KP_PLUS),
	DNKEY(KP_ENTER),
	DNKEY(KP_EQUALS),
	DNKEY(UP),
	DNKEY(DOWN),
	DNKEY(RIGHT),
	DNKEY(LEFT),
	DNKEY(INSERT),
	DNKEY(HOME),
	DNKEY(END),
	DNKEY(PAGEUP),
	DNKEY(PAGEDOWN),
	DNKEY(F1),
	DNKEY(F2),
	DNKEY(F3),
	DNKEY(F4),
	DNKEY(F5),
	DNKEY(F6),
	DNKEY(F7),
	DNKEY(F8),
	DNKEY(F9),
	DNKEY(F10),
	DNKEY(F11),
	DNKEY(F12),
	DNKEY(F13),
	DNKEY(F14),
	DNKEY(F15),
	DNKEY(NUMLOCK),
	DNKEY(CAPSLOCK),
	DNKEY(SCROLLOCK),
	DNKEY(RSHIFT),
	DNKEY(LSHIFT),
	DNKEY(RCTRL),
	DNKEY(LCTRL),
	DNKEY(RALT),
	DNKEY(LALT),
	DNKEY(RMETA),
	DNKEY(LMETA),
	DNKEY(LSUPER),
	DNKEY(RSUPER),
	DNKEY(MODE),
	DNKEY(COMPOSE),
};

void omapsdl_input_bind(const char *kname, const char *sdlname)
{
	int i, kc;

	if (kname == NULL || strncasecmp(kname, "ev_", 3) != 0)
		goto bad_ev_key;

	for (i = 0; i < ARRAY_SIZE(osdl_evdev_keys); i++) {
		if (osdl_evdev_keys[i] == NULL)
			continue;
		if (strcasecmp(osdl_evdev_keys[i], kname + 3) == 0)
			break;
	}

	if (i >= ARRAY_SIZE(osdl_evdev_keys))
		goto bad_ev_key;
	kc = i;

	if (sdlname == NULL || strncasecmp(sdlname, "SDLK_", 5) != 0)
		goto bad_sdlkey;

	for (i = 0; i < ARRAY_SIZE(sdl_keynames); i++) {
		if (sdl_keynames[i] == NULL)
			continue;
		if (strcasecmp(sdl_keynames[i], sdlname + 5) == 0)
			break;
	}

	if (i >= ARRAY_SIZE(sdl_keynames))
		goto bad_sdlkey;

	osdl_evdev_map[kc] = i;
	return;

bad_ev_key:
	err("can't resolve evdev key '%s'", kname);
	return;

bad_sdlkey:
	err("can't resolve SDL key '%s'", sdlname);
}

#define KEYBITS_BIT(keybits, x) (keybits[(x)/sizeof(keybits[0])/8] & \
	(1 << ((x) & (sizeof(keybits[0])*8-1))))

void omapsdl_input_init(void)
{
	long keybits[KEY_CNT / sizeof(long) / 8];
	ino_t touchscreen_ino = (ino_t)-1;
	struct stat stat_buf;
	int i;

#if SDL_INPUT_TSLIB
	/* start with touchscreen so that we can skip it later */
	osdl_tslib_dev = ts_open(SDL_getenv("TSLIB_TSDEVICE"), 1);
	if (ts_config(osdl_tslib_dev) < 0) {
		ts_close(osdl_tslib_dev);
		osdl_tslib_dev = NULL;
	}
	if (osdl_tslib_dev != NULL) {
		osdl_tslib_fd = ts_fd(osdl_tslib_dev);
		osdl_evdev_devs[osdl_evdev_dev_count++] = osdl_tslib_fd;
		if (fstat(osdl_tslib_fd, &stat_buf) == -1)
			err_perror("fstat ts");
		else
			touchscreen_ino = stat_buf.st_ino;
		log("opened tslib touchscreen");
	}
#endif

	/* the kernel might support and return less keys then we know about,
	 * so make sure the buffer is clear. */
	memset(keybits, 0, sizeof(keybits));

	for (i = 0;; i++)
	{
		int support = 0, count = 0;
		int u, ret, fd;
		char name[64];

		snprintf(name, sizeof(name), "/dev/input/event%d", i);
		fd = open(name, O_RDONLY|O_NONBLOCK);
		if (fd == -1) {
			if (errno == EACCES)
				continue;	/* maybe we can access next one */
			break;
		}

		/* touchscreen check */
		if (touchscreen_ino != (dev_t)-1) {
			if (fstat(fd, &stat_buf) == -1)
				err_perror("fstat");
			else if (touchscreen_ino == stat_buf.st_ino) {
				log("skip %s as ts", name);
				goto skip;
			}
		}

		/* check supported events */
		ret = ioctl(fd, EVIOCGBIT(0, sizeof(support)), &support);
		if (ret == -1) {
			err_perror("in_evdev: ioctl failed on %s", name);
			goto skip;
		}

		if (!(support & (1 << EV_KEY)))
			goto skip;

		ret = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
		if (ret == -1) {
			err_perror("in_evdev: ioctl failed on %s", name);
			goto skip;
		}

		/* check for interesting keys */
		for (u = 0; u < KEY_CNT; u++) {
			if (KEYBITS_BIT(keybits, u)) {
				if (u != KEY_POWER && u != KEY_SLEEP && u != BTN_TOUCH)
					count++;
			}
		}

		if (count == 0)
			goto skip;

		osdl_evdev_devs[osdl_evdev_dev_count++] = fd;
		ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		log("in_evdev: found \"%s\" with %d events (type %08x)",
			name, count, support);
		continue;

skip:
		close(fd);
	}

	log("found %d evdev device(s).", osdl_evdev_dev_count);
}

void omapsdl_input_finish(void)
{
	int i;

#if SDL_INPUT_TSLIB
	if (osdl_tslib_dev != NULL)
		ts_close(osdl_tslib_dev);
#endif
	osdl_tslib_dev = NULL;

	for (i = 0; i < osdl_evdev_dev_count; i++) {
		if (osdl_evdev_devs[i] != osdl_tslib_fd)
			close(osdl_evdev_devs[i]);
	}
	osdl_evdev_dev_count = 0;
	osdl_tslib_fd = 0;
}

int omapsdl_input_get_events(int timeout_ms,
		int (*key_cb)(void *cb_arg, int sdl_kc, int is_pressed),
		int (*ts_cb)(void *cb_arg, int x, int y, unsigned int pressure),
		void *cb_arg)
{
	struct timeval tv, *timeout = NULL;
	struct input_event ev;
	int i, fdmax = -1;
	fd_set fdset;

	if (timeout_ms >= 0) {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
		timeout = &tv;
	}

	FD_ZERO(&fdset);
	for (i = 0; i < osdl_evdev_dev_count; i++) {
		if (osdl_evdev_devs[i] > fdmax)
			fdmax = osdl_evdev_devs[i];
		FD_SET(osdl_evdev_devs[i], &fdset);
	}

	while (1)
	{
		int fd, ret, sdl_kc;

		ret = select(fdmax + 1, &fdset, NULL, NULL, timeout);
		if (ret == -1)
		{
			err_perror("in_evdev: select failed");
			return -1;
		}
		else if (ret == 0)
			return -1; /* timeout */

		for (i = 0; i < osdl_evdev_dev_count; i++) {
			if (!FD_ISSET(osdl_evdev_devs[i], &fdset))
				continue;

			fd = osdl_evdev_devs[i];
#if SDL_INPUT_TSLIB
			if (fd == osdl_tslib_fd && ts_cb != NULL) {
				while (1) {
					struct ts_sample tss;
					ret = ts_read(osdl_tslib_dev, &tss, 1);
					if (ret <= 0)
						break;
					ret = ts_cb(cb_arg, tss.x, tss.y, tss.pressure);
					if (ret != 0)
						return ret;
				}
				continue;
			}
			/* else read below will consume the event, even if it's from ts */
#endif

			while (1) {
				ret = read(fd, &ev, sizeof(ev));
				if (ret < (int)sizeof(ev)) {
					if (errno != EAGAIN) {
						err_perror("in_evdev: read failed");
						return -1;
					}
					break;
				}

				if (ev.type != EV_KEY)
					continue; /* not key event */
				if ((unsigned int)ev.value > 1)
					continue; /* not key up/down */
				if ((unsigned int)ev.code >= ARRAY_SIZE(osdl_evdev_map))
					continue; /* keycode from future */
				sdl_kc = osdl_evdev_map[ev.code];
				if (sdl_kc == 0)
					continue; /* not mapped */
				ret = key_cb(cb_arg, sdl_kc, ev.value);
				if (ret != 0)
					return ret;
			}
		}
	}
}

/* SDL */
#ifdef STANDALONE

static unsigned char g_keystate[SDLK_LAST];

struct key_event {
	int sdl_kc;
	int is_pressed;
};

static int do_key_cb(void *cb_arg, int sdl_kc, int is_pressed)
{
	struct key_event *ev = cb_arg;
	ev->sdl_kc = sdl_kc;
	ev->is_pressed = is_pressed;

	return 1; /* done */
}

static int do_event(SDL_Event *event, int timeout)
{
	struct key_event ev;
	int ret;

	ret = omapsdl_input_get_events(timeout, do_key_cb, NULL, &ev);
	if (ret < 0)
		return 0;

	g_keystate[ev.sdl_kc] = ev.is_pressed;

	if (event == NULL)
		return 1; // FIXME: ..but the event is lost

	memset(event, 0, sizeof(event->key));
	event->type = ev.is_pressed ? SDL_KEYDOWN : SDL_KEYUP;
	// event->key.which =
	event->key.state = ev.is_pressed ? SDL_PRESSED : SDL_RELEASED;
	event->key.keysym.sym = ev.sdl_kc;
	// event->key.keysym.mod

	return 1;
}

DECLSPEC int SDLCALL
SDL_WaitEvent(SDL_Event *event)
{
	trace("%p", event);

	return do_event(event, -1);
}

DECLSPEC int SDLCALL
SDL_PollEvent(SDL_Event *event)
{
	trace("%p", event);

	return do_event(event, 0);
}

DECLSPEC Uint8 * SDLCALL
SDL_GetKeyState(int *numkeys)
{
	trace("%p", numkeys);

	if (numkeys != NULL)
		*numkeys = ARRAY_SIZE(g_keystate);
	return g_keystate;
}

DECLSPEC char * SDLCALL
SDL_GetKeyName(SDLKey key)
{
	trace("%d", key);

	if ((unsigned int)key < ARRAY_SIZE(sdl_keynames))
		return (char *)sdl_keynames[key];
	else
		return "";
}

DECLSPEC int SDLCALL
SDL_EnableUNICODE(int enable)
{
	trace("%d", enable);

	if (enable > 0)
		not_supported();

	return 0;
}

DECLSPEC int SDLCALL
SDL_EnableKeyRepeat(int delay, int interval)
{
	trace("%d, %d", delay, interval);

	if (delay)
		not_supported();
	return 0;
}

DECLSPEC void SDLCALL
SDL_GetKeyRepeat(int *delay, int *interval)
{
	trace("%p, %p", delay, interval);

	if (delay)
		*delay = 0;
	if (interval)
		*interval = 0;
}

DECLSPEC SDLMod SDLCALL
SDL_GetModState(void)
{
	static const short syms[] = { SDLK_LSHIFT, SDLK_RSHIFT, SDLK_LCTRL, SDLK_RCTRL, SDLK_LALT,
				SDLK_RALT, SDLK_LMETA, SDLK_RMETA, SDLK_NUMLOCK, SDLK_CAPSLOCK, SDLK_MODE };
	static const short mods[] = { KMOD_LSHIFT, KMOD_RSHIFT, KMOD_LCTRL, KMOD_RCTRL, KMOD_LALT,
				KMOD_RALT, KMOD_LMETA, KMOD_RMETA, KMOD_NUM, KMOD_CAPS, KMOD_MODE };
	SDLMod ret = KMOD_NONE;
	int i;

	trace("");

	for (i = 0; i < ARRAY_SIZE(syms); i++)
		if (g_keystate[syms[i]])
			ret |= mods[i];

	return ret;
}

/* Joysticks */
DECLSPEC int SDLCALL
SDL_NumJoysticks(void)
{
	trace("");
	return 0;
}

DECLSPEC SDL_Joystick * SDLCALL
SDL_JoystickOpen(int device_index)
{
	trace("%d", device_index);
	return NULL;
}

#endif // STANDALONE
