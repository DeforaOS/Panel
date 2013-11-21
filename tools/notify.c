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



#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>
#include "../src/panel.h"
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME
# define PROGNAME	"panel-notify"
#endif
#include "helper.c"

#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef LIBDIR
# define LIBDIR		PREFIX "/lib"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* private */
/* prototypes */
static int _notify(GtkIconSize iconsize, int timeout, char * applets[]);
/* callbacks */
static gboolean _notify_on_timeout(gpointer data);

static int _usage(void);


/* functions */
/* notify */
static int _notify(GtkIconSize iconsize, int timeout, char * applets[])
{
	Panel panel;
	PanelWindow top;
	char * filename;
	size_t i;
	PanelAppletHelper helper;
	GdkScreen * screen;
	GdkWindow * root;
	GdkRectangle rect;

	if((panel.config = config_new()) == NULL)
		return error_print(PROGNAME);
	if(gtk_icon_size_lookup(iconsize, &rect.width, &rect.height) == TRUE)
		top.height = rect.height + 8;
	else
		top.height = 72;
	top.applets = NULL;
	top.applets_cnt = 0;
	top.box = gtk_hbox_new(FALSE, 4);
	panel.top = &top;
	if((filename = _config_get_filename()) != NULL
			&& config_load(panel.config, filename) != 0)
		error_print(PROGNAME);
	free(filename);
	panel.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_window_set_has_resize_grip(GTK_WINDOW(panel.window), FALSE);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(panel.window), 4);
	gtk_window_set_accept_focus(GTK_WINDOW(panel.window), FALSE);
	gtk_window_set_decorated(GTK_WINDOW(panel.window), FALSE);
	gtk_window_set_position(GTK_WINDOW(panel.window),
			GTK_WIN_POS_CENTER_ALWAYS);
	g_signal_connect(G_OBJECT(panel.window), "delete-event", G_CALLBACK(
				gtk_main_quit), NULL);
	gtk_window_set_title(GTK_WINDOW(panel.window), _("Notification"));
	_helper_init(&helper, &panel, PANEL_APPLET_TYPE_NOTIFICATION, iconsize);
	for(i = 0; applets[i] != NULL; i++)
		_helper_append(&helper, &top, applets[i]);
	gtk_container_add(GTK_CONTAINER(panel.window), top.box);
	gtk_widget_show_all(panel.window);
	panel.timeout = 0;
	if(timeout > 0)
		panel.timeout = g_timeout_add(timeout * 1000,
				_notify_on_timeout, &panel);
	/* root window */
	screen = gdk_screen_get_default();
	root = gdk_screen_get_root_window(screen);
	gdk_screen_get_monitor_geometry(screen, 0, &rect);
	panel.root_height = rect.height;
	panel.root_width = rect.width;
	panel.source = 0;
	panel.ab_window = NULL;
	panel.lo_window = NULL;
	panel.sh_window = NULL;
	gtk_main();
	if(panel.timeout != 0)
		g_source_remove(panel.timeout);
	if(panel.source != 0)
		g_source_remove(panel.source);
	return 0;
}


/* callbacks */
/* notify_on_timeout */
static gboolean _notify_on_timeout(gpointer data)
{
	Panel * panel = data;

	panel->timeout = 0;
	gtk_main_quit();
	return FALSE;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-L|-S|-X|-x][-t timeout] applet...\n"
"       %s -l\n"
"  -L	Use icons the size of a large toolbar\n"
"  -S	Use icons the size of a small toolbar\n"
"  -X	Use huge icons\n"
"  -x	Use icons the size of menus\n"
"  -t	Time to wait before disappearing (0: unlimited)\n"
"  -l	Lists the plug-ins available\n"), PROGNAME, PROGNAME);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	GtkIconSize iconsize;
	GtkIconSize huge;
	int timeout = 3;
	int o;
	char * p;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	if((huge = gtk_icon_size_from_name("panel-huge"))
			== GTK_ICON_SIZE_INVALID)
		huge = gtk_icon_size_register("panel-huge", 64, 64);
	iconsize = huge;
	while((o = getopt(argc, argv, "LlSt:Xx")) != -1)
		switch(o)
		{
			case 'L':
				iconsize = GTK_ICON_SIZE_LARGE_TOOLBAR;
				break;
			case 'l':
				return _applet_list();
			case 'S':
				iconsize = GTK_ICON_SIZE_SMALL_TOOLBAR;
				break;
			case 't':
				timeout = strtoul(optarg, &p, 0);
				if(optarg[0] == '\0' || *p != '\0'
						|| timeout < 0)
					return _usage();
				break;
			case 'X':
				iconsize = huge;
				break;
			case 'x':
				iconsize = GTK_ICON_SIZE_MENU;
				break;
			default:
				return _usage();
		}
	if(optind == argc)
		return _usage();
	_notify(iconsize, timeout, &argv[optind]);
	return 0;
}
