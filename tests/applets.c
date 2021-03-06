/* $Id$ */
/* Copyright (c) 2013-2015 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Panel */
/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */



#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "Panel/applet.h"

#define PROGNAME	"applets"


/* private */
/* prototypes */
static int _applets(void);

static int _dlerror(char const * message, int ret);
static int _error(char const * message, char const * error, int ret);
static int _perror(char const * message, int ret);


/* functions */
/* applets */
static int _applets(void)
{
	int ret = 0;
	const char path[] = "../src/applets";
#ifdef __APPLE__
	const char ext[] = ".dylib";
#else
	const char ext[] = ".so";
#endif
	DIR * dir;
	struct dirent * de;
	size_t len;
	char * s;
	void * p;
	PanelAppletDefinition * pad;

	if((dir = opendir(path)) == NULL)
		return -_perror(path, 1);
	while((de = readdir(dir)) != NULL)
	{
		if((len = strlen(de->d_name)) < sizeof(ext))
			continue;
		if(strcmp(&de->d_name[len - sizeof(ext) + 1], ext) != 0)
			continue;
		if((s = malloc(sizeof(path) + len + 1)) == NULL)
		{
			ret += _perror(de->d_name, 1);
			continue;
		}
		snprintf(s, sizeof(path) + len + 1, "%s/%s", path, de->d_name);
		if((p = dlopen(s, RTLD_LAZY)) == NULL)
		{
			ret += _dlerror(s, 1);
			free(s);
			continue;
		}
		if((pad = dlsym(p, "applet")) == NULL)
			ret += _dlerror(s, 1);
		else if(pad->icon == NULL)
			ret += _error(s, "No icon defined", 1);
		else
			printf("\n%s: %s (%s)\n%s\n", de->d_name, pad->name,
					pad->icon, (pad->description != NULL)
					? pad->description
					: "(no description)");
		free(s);
		dlclose(p);
	}
	closedir(dir);
	return ret;
}


/* dlerror */
static int _dlerror(char const * message, int ret)
{
	fputs(PROGNAME ": ", stderr);
	fprintf(stderr, "%s: %s\n", message, dlerror());
	return ret;
}


/* error */
static int _error(char const * message, char const * error, int ret)
{
	fputs(PROGNAME ": ", stderr);
	fprintf(stderr, "%s: %s\n", message, error);
	return ret;
}


/* perror */
static int _perror(char const * message, int ret)
{
	fputs(PROGNAME ": ", stderr);
	perror(message);
	return ret;
}


/* public */
/* functions */
/* main */
int main(void)
{
	return (_applets() == 0) ? 0 : 2;
}
