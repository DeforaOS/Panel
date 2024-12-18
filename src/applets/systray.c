/* $Id$ */
/* Copyright (c) 2010-2024 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Pager Panel */
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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>
#if defined(GDK_WINDOWING_X11)
# if GTK_CHECK_VERSION(3, 0, 0)
#  include <gtk/gtkx.h>
# else
#  include <gdk/gdkx.h>
#  include <X11/Xatom.h>
# endif
#endif
#include <System.h>
#include "Panel/applet.h"

#define N_(string) string


/* Systray */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
#if defined(GDK_WINDOWING_X11)
	GtkWidget * hbox;
	GtkWidget * owner;
	gulong source;
#endif
} Systray;


/* constants */
#define SYSTEM_TRAY_REQUEST_DOCK	0
#define SYSTEM_TRAY_BEGIN_MESSAGE	1
#define SYSTEM_TRAY_CANCEL_MESSAGE	2


/* prototypes */
static Systray * _systray_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _systray_destroy(Systray * systray);

#if defined(GDK_WINDOWING_X11)
/* callbacks */
static GdkFilterReturn _systray_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data);
static void _systray_on_owner_destroy(gpointer data);
static void _systray_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data);
#endif


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("System tray"),
	"gnome-monitor",
	NULL,
	_systray_init,
	_systray_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* systray_init */
static Systray * _systray_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
#if defined(GDK_WINDOWING_X11)
	Systray * systray;
	gint height = 24;

	if((systray = malloc(sizeof(*systray))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	systray->helper = helper;
# if GTK_CHECK_VERSION(3, 0, 0)
	systray->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
# else
	systray->hbox = gtk_hbox_new(FALSE, 0);
# endif
	gtk_icon_size_lookup(panel_window_get_icon_size(helper->window), NULL,
			&height);
	gtk_widget_set_size_request(systray->hbox, -1, height);
	systray->owner = NULL;
	systray->source = g_signal_connect(systray->hbox, "screen-changed",
			G_CALLBACK(_systray_on_screen_changed), systray);
	gtk_widget_show(systray->hbox);
	*widget = systray->hbox;
	return systray;
#else
	(void) helper;
	(void) widget;

	error_set_code(-ENOSYS, "X11 support not detected");
	return NULL;
#endif
}


/* systray_destroy */
static void _systray_destroy(Systray * systray)
{
#if defined(GDK_WINDOWING_X11)
	GdkWindow * window;

	if(systray->source != 0)
		g_signal_handler_disconnect(systray->hbox, systray->source);
	systray->source = 0;
	if(systray->owner != NULL)
	{
		window = gtk_widget_get_window(systray->owner);
		gdk_window_remove_filter(window, _systray_on_filter, systray);
		gtk_widget_destroy(systray->owner);
	}
	gtk_widget_destroy(systray->hbox);
	free(systray);
#else
	(void) systray;
#endif
}


#if defined(GDK_WINDOWING_X11)
/* callbacks */
/* systray_on_filter */
static GdkFilterReturn _filter_clientmessage(Systray * systray,
		XClientMessageEvent * xev);

static GdkFilterReturn _systray_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data)
{
	Systray * systray = data;
	XEvent * xev = xevent;
	(void) event;

# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
# endif
	if(xev->type == ClientMessage)
		return _filter_clientmessage(systray, &xev->xclient);
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn _filter_clientmessage(Systray * systray,
		XClientMessageEvent * xev)
{
	GtkWidget * socket;

	switch(xev->data.l[1])
	{
		case SYSTEM_TRAY_REQUEST_DOCK:
			if(xev->data.l[2] == 0)
				return GDK_FILTER_CONTINUE;
			socket = gtk_socket_new();
			gtk_widget_show(socket);
			gtk_box_pack_start(GTK_BOX(systray->hbox), socket,
					FALSE, TRUE, 0);
			gtk_socket_add_id(GTK_SOCKET(socket), xev->data.l[2]);
# ifdef DEBUG
			fprintf(stderr, "DEBUG: %s() %ld\n", __func__,
					xev->data.l[2]);
# endif
			return GDK_FILTER_REMOVE;
	}
	return GDK_FILTER_CONTINUE;
}


/* systray_on_owner_destroy */
static void _systray_on_owner_destroy(gpointer data)
{
	Systray * systray = data;
	GdkWindow * window;

	if(systray->owner != NULL
			&& (window = gtk_widget_get_window(systray->owner))
			!= NULL)
		gdk_window_remove_filter(window, _systray_on_filter, systray);
	systray->owner = NULL;
}


/* systray_on_screen_changed */
static void _systray_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data)
{
	Systray * systray = data;
	const char name[] = "_NET_SYSTEM_TRAY_S";
	char buf[sizeof(name) + 2];
	GdkScreen * screen;
	GdkAtom atom;
	GdkDisplay * display;
	GdkWindow * root;
	GdkWindow * window;
	XEvent xev;
	(void) previous;

	if(systray->owner != NULL)
		return;
	screen = gtk_widget_get_screen(widget);
	snprintf(buf, sizeof(buf), "%s%d", name, gdk_screen_get_number(screen));
	atom = gdk_atom_intern(buf, FALSE);
	systray->owner = gtk_invisible_new();
	g_signal_connect_swapped(systray->owner, "destroy", G_CALLBACK(
				_systray_on_owner_destroy), systray);
	gtk_widget_realize(systray->owner);
	window = gtk_widget_get_window(systray->owner);
	if(gdk_selection_owner_set(window, atom, gtk_get_current_event_time(),
				TRUE) != TRUE)
		return;
	display = gtk_widget_get_display(widget);
	root = gdk_screen_get_root_window(screen);
	memset(&xev, 0, sizeof(xev));
	xev.xclient.type = ClientMessage;
	xev.xclient.window = GDK_WINDOW_XID(root);
	xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display(
			display, "MANAGER");
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = gtk_get_current_event_time();
	xev.xclient.data.l[1] = gdk_x11_atom_to_xatom(atom);
	xev.xclient.data.l[2] = GDK_WINDOW_XID(window);
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XID(root), False,
			StructureNotifyMask, &xev);
	gdk_error_trap_pop();
	gtk_widget_add_events(systray->owner, GDK_PROPERTY_CHANGE_MASK
			| GDK_STRUCTURE_MASK);
	gdk_window_add_filter(window, _systray_on_filter, systray);
}
#endif
