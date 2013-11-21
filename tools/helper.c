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
struct _PanelApplet
{
	Plugin * plugin;
	PanelAppletDefinition * pad;
	PanelApplet * pa;
};

typedef struct _PanelWindow
{
	int height;

	/* applets */
	PanelApplet * applets;
	size_t applets_cnt;

	/* widgets */
	GtkWidget * window;
	GtkWidget * box;
} PanelWindow;

struct _Panel
{
	Config * config;

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
int panel_init(Panel * panel, GtkIconSize iconsize);
void panel_destroy(Panel * panel);

void panel_window_init(PanelWindow * panel, GtkIconSize iconsize);
void panel_window_destroy(PanelWindow * panel);
int panel_window_get_height(PanelWindow * panel);


/* private */
/* prototypes */
static int _applet_list(void);
static char * _config_get_filename(void);

static int _error(char const * message, int ret);

/* callbacks */
static gboolean _panel_window_on_closex(void);


/* helper */
/* essential */
static void _helper_init(PanelAppletHelper * helper, Panel * panel,
		PanelAppletType type, GtkIconSize iconsize);

/* useful */
#define HELPER_POSITION_MENU_WIDGET
#include "../src/helper.c"
static int _helper_append(PanelAppletHelper * helper, PanelWindow * window,
		char const * applet);


/* public */
/* functions */
/* panel_init */
int panel_init(Panel * panel, GtkIconSize iconsize)
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
	panel_window_init(&panel->top, iconsize);
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
void panel_destroy(Panel * panel)
{
	if(panel->timeout != 0)
		g_source_remove(panel->timeout);
	if(panel->source != 0)
		g_source_remove(panel->source);
	panel_window_destroy(&panel->top);
	if(panel->ab_window != NULL)
		gtk_widget_destroy(panel->ab_window);
	if(panel->lo_window != NULL)
		gtk_widget_destroy(panel->lo_window);
	if(panel->sh_window != NULL)
		gtk_widget_destroy(panel->sh_window);
}


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


/* panel_window_init */
void panel_window_init(PanelWindow * panel, GtkIconSize iconsize)
{
	GdkRectangle rect;

	if(gtk_icon_size_lookup(iconsize, &rect.width, &rect.height) == TRUE)
		panel->height = rect.height + 8;
	else
		panel->height = 72;
	panel->applets = NULL;
	panel->applets_cnt = 0;
	panel->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(panel->window, "delete-event", G_CALLBACK(
				_panel_window_on_closex), NULL);
	panel->box = gtk_hbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(panel->window), panel->box);
}


/* panel_window_destroy */
void panel_window_destroy(PanelWindow * panel)
{
	size_t i;
	PanelApplet * pa;

	for(i = 0; i < panel->applets_cnt; i++)
	{
		pa = &panel->applets[i];
		pa->pad->destroy(pa->pa);
		plugin_delete(pa->plugin);
	}
	free(panel->applets);
	gtk_widget_destroy(panel->window);
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
/* helper_append */
static int _helper_append(PanelAppletHelper * helper, PanelWindow * window,
		char const * applet)
{
	PanelApplet * pa;
	GtkWidget * widget;

	if((pa = realloc(window->applets, sizeof(*pa)
					* (window->applets_cnt + 1))) == NULL)
		return error_print(PROGNAME);
	window->applets = pa;
	pa = &window->applets[window->applets_cnt];
	if((pa->plugin = plugin_new(LIBDIR, "Panel", "applets", applet))
			== NULL)
		return error_print(PROGNAME);
	if((pa->pad = plugin_lookup(pa->plugin, "applet")) == NULL)
	{
		plugin_delete(pa->plugin);
		return error_print(PROGNAME);
	}
	widget = NULL;
	if((pa->pa = pa->pad->init(helper, &widget)) != NULL && widget != NULL)
		gtk_box_pack_start(GTK_BOX(window->box), widget,
				pa->pad->expand, pa->pad->fill, 0);
	window->applets_cnt++;
	return 0;
}


/* callbacks */
/* panel_window_on_closex */
static gboolean _panel_window_on_closex(void)
{
	gtk_main_quit();
	return TRUE;
}
