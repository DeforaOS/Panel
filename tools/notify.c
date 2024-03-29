/* $Id$ */
/* Copyright (c) 2012-2022 Pierre Pronchery <khorben@defora.org> */
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
#define N_(string) string

/* constants */
#ifndef PROGNAME_PANEL_NOTIFY
# define PROGNAME_PANEL_NOTIFY	"panel-notify"
#endif
#define PROGNAME		PROGNAME_PANEL_NOTIFY
#include "helper.c"

#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef LIBDIR
# define LIBDIR			PREFIX "/lib"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR		DATADIR "/locale"
#endif


/* private */
/* prototypes */
static int _notify(int embed, GtkIconSize iconsize, int timeout,
		char * applets[]);
/* accessors */
static uint32_t _notify_get_xid(Panel * panel);
/* callbacks */
static gboolean _notify_on_timeout(gpointer data);

static int _usage(void);


/* functions */
/* notify */
static int _notify_embed(GtkIconSize iconsize, int timeout, char * applets[]);
static int _notify_panel(GtkIconSize iconsize, int timeout, char * applets[]);

static int _notify(int embed, GtkIconSize iconsize, int timeout,
		char * applets[])
{
	return (embed != 0) ? _notify_embed(iconsize, timeout, applets)
		: _notify_panel(iconsize, timeout, applets);
}

static int _notify_embed(GtkIconSize iconsize, int timeout, char * applets[])
{
	Panel panel;
	size_t i;
	uint32_t xid;

	_panel_init(&panel, PANEL_WINDOW_POSITION_EMBEDDED,
			PANEL_WINDOW_TYPE_NOTIFICATION, iconsize);
	for(i = 0; applets[i] != NULL; i++)
		if(_panel_append(&panel, PANEL_POSITION_TOP, applets[i]) != 0)
			error_print(PROGNAME_PANEL_NOTIFY);
	if((xid = _notify_get_xid(&panel)) == 0)
		/* XXX report error */
		return -1;
	desktop_message_send(PANEL_CLIENT_MESSAGE, PANEL_MESSAGE_EMBED, xid, 0);
	if(timeout > 0)
		g_timeout_add(timeout * 1000, _notify_on_timeout, NULL);
	gtk_main();
	return 0;
}

static int _notify_panel(GtkIconSize iconsize, int timeout, char * applets[])
{
	Panel panel;
	size_t i;

	_panel_init(&panel, PANEL_WINDOW_POSITION_CENTER,
			PANEL_WINDOW_TYPE_NOTIFICATION, iconsize);
	_panel_set_title(&panel, _("Notification"));
	for(i = 0; applets[i] != NULL; i++)
		if(_panel_append(&panel, PANEL_POSITION_TOP, applets[i]) != 0)
			error_print(PROGNAME_PANEL_NOTIFY);
	_panel_show(&panel, TRUE);
	if(timeout > 0)
		panel.timeout = g_timeout_add(timeout * 1000,
				_notify_on_timeout, &panel);
	gtk_main();
	_panel_destroy(&panel);
	return 0;
}


/* accessors */
/* notify_get_xid */
static uint32_t _notify_get_xid(Panel * panel)
{
	return panel_window_get_xid(panel->windows[PANEL_POSITION_TOP]);
}


/* callbacks */
/* notify_on_timeout */
static gboolean _notify_on_timeout(gpointer data)
{
	Panel * panel = data;

	if(panel != NULL)
		panel->timeout = 0;
	gtk_main_quit();
	return FALSE;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-e][-L|-S|-X|-x][-t timeout] applet...\n"
"       %s -l\n"
"  -e	Embed the notification in the panel\n"
"  -L	Use icons the size of a large toolbar\n"
"  -S	Use icons the size of a small toolbar\n"
"  -X	Use huge icons\n"
"  -x	Use icons the size of menus\n"
"  -t	Time to wait before disappearing (0: unlimited)\n"
"  -l	Lists the plug-ins available\n"), PROGNAME_PANEL_NOTIFY,
			PROGNAME_PANEL_NOTIFY);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	GtkIconSize iconsize;
	GtkIconSize huge;
	int embed = 0;
	int timeout = 3;
	int o;
	char * p;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	if((huge = gtk_icon_size_from_name("panel-huge"))
			== GTK_ICON_SIZE_INVALID)
		huge = gtk_icon_size_register("panel-huge", 64, 64);
	iconsize = huge;
	while((o = getopt(argc, argv, "eLlSt:Xx")) != -1)
		switch(o)
		{
			case 'e':
				embed = 1;
				break;
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
	if(_notify(embed, iconsize, timeout, &argv[optind]) != 0)
		return 2;
	return 0;
}
