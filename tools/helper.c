/* $Id$ */
/* Copyright (c) 2012-2013 Pierre Pronchery <khorben@defora.org> */
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
#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>
#include <Desktop/Browser.h>
#include "../config.h"

/* constants */
#ifndef PROGNAME
# define PROGNAME	PACKAGE
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef LIBDIR
# define LIBDIR		PREFIX "/lib"
#endif


/* private */
/* types */
typedef struct _PanelWindow
{
	int height;
} PanelWindow;

struct _Panel
{
	Config * config;

	PanelWindow * top;

	GtkWidget * window;
	gint timeout;
	gint root_width;		/* width of the root window	*/
	gint root_height;		/* height of the root window	*/
	guint source;

	/* dialogs */
	GtkWidget * ab_window;
	GtkWidget * lo_window;
	GtkWidget * sh_window;
};


static char const * _authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};


/* public */
/* prototypes */
int panel_window_get_height(PanelWindow * panel);


/* private */
/* prototypes */
static int _applet_list(void);
static char * _config_get_filename(void);

static int _error(char const * message, int ret);


/* helper */
/* essential */
static void _helper_init(PanelAppletHelper * helper, Panel * panel,
		PanelAppletType type, GtkIconSize iconsize);

/* useful */
#define HELPER_POSITION_MENU_WIDGET
#include "../src/helper.c"


/* public */
/* functions */
/* panel_error */
int panel_error(Panel * panel, char const * message, int ret)
{
	fputs(PROGNAME ": ", stderr);
	perror(message);
	return ret;
}


/* panel_show_preferences */
void panel_show_preferences(Panel * panel, gboolean show)
{
	/* XXX just a stub */
}


/* panel_window_get_height */
int panel_window_get_height(PanelWindow * panel)
{
	return panel->height;
}


/* functions */
/* applet_list */
static int _applet_list(void)
{
	char const path[] = LIBDIR "/Panel/applets";
	DIR * dir;
	struct dirent * de;
	size_t len;
	char const * sep = "";
#ifdef __APPLE__
	char const ext[] = ".dylib";
#else
	char const ext[] = ".so";
#endif

	puts("Applets available:");
	if((dir = opendir(path)) == NULL)
		return _error(path, 1);
	while((de = readdir(dir)) != NULL)
	{
		len = strlen(de->d_name);
		if(len < sizeof(ext) || strcmp(&de->d_name[
					len - sizeof(ext) + 1], ext) != 0)
			continue;
		de->d_name[len - 3] = '\0';
		printf("%s%s", sep, de->d_name);
		sep = ", ";
	}
	putchar('\n');
	closedir(dir);
	return 0;
}


/* config_get_filename */
static char * _config_get_filename(void)
{
	char const * homedir;
	size_t len;
	char * filename;

	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(PANEL_CONFIG_FILE);
	if((filename = malloc(len)) == NULL)
		return NULL;
	snprintf(filename, len, "%s/%s", homedir, PANEL_CONFIG_FILE);
	return filename;
}


/* error */
static int _error(char const * message, int ret)
{
	return _panel_helper_error(NULL, message, ret);
}


/* helpers */
/* essential */
/* helper_init */
static void _helper_init(PanelAppletHelper * helper, Panel * panel,
		PanelAppletType type, GtkIconSize iconsize)
{
	char const * p;

	memset(helper, 0, sizeof(*helper));
	helper->panel = panel;
	helper->type = type;
	helper->icon_size = iconsize;
	helper->config_get = _panel_helper_config_get;
	helper->config_set = _panel_helper_config_set;
	helper->error = _panel_helper_error;
	helper->about_dialog = _panel_helper_about_dialog;
	helper->lock = _panel_helper_lock;
#ifndef EMBEDDED
	if((p = config_get(panel->config, NULL, "logout")) == NULL
			|| strtol(p, NULL, 0) != 0)
#else
	if((p = config_get(panel->config, NULL, "logout")) != NULL
			&& strtol(p, NULL, 0) != 0)
#endif
		helper->logout_dialog = _panel_helper_logout_dialog;
	else
		helper->logout_dialog = NULL;
	helper->position_menu = _panel_helper_position_menu_widget;
	helper->preferences_dialog = _panel_helper_preferences_dialog;
	helper->rotate_screen = _panel_helper_rotate_screen;
	helper->shutdown_dialog = _panel_helper_shutdown_dialog;
	helper->suspend = _panel_helper_suspend;
}
