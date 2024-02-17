/* $Id$ */
/* Copyright (c) 2010-2023 Pierre Pronchery <khorben@defora.org> */
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



#include <string.h>
#include <libintl.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
# include <gdk/gdkx.h>
# include <X11/X.h>
#endif
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string


/* Desktop */
/* private */
/* types */
typedef struct _PanelApplet Desktop;


/* prototypes */
static Desktop * _desktop_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _desktop_destroy(Desktop * desktop);

/* callbacks */
static void _desktop_on_clicked(GtkWidget * widget);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Desktop switch"),
	"panel-applet-desktop",
	NULL,
	_desktop_init,
	_desktop_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* desktop_init */
static Desktop * _desktop_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	GtkWidget * ret;
	GtkWidget * image;

	ret = gtk_button_new();
	image = gtk_image_new_from_icon_name(applet.icon,
			panel_window_get_icon_size(helper->window));
	gtk_button_set_image(GTK_BUTTON(ret), image);
	gtk_button_set_relief(GTK_BUTTON(ret), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(ret, _("Show desktop"));
#endif
	g_signal_connect(ret, "clicked", G_CALLBACK(_desktop_on_clicked), NULL);
	gtk_widget_show_all(ret);
	*widget = ret;
	/* XXX ugly workaround */
	return (Desktop *)ret;
}


/* desktop_destroy */
static void _desktop_destroy(Desktop * desktop)
{
	/* XXX just as ugly */
	gtk_widget_destroy((GtkWidget *)desktop);
}


/* callbacks */
/* on_clicked */
static void _desktop_on_clicked(GtkWidget * widget)
{
#if defined(GDK_WINDOWING_X11)
	GdkScreen * screen;
	GdkDisplay * display;
	GdkWindow * root;
	XEvent xev;

	screen = gtk_widget_get_screen(widget);
	display = gtk_widget_get_display(widget);
	root = gdk_screen_get_root_window(screen);
	xev.xclient.type = ClientMessage;
	xev.xclient.window = GDK_WINDOW_XID(root);
	xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display(
			display, "_NET_SHOWING_DESKTOP");
	xev.xclient.format = 32;
	memset(&xev.xclient.data, 0, sizeof(xev.xclient.data));
	xev.xclient.data.l[0] = 1;
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XID(root),
			False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
	gdk_error_trap_pop();
#endif
}
