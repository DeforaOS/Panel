/* $Id$ */
/* Copyright (c) 2012-2018 Pierre Pronchery <khorben@defora.org> */
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
#include <errno.h>
#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>
#include "../src/window.h"
#include "../config.h"

/* constants */
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef LIBDIR
# define LIBDIR		PREFIX "/lib"
#endif


/* private */
/* types */
struct _Panel
{
	Config * config;

	PanelPrefs prefs;

	PanelAppletHelper helper[PANEL_POSITION_COUNT];
	PanelWindow * windows[PANEL_POSITION_COUNT];

	GdkScreen * screen;
	GdkWindow * root;
	gint root_width;		/* width of the root window	*/
	gint root_height;		/* height of the root window	*/
	guint source;
	guint timeout;

	/* dialogs */
	GtkWidget * ab_window;
	GtkWidget * lo_window;
	GtkWidget * sh_window;
	GtkWidget * su_window;
};


/* constants */
static char const * _authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};


/* prototypes */
/* Panel */
static int _panel_init(Panel * panel, PanelWindowPosition position,
		PanelWindowType type, GtkIconSize iconsize);
static void _panel_destroy(Panel * panel);

/* accessors */
static uint32_t _panel_get_xid(Panel * panel);
static void _panel_set_title(Panel * panel, char const * title);

/* useful */
#define HELPER_POSITION_MENU_WIDGET
#include "../src/helper.c"
static int _panel_append(Panel * panel, PanelPosition position,
		char const * applet);
static void _panel_show(Panel * panel, gboolean show);

static int _applet_list(void);
static char * _config_get_filename(void);

static int _error(char const * message, int ret);

/* helper */
/* essential */
static void _helper_init(PanelAppletHelper * helper, Panel * panel,
		PanelWindowType type, GtkIconSize iconsize);


/* public */
/* functions */
/* accessors */
/* panel_get_config */
char const * panel_get_config(Panel * panel, char const * section,
		char const * variable)
{
	return config_get(panel->config, section, variable);
}


/* useful */
/* panel_error */
int panel_error(Panel * panel, char const * message, int ret)
{
	(void) panel;

	fprintf(stderr, "%s: %s\n", PROGNAME, (message != NULL) ? message
			: error_get(NULL));
	return ret;
}


/* panel_show_preferences */
void panel_show_preferences(Panel * panel, gboolean show)
{
	(void) panel;
	(void) show;

	/* XXX just a stub */
}


/* private */
/* functions */
/* Panel */
/* panel_init */
static int _panel_init(Panel * panel, PanelWindowPosition position,
		PanelWindowType type, GtkIconSize iconsize)
{
	const PanelPosition top = PANEL_POSITION_TOP;
	char * filename;
	GdkRectangle rect;
	size_t i;

	if((panel->config = config_new()) == NULL)
		return -1;
	if((filename = _config_get_filename()) != NULL
			&& config_load(panel->config, filename) != 0)
		error_print(PROGNAME);
	free(filename);
	panel->prefs.iconsize = NULL;
	panel->prefs.monitor = -1;
	/* root window */
	panel->screen = gdk_screen_get_default();
	panel->root = gdk_screen_get_root_window(panel->screen);
	gdk_screen_get_monitor_geometry(panel->screen, 0, &rect);
	panel->root_height = rect.height;
	panel->root_width = rect.width;
	/* panel window */
	_helper_init(&panel->helper[top], panel, type, iconsize);
	panel->windows[top] = panel_window_new(&panel->helper[top],
			PANEL_WINDOW_TYPE_NORMAL, position, iconsize, &rect);
	panel->helper[top].window = panel->windows[top];
	for(i = 0; i < sizeof(panel->windows) / sizeof(*panel->windows); i++)
		if(i != top)
			panel->windows[i] = NULL;
	panel->source = 0;
	panel->timeout = 0;
	panel->ab_window = NULL;
	panel->lo_window = NULL;
	panel->sh_window = NULL;
	panel->su_window = NULL;
	return 0;
}


/* panel_destroy */
static void _panel_destroy(Panel * panel)
{
	size_t i;

	if(panel->timeout != 0)
		g_source_remove(panel->timeout);
	if(panel->source != 0)
		g_source_remove(panel->source);
	for(i = 0; i < sizeof(panel->windows) / sizeof(*panel->windows); i++)
		if(panel->windows[i] != NULL)
			panel_window_delete(panel->windows[i]);
	if(panel->ab_window != NULL)
		gtk_widget_destroy(panel->ab_window);
	if(panel->lo_window != NULL)
		gtk_widget_destroy(panel->lo_window);
	if(panel->sh_window != NULL)
		gtk_widget_destroy(panel->sh_window);
	if(panel->su_window != NULL)
		gtk_widget_destroy(panel->su_window);
}


/* accessors */
/* panel_get_xid */
static uint32_t _panel_get_xid(Panel * panel)
{
	return panel_window_get_xid(panel->windows[PANEL_POSITION_TOP]);
}


/* panel_set_title */
static void _panel_set_title(Panel * panel, char const * title)
{
	panel_window_set_title(panel->windows[PANEL_POSITION_TOP], title);
}


/* useful */
/* panel_append */
static int _panel_append(Panel * panel, PanelPosition position,
		char const * applet)
{
	if(position == PANEL_POSITION_TOP)
		return panel_window_append(panel->windows[PANEL_POSITION_TOP],
				applet);
	return -error_set_code(1, "%s", _("Invalid panel position"));
}


/* panel_show */
static void _panel_show(Panel * panel, gboolean show)
{
	panel_window_show(panel->windows[PANEL_POSITION_TOP], show);
}


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

	puts(_("Applets available:"));
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
		PanelWindowType type, GtkIconSize iconsize)
{
	char const * p;
	(void) panel;
	(void) iconsize;

	memset(helper, 0, sizeof(*helper));
	helper->panel = panel;
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
