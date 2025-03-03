/* $Id$ */
/* Copyright (c) 2012-2025 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <System/error.h>
#include <Desktop.h>
#if defined(GDK_WINDOWING_X11)
# if GTK_CHECK_VERSION(3, 0, 0)
#  include <gtk/gtkx.h>
# endif
#endif
#include "../include/Panel.h"
#include "../config.h"

#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME_PANEL_MESSAGE
# define PROGNAME_PANEL_MESSAGE	"panel-message"
#endif
#ifndef PREFIX
# define PREFIX 	        "/usr/local"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR		DATADIR "/locale"
#endif


/* private */
/* prototypes */
static int _message(unsigned int timeout, char const * stock,
		char const * title, char const * message);

/* callbacks */
static gboolean _message_on_timeout(gpointer data);

static int _error(char const * message, int ret);
static int _usage(void);


/* functions */
/* message */
static int _message(unsigned int timeout, char const * stock,
		char const * title, char const * message)
{
	PangoFontDescription * bold;
	GtkWidget * plug;
	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * image;
	GtkWidget * widget;
#if defined(GDK_WINDOWING_X11)
	uint32_t xid;
#endif

#if !defined(GDK_WINDOWING_X11)
	error_set_print(PROGNAME_PANEL_MESSAGE, 2, "%s",
			"X11 support not detected");
#endif
	bold = pango_font_description_new();
	pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
#if defined(GDK_WINDOWING_X11)
	plug = gtk_plug_new(0);
#else
	plug = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(plug), "Message");
#endif
	gtk_container_set_border_width(GTK_CONTAINER(plug), 4);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	/* icon */
#if !GTK_CHECK_VERSION(3, 10, 0)
	if(strncmp(stock, "stock_", 6) == 0)
		widget = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_DIALOG);
	else
#endif
		widget = gtk_image_new_from_icon_name(stock,
				GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	/* title */
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox = gtk_vbox_new(FALSE, 4);
#endif
	widget = gtk_label_new(title);
#if GTK_CHECK_VERSION(3, 14, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(widget, bold);
#else
	gtk_widget_modify_font(widget, bold);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);
	/* label */
	widget = gtk_label_new(message);
#if GTK_CHECK_VERSION(3, 14, 0)
	g_object_set(widget, "halign", GTK_ALIGN_START, NULL);
#else
	gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	/* button */
	widget = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(3, 10, 0)
	image = gtk_image_new_from_icon_name(
#else
	image = gtk_image_new_from_stock(
#endif
			GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(widget), image);
	g_signal_connect(widget, "clicked", G_CALLBACK(gtk_main_quit), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(plug), hbox);
	gtk_widget_show_all(plug);
#if defined(GDK_WINDOWING_X11)
	xid = gtk_plug_get_id(GTK_PLUG(plug));
	desktop_message_send(PANEL_CLIENT_MESSAGE, PANEL_MESSAGE_EMBED, xid, 0);
#endif
	if(timeout > 0)
		g_timeout_add(timeout * 1000, _message_on_timeout, NULL);
	pango_font_description_free(bold);
	gtk_main();
	return 0;
}


/* callbacks */
/* message_on_timeout */
static gboolean _message_on_timeout(gpointer data)
{
	(void) data;

	gtk_main_quit();
	return FALSE;
}


/* error */
static int _error(char const * message, int ret)
{
	fputs(PROGNAME_PANEL_MESSAGE ": ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-EIQW][-T title][-t timeout] message\n"
"       %s [-N name][-T title][-t timeout] message\n"), PROGNAME_PANEL_MESSAGE,
			PROGNAME_PANEL_MESSAGE);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	unsigned int timeout = 3;
	char const * stock = GTK_STOCK_DIALOG_INFO;
	char const * title = _("Information");
	int o;
	char * p;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "EIN:QT:Wt:")) != -1)
		switch(o)
		{
			case 'E':
				stock = GTK_STOCK_DIALOG_ERROR;
				title = _("Error");
				break;
			case 'I':
				stock = GTK_STOCK_DIALOG_INFO;
				title = _("Information");
				break;
			case 'N':
				stock = optarg;
				break;
			case 'Q':
				stock = GTK_STOCK_DIALOG_QUESTION;
				title = _("Question");
				break;
			case 'T':
				title = optarg;
				break;
			case 'W':
				stock = GTK_STOCK_DIALOG_WARNING;
				title = _("Warning");
				break;
			case 't':
				timeout = strtoul(optarg, &p, 10);
				if(optarg[0] == '\0' || *p != '\0')
					return _usage();
				break;
			default:
				return _usage();
		}
	if(argc - optind != 1)
		return _usage();
	return (_message(timeout, stock, title, argv[optind]) == 0) ? 0 : 2;
}
