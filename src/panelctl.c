/* $Id$ */
/* Copyright (c) 2011-2015 Pierre Pronchery <khorben@defora.org> */
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



#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <Desktop.h>
#include "panel.h"
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME_PANELCTL
# define PROGNAME_PANELCTL	"panelctl"
#endif
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR		DATADIR "/locale"
#endif


/* panelctl */
/* private */
/* prototypes */
static int _panelctl(PanelMessageShow what, gboolean show);

static int _error(char const * message, int ret);
static int _usage(void);


/* functions */
static int _panelctl(PanelMessageShow what, gboolean show)
{
	desktop_message_send(PANEL_CLIENT_MESSAGE, PANEL_MESSAGE_SHOW, what,
			show);
	return 0;
}


/* error */
static int _error(char const * message, int ret)
{
	fputs(PROGNAME_PANELCTL ": ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-B|-L|-R|-S|-T|-b|-l|-r|-t]\n"
"  -B	Show the bottom panel\n"
"  -L	Show the left panel\n"
"  -R	Show the right panel\n"
"  -S	Display or change settings\n"
"  -T	Show the top panel\n"
"  -b	Hide the bottom panel\n"
"  -l	Hide the left panel\n"
"  -r	Hide the right panel\n"
"  -t	Hide the top panel\n"), PROGNAME_PANELCTL);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	unsigned int what = 0;
	gboolean show = TRUE;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "BLRSTblrt")) != -1)
		switch(o)
		{
			case 'B':
				what = show
					? what | PANEL_MESSAGE_SHOW_PANEL_BOTTOM
					: PANEL_MESSAGE_SHOW_PANEL_BOTTOM;
				show = TRUE;
				break;
			case 'L':
				what = show
					? what | PANEL_MESSAGE_SHOW_PANEL_LEFT
					: PANEL_MESSAGE_SHOW_PANEL_LEFT;
				show = TRUE;
				break;
			case 'R':
				what = show
					? what | PANEL_MESSAGE_SHOW_PANEL_RIGHT
					: PANEL_MESSAGE_SHOW_PANEL_RIGHT;
				show = TRUE;
				break;
			case 'S':
				what = show
					? what | PANEL_MESSAGE_SHOW_SETTINGS
					: PANEL_MESSAGE_SHOW_SETTINGS;
				show = TRUE;
				break;
			case 'T':
				what = show
					? what | PANEL_MESSAGE_SHOW_PANEL_TOP
					: PANEL_MESSAGE_SHOW_PANEL_TOP;
				show = TRUE;
				break;
			case 'b':
				what = show ? PANEL_MESSAGE_SHOW_PANEL_BOTTOM
					: what | PANEL_MESSAGE_SHOW_PANEL_BOTTOM;
				show = FALSE;
				break;
			case 'l':
				what = show ? PANEL_MESSAGE_SHOW_PANEL_LEFT
					: what | PANEL_MESSAGE_SHOW_PANEL_LEFT;
				show = FALSE;
				break;
			case 'r':
				what = show ? PANEL_MESSAGE_SHOW_PANEL_RIGHT
					: what | PANEL_MESSAGE_SHOW_PANEL_RIGHT;
				show = FALSE;
				break;
			case 't':
				what = show ? PANEL_MESSAGE_SHOW_PANEL_TOP
					: what | PANEL_MESSAGE_SHOW_PANEL_TOP;
				show = FALSE;
				break;
			default:
				return _usage();
		}
	if(argc != optind || what == 0)
		return _usage();
	return (_panelctl(what, show) == 0) ? 0 : 2;
}
