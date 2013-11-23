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
#include <errno.h>
#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>
#include <Desktop/Browser.h>
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
struct _PanelApplet
{
	Plugin * plugin;
	PanelAppletDefinition * pad;
	PanelApplet * pa;
	GtkWidget * widget;
};

typedef struct _PanelWindow
{
	int height;

	/* applets */
	PanelAppletHelper * helper;
	PanelApplet * applets;
	size_t applets_cnt;

	/* widgets */
	GtkWidget * window;
	GtkWidget * box;
} PanelWindow;

struct _Panel
{
	Config * config;
	PanelAppletHelper helper;

	/* FIXME should not be needed */
	GtkWidget * window;
	PanelWindow top;

	guint timeout;
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
/* Panel */
static int _panel_init(Panel * panel, PanelAppletType type,
		GtkIconSize iconsize);
static void _panel_destroy(Panel * panel);

static void _panel_set_title(Panel * panel, char const * title);

#define HELPER_POSITION_MENU_WIDGET
#include "../src/helper.c"
static int _panel_append(Panel * panel, PanelPosition position,
		char const * applet);

/* PanelWindow */
static void _panel_window_init(PanelWindow * panel,
		PanelAppletHelper * helper);
static void _panel_window_destroy(PanelWindow * panel);

static int _panel_window_append(PanelWindow * window, char const * applet);

static int _applet_list(void);
static char * _config_get_filename(void);

static int _error(char const * message, int ret);

/* callbacks */
static gboolean _panel_window_on_closex(void);


/* helper */
/* essential */
static void _helper_init(PanelAppletHelper * helper, Panel * panel,
		PanelAppletType type, GtkIconSize iconsize);


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


/* private */
/* functions */
/* Panel */
/* panel_init */
static int _panel_init(Panel * panel, PanelAppletType type,
		GtkIconSize iconsize)
{
	char * filename;
	GdkScreen * screen;
	GdkWindow * root;
	GdkRectangle rect;

	if((panel->config = config_new()) == NULL)
		return -1;
	if((filename = _config_get_filename()) != NULL
			&& config_load(panel->config, filename) != 0)
		error_print(PROGNAME);
	free(filename);
	_helper_init(&panel->helper, panel, type, iconsize);
	_panel_window_init(&panel->top, &panel->helper);
	panel->window = panel->top.window;
	panel->timeout = 0;
	panel->source = 0;
	panel->ab_window = NULL;
	panel->lo_window = NULL;
	panel->sh_window = NULL;
	/* root window */
	screen = gdk_screen_get_default();
	root = gdk_screen_get_root_window(screen);
	gdk_screen_get_monitor_geometry(screen, 0, &rect);
	panel->root_height = rect.height;
	panel->root_width = rect.width;
	return 0;
}


/* panel_destroy */
static void _panel_destroy(Panel * panel)
{
	if(panel->timeout != 0)
		g_source_remove(panel->timeout);
	if(panel->source != 0)
		g_source_remove(panel->source);
	_panel_window_destroy(&panel->top);
	if(panel->ab_window != NULL)
		gtk_widget_destroy(panel->ab_window);
	if(panel->lo_window != NULL)
		gtk_widget_destroy(panel->lo_window);
	if(panel->sh_window != NULL)
		gtk_widget_destroy(panel->sh_window);
}


/* panel_set_title */
static void _panel_set_title(Panel * panel, char const * title)
{
	gtk_window_set_title(GTK_WINDOW(panel->top.window), title);
}


/* panel_append */
static int _panel_append(Panel * panel, PanelPosition position,
		char const * applet)
{
	if(position == PANEL_POSITION_TOP)
		return _panel_window_append(&panel->top, applet);
	return -error_set_code(1, "%s", "Invalid panel position");
}


/* PanelWindow */
/* panel_window_init */
static void _panel_window_init(PanelWindow * panel, PanelAppletHelper * helper)
{
	GdkRectangle rect;

	if(gtk_icon_size_lookup(helper->icon_size, &rect.width, &rect.height)
			== TRUE)
		panel->height = rect.height + 8;
	else
		panel->height = 72;
	panel->helper = helper;
	panel->applets = NULL;
	panel->applets_cnt = 0;
	panel->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(panel->window, "delete-event", G_CALLBACK(
				_panel_window_on_closex), NULL);
	panel->box = gtk_hbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(panel->window), panel->box);
}


/* panel_window_destroy */
static void _panel_window_destroy(PanelWindow * panel)
{
	size_t i;
	PanelApplet * pa;

	for(i = 0; i < panel->applets_cnt; i++)
	{
		pa = &panel->applets[i];
		gtk_widget_destroy(pa->widget);
		pa->pad->destroy(pa->pa);
		plugin_delete(pa->plugin);
	}
	free(panel->applets);
	gtk_widget_destroy(panel->window);
}


/* panel_window_append */
static int _panel_window_append(PanelWindow * window, char const * applet)
{
	PanelAppletHelper * helper = window->helper;
	PanelApplet * pa;

	if((pa = realloc(window->applets, sizeof(*pa)
					* (window->applets_cnt + 1))) == NULL)
		return -error_set_code(1, "%s", strerror(errno));
	window->applets = pa;
	pa = &window->applets[window->applets_cnt];
	if((pa->plugin = plugin_new(LIBDIR, PACKAGE, "applets", applet))
			== NULL)
		return -1;
	pa->widget = NULL;
	if((pa->pad = plugin_lookup(pa->plugin, "applet")) == NULL
			|| (pa->pa = pa->pad->init(helper, &pa->widget)) == NULL
			|| pa->widget == NULL)
	{
		if(pa->pa != NULL)
			pa->pad->destroy(pa->pa);
		plugin_delete(pa->plugin);
		return -1;
	}
	gtk_box_pack_start(GTK_BOX(window->box), pa->widget, pa->pad->expand,
			pa->pad->fill, 0);
	window->applets_cnt++;
	return 0;
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


/* useful */


/* callbacks */
/* panel_window_on_closex */
static gboolean _panel_window_on_closex(void)
{
	gtk_main_quit();
	return TRUE;
}
