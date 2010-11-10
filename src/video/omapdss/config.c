/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
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

static int check_token(const char *p, const char *token)
{
	int tlen = strlen(token);
	return strncasecmp(p, token, tlen) == 0 && isspace(p[tlen]);
}

void omapsdl_config(void)
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
		if (*p == '#')
			continue;

		if (check_token(p, "bind")) {
			char *key, *key_end, *sdlkey, *sdlkey_end;
			key = sskip(p + 5);
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

bad:
		err("config: failed to parse: %s", line);
	}
	fclose(f);
}


