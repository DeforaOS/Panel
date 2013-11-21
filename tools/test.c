/* $Id$ */
/* Copyright (c) 2011-2013 Pierre Pronchery <khorben@defora.org> */
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
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>
#include "../src/panel.h"
#include "../config.h"
#define _(string) gettext(string)

#ifndef PROGNAME
# define PROGNAME	"panel-test"
#endif
#include "helper.c"

/* constants */
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
static int _test(GtkIconSize iconsize, char * applets[]);
static int _usage(void);


/* functions */
/* test */
static int _test(GtkIconSize iconsize, char * applets[])
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
		top.height = rect.height;
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
	g_signal_connect(G_OBJECT(panel.window), "delete-event", G_CALLBACK(
				gtk_main_quit), NULL);
	gtk_window_set_title(GTK_WINDOW(panel.window), _("Applet tester"));
	_helper_init(&helper, &panel, PANEL_APPLET_TYPE_NORMAL, iconsize);
	for(i = 0; applets[i] != NULL; i++)
		_helper_append(&helper, &top, applets[i]);
	gtk_container_add(GTK_CONTAINER(panel.window), top.box);
	gtk_widget_show_all(panel.window);
	panel.timeout = 0;
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
	if(panel.source != 0)
		g_source_remove(panel.source);
	return 0;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-L|-S|-X|-x] applet...\n"
"       %s -l\n"
"  -l	Lists the plug-ins available\n"), PROGNAME, PROGNAME);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	GtkIconSize iconsize = GTK_ICON_SIZE_LARGE_TOOLBAR;
	GtkIconSize huge;
	int o;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	if((huge = gtk_icon_size_from_name("panel-huge"))
			== GTK_ICON_SIZE_INVALID)
		huge = gtk_icon_size_register("panel-huge", 64, 64);
	while((o = getopt(argc, argv, "LlSXx")) != -1)
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
	_test(iconsize, &argv[optind]);
	return 0;
}
