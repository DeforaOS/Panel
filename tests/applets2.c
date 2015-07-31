/* $Id$ */
/* Copyright (c) 2013 Pierre Pronchery <khorben@defora.org> */
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
#include "../src/window.h"
#include "Panel/applet.h"


/* private */
/* prototypes */
static gboolean _applets2(gpointer data);

static int _dlerror(char const * message, int ret);
static int _error(char const * message, char const * error, int ret);
static int _perror(char const * message, int ret);


/* functions */
/* applets2 */
/* callbacks */
static char const * _applets2_helper_config_get(Panel * panel,
		char const * section, char const * variable);
static int _applets2_helper_error(Panel * panel, char const * message, int ret);

static gboolean _applets2(gpointer data)
{
	int * ret = data;
	char const * path = "../src/applets";
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
	GdkRectangle root;
	PanelAppletHelper helper;
	PanelAppletDefinition * pad;
	PanelApplet * pa;
	GtkWidget * widget;

	if((p = getenv("OBJDIR")) != NULL)
		path = p;
	if((dir = opendir(path)) == NULL)
	{
		*ret = _perror(path, 1);
		gtk_main_quit();
		return FALSE;
	}
	*ret = 0;
	memset(&helper, 0, sizeof(helper));
	root.x = 0;
	root.y = 0;
	root.height = 768;
	root.width = 1024;
	if((helper.window = panel_window_new(&helper, PANEL_WINDOW_TYPE_NORMAL,
					PANEL_WINDOW_POSITION_TOP,
					GTK_ICON_SIZE_SMALL_TOOLBAR, &root))
			== NULL)
	{
		closedir(dir);
		return FALSE;
	}
	helper.config_get = _applets2_helper_config_get;
	helper.error = _applets2_helper_error;
	while((de = readdir(dir)) != NULL)
	{
		if((len = strlen(de->d_name)) < sizeof(ext))
			continue;
		if(strcmp(&de->d_name[len - sizeof(ext) + 1], ext) != 0)
			continue;
		if((s = malloc(strlen(path) + 1 + len + 1)) == NULL)
		{
			*ret += _perror(de->d_name, 1);
			continue;
		}
		snprintf(s, strlen(path) + 1 + len + 1, "%s/%s", path,
				de->d_name);
		if((p = dlopen(s, RTLD_LAZY)) == NULL)
		{
			*ret += _dlerror(s, 1);
			free(s);
			continue;
		}
		widget = NULL;
		if((pad = dlsym(p, "applet")) == NULL)
			*ret += _dlerror(s, 1);
		else if(pad->init == NULL)
			*ret += _error(s, "Missing initializer", 1);
		else if(pad->destroy == NULL)
			*ret += _error(s, "Missing destructor", 1);
		else if((pa = pad->init(&helper, &widget)) == NULL)
			_error(s, "Could not initialize", 0);
		else if(widget == NULL)
		{
			_error(s, "No widget returned", 0);
			pad->destroy(pa);
		}
		else
			pad->destroy(pa);
		free(s);
		dlclose(p);
	}
	closedir(dir);
	gtk_main_quit();
	return FALSE;
}

static char const * _applets2_helper_config_get(Panel * panel,
		char const * section, char const * variable)
{
	printf("%s: config_get(\"%s\", \"%s\")\n", "applets2", section,
			variable);
	return NULL;
}

static int _applets2_helper_error(Panel * panel, char const * message, int ret)
{
	fprintf(stderr, "%s: %s\n", "applets2", message);
	return ret;
}


/* dlerror */
static int _dlerror(char const * message, int ret)
{
	fputs("applets2: ", stderr);
	fprintf(stderr, "%s: %s\n", message, dlerror());
	return ret;
}


/* error */
static int _error(char const * message, char const * error, int ret)
{
	fputs("applets2: ", stderr);
	fprintf(stderr, "%s: %s\n", message, error);
	return ret;
}


/* perror */
static int _perror(char const * message, int ret)
{
	fputs("applets2: ", stderr);
	perror(message);
	return ret;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int ret = 0;
	guint source;

	if(getenv("DISPLAY") == NULL)
		/* XXX ignore this test */
		return 0;
	gtk_init(&argc, &argv);
	source = g_idle_add(_applets2, &ret);
	gtk_main();
	return (ret == 0) ? 0 : 2;
}
