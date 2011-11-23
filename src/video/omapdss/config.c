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

#include "omapsdl.h"

static char *sskip(char *p)
{
	while (*p && isspace(*p))
		p++;
	return p;
}

static char *nsskip(char *p)
{
	while (*p && !isspace(*p))
		p++;
	return p;
}

static int check_token(char **p_, const char *token)
{
	char *p = *p_;
	int tlen = strlen(token);
	int ret = strncasecmp(p, token, tlen) == 0 && isspace(p[tlen]);
	if (ret)
		*p_ = sskip(p + tlen + 1);

	return ret;
}

static int check_token_eq(char **p_, const char *token)
{
	char *p = *p_;
	int ret = check_token(&p, token);
	ret = ret && *p == '=';
	if (ret)
		*p_ = sskip(p + 1);

	return ret;
}

void omapsdl_config(struct SDL_PrivateVideoData *pdata)
{
	char buff[256];
	FILE *f;

	f = fopen("omapsdl.cfg", "r");
	if (f == NULL)
		return;

	while (!feof(f)) {
		char *p, *line = fgets(buff, sizeof(buff), f);
		if (line == NULL)
			break;
		p = line = sskip(line);
		if (*p == 0 || *p == '#')
			continue;

		if (check_token(&p, "bind")) {
			char *key, *key_end, *sdlkey, *sdlkey_end;
			key = p;
			key_end = nsskip(key);
			p = sskip(key_end);
			if (*p != '=')
				goto bad;
			sdlkey = sskip(p + 1);
			sdlkey_end = nsskip(sdlkey);
			p = sskip(sdlkey_end);
			if (*key == 0 || *sdlkey == 0 || *p != 0)
				goto bad;
			*key_end = *sdlkey_end = 0;

			omapsdl_input_bind(key, sdlkey);
			continue;
		}
		else if (check_token_eq(&p, "force_vsync")) {
			pdata->cfg_force_vsync = !!strtol(p, NULL, 0);
			continue;
		}
		else if (check_token_eq(&p, "force_doublebuf")) {
			pdata->cfg_force_doublebuf = !!strtol(p, NULL, 0);
			continue;
		}
		else if (check_token_eq(&p, "no_ts_translate")) {
			pdata->cfg_no_ts_translate = !!strtol(p, NULL, 0);
			continue;
		}

bad:
		err("config: failed to parse: %s", line);
	}
	fclose(f);
}

void omapsdl_config_from_env(struct SDL_PrivateVideoData *pdata)
{
	const char *tmp;

	tmp = getenv("SDL_OMAP_VSYNC");
	if (tmp != NULL)
		pdata->cfg_force_vsync = !!strtol(tmp, NULL, 0);
	tmp = getenv("SDL_OMAP_FORCE_DOUBLEBUF");
	if (tmp != NULL)
		pdata->cfg_force_doublebuf = !!strtol(tmp, NULL, 0);
	tmp = getenv("SDL_OMAP_NO_TS_TRANSLATE");
	if (tmp != NULL)
		pdata->cfg_no_ts_translate = !!strtol(tmp, NULL, 0);
	tmp = getenv("SDL_OMAP_BORDER_CUT");
	if (tmp != NULL) {
		int l, r, t, b;
		if (sscanf(tmp, "%d,%d,%d,%d", &l, &r, &t, &b) == 4
		    && l >= 0 && r >= 0 && t >= 0 && b >= 0) {
			pdata->border_l = l, pdata->border_r = r;
			pdata->border_t = t, pdata->border_b = b;
		}
		else
			err("border incorrectly specified, ignored");
	}
}

