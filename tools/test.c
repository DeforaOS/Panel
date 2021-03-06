/* $Id$ */
/* Copyright (c) 2011-2018 Pierre Pronchery <khorben@defora.org> */
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
#define N_(string) string

#ifndef PROGNAME_PANEL_TEST
# define PROGNAME_PANEL_TEST	"panel-test"
#endif
#define PROGNAME		PROGNAME_PANEL_TEST
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
static int _test(PanelWindowType type, PanelWindowPosition position,
		GtkIconSize iconsize, char * applets[]);

static int _usage(void);


/* functions */
/* test */
static int _test(PanelWindowType type, PanelWindowPosition position,
		GtkIconSize iconsize, char * applets[])
{
	Panel panel;
	size_t i;

	_panel_init(&panel, position, type, iconsize);
	_panel_set_title(&panel, "Applet tester");
	for(i = 0; applets[i] != NULL; i++)
		if(_panel_append(&panel, PANEL_POSITION_TOP, applets[i]) != 0)
			error_print(PROGNAME_PANEL_TEST);
	_panel_show(&panel, TRUE);
	gtk_main();
	_panel_destroy(&panel);
	return 0;
}


/* usage */
static int _usage(void)
{
	fputs("Usage: " PROGNAME_PANEL_TEST " [-C|-F|-M][-L|-S|-X|-x][-n] applet...\n"
"       " PROGNAME_PANEL_TEST " -l\n"
"  -l	Lists the plug-ins available\n", stderr);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	PanelWindowType type = PANEL_WINDOW_TYPE_NORMAL;
	PanelWindowPosition position = PANEL_WINDOW_POSITION_MANAGED;
	GtkIconSize iconsize = GTK_ICON_SIZE_LARGE_TOOLBAR;
	GtkIconSize huge;
	int o;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	if((huge = gtk_icon_size_from_name("panel-huge"))
			== GTK_ICON_SIZE_INVALID)
		huge = gtk_icon_size_register("panel-huge", 64, 64);
	while((o = getopt(argc, argv, "CFLlMnSXx")) != -1)
		switch(o)
		{
			case 'C':
				position = PANEL_WINDOW_POSITION_CENTER;
				break;
			case 'F':
				position = PANEL_WINDOW_POSITION_FLOATING;
				break;
			case 'L':
				iconsize = GTK_ICON_SIZE_LARGE_TOOLBAR;
				break;
			case 'l':
				return _applet_list();
			case 'M':
				position = PANEL_WINDOW_POSITION_MANAGED;
				break;
			case 'n':
				type = PANEL_WINDOW_TYPE_NOTIFICATION;
				break;
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
	_test(type, position, iconsize, &argv[optind]);
	return 0;
}
