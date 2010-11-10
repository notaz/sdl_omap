/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <strings.h>
#include <SDL/SDL.h>
#include <linux/input.h>

#include "omapsdl.h"
#include "common/input.h"

static unsigned char g_keystate[SDLK_LAST];

static short pmsdl_map[KEY_CNT] = {
	[KEY_0]		= SDLK_0,
	[KEY_1]		= SDLK_1,
	[KEY_2]		= SDLK_2,
	[KEY_3]		= SDLK_3,
	[KEY_4]		= SDLK_4,
	[KEY_5]		= SDLK_5,
	[KEY_6]		= SDLK_6,
	[KEY_7]		= SDLK_7,
	[KEY_8]		= SDLK_8,
	[KEY_9]		= SDLK_9,
	[KEY_A]		= SDLK_a,
	[KEY_B]		= SDLK_b,
	[KEY_C]		= SDLK_c,
	[KEY_D]		= SDLK_d,
	[KEY_E]		= SDLK_e,
	[KEY_F]		= SDLK_f,
	[KEY_G]		= SDLK_g,
	[KEY_H]		= SDLK_h,
	[KEY_I]		= SDLK_i,
	[KEY_J]		= SDLK_j,
	[KEY_K]		= SDLK_k,
	[KEY_L]		= SDLK_l,
	[KEY_M]		= SDLK_m,
	[KEY_N]		= SDLK_n,
	[KEY_O]		= SDLK_o,
	[KEY_P]		= SDLK_p,
	[KEY_Q]		= SDLK_q,
	[KEY_R]		= SDLK_r,
	[KEY_S]		= SDLK_s,
	[KEY_T]		= SDLK_t,
	[KEY_U]		= SDLK_u,
	[KEY_V]		= SDLK_v,
	[KEY_W]		= SDLK_w,
	[KEY_X]		= SDLK_x,
	[KEY_Y]		= SDLK_y,
	[KEY_Z]		= SDLK_z,
	[KEY_SPACE]	= SDLK_SPACE,
	[KEY_BACKSPACE]	= SDLK_BACKSPACE,
	[KEY_FN]	= SDLK_MODE,
	[KEY_DOT]	= SDLK_PERIOD,
	[KEY_ENTER]	= SDLK_RETURN,
	[KEY_LEFTSHIFT]	= SDLK_LSHIFT,
	[KEY_COMMA]	= SDLK_COMMA,
//	[KEY_BRIGHTNESSUP]	=
//	[KEY_BRIGHTNESSDOWN]	=
//	[KEY_GRAVE]	=
	[KEY_TAB]	= SDLK_TAB,
	[KEY_INSERT]	= SDLK_INSERT,
	[KEY_EQUAL]	= SDLK_EQUALS,
	[KEY_KPPLUS]	= SDLK_KP_PLUS,
	[KEY_BACKSLASH]	= SDLK_BACKSLASH,
	[KEY_RIGHTBRACE]= SDLK_RIGHTBRACKET,
	[KEY_KPMINUS]	= SDLK_KP_MINUS,
	[KEY_QUESTION]	= SDLK_QUESTION,
	[KEY_LEFTBRACE]	= SDLK_LEFTBRACKET,
	[KEY_SLASH]	= SDLK_SLASH,
//	[KEY_YEN]	=
	[KEY_APOSTROPHE]= SDLK_QUOTE,
	[KEY_ESC]	= SDLK_ESCAPE,
	[KEY_CAPSLOCK]	= SDLK_CAPSLOCK,
	[KEY_SEMICOLON]	= SDLK_SEMICOLON,
	[KEY_F1]	= SDLK_F1,
	[KEY_F2]	= SDLK_F2,
	[KEY_F3]	= SDLK_F3,
	[KEY_F4]	= SDLK_F4,
	[KEY_F5]	= SDLK_F5,
	[KEY_F6]	= SDLK_F6,
	[KEY_F7]	= SDLK_F7,
	[KEY_F8]	= SDLK_F8,
	[KEY_F9]	= SDLK_F9,
	[KEY_F10]	= KEY_F10,
	[KEY_F11]	= KEY_F11,
	[KEY_F12]	= KEY_F12,
	[KEY_F13]	= KEY_F13,	/* apostrophe, differs from Fn-A? */
	[KEY_F14]	= KEY_F14,	/* pipe/bar */
	[KEY_F15]	= KEY_F15,	/* dash */
	[KEY_F16]	= SDLK_HASH,	/* # (pound/hash) */
	[KEY_F17]	= SDLK_EXCLAIM,	/* ! */
//	[KEY_F18]	=		/* £ (pound) */
	[KEY_F19]	= SDLK_QUOTEDBL,/* " */
	[KEY_F20]	= SDLK_AT,	/* @ */
	[KEY_F21]	= SDLK_SEMICOLON,/* : */
//	[KEY_F22]	=
//	[KEY_F23]	=

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
//	[KEY_MENU]	=
	[KEY_RIGHTSHIFT]= SDLK_RSHIFT,
	[KEY_RIGHTCTRL]	= SDLK_RCTRL,
};

#define DNKEY(x) [SDLK_##x] = #x
static const char *sdl_keynames[SDLK_LAST] = {
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

	kc = in_get_key_code(-1, kname);
	if (kc < 0) {
		err("can't resolve key '%s'", kname);
		return;
	}

	if (sdlname == NULL || strncasecmp(sdlname, "SDLK_", 5) != 0)
		goto bad_sdlkey;

	for (i = 0; i < SDLK_LAST; i++) {
		if (sdl_keynames[i] == NULL)
			continue;
		if (strcasecmp(sdl_keynames[i], sdlname + 5) == 0)
			break;
	}

	if (i >= SDLK_LAST)
		goto bad_sdlkey;

	pmsdl_map[kc] = i;
	return;

bad_sdlkey:
	err("can't resolve SDL key '%s'", sdlname);
}

void omapsdl_input_init(void)
{
	in_init();
	in_probe();
}

int omapsdl_input_get_event(void *event_, int timeout)
{
	SDL_Event *event = event_;
	int key, is_down;

	while (1) {
		int kc;

		is_down = 0;
		kc = in_update_keycode(NULL, &is_down, timeout);
		if (kc < 0 || kc > KEY_MAX)
			return 0;

		key = pmsdl_map[kc];
		if (key != 0)
			break;
	}

	g_keystate[key] = is_down;

	if (event == NULL)
		return 1; // FIXME: ..but the event is lost

	memset(event, 0, sizeof(event->key));
	event->type = is_down ? SDL_KEYDOWN : SDL_KEYUP;
	// event->key.which =
	event->key.state = is_down ? SDL_PRESSED : SDL_RELEASED;
	event->key.keysym.sym = key;
	// event->key.keysym.mod

	return 1;
}

/* SDL */
#ifdef STANDALONE

DECLSPEC int SDLCALL
SDL_WaitEvent(SDL_Event *event)
{
	trace("%p", event);

	return omapsdl_input_get_event(event, -1);
}

DECLSPEC int SDLCALL
SDL_PollEvent(SDL_Event *event)
{
	trace("%p", event);

	return omapsdl_input_get_event(event, 0);
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
