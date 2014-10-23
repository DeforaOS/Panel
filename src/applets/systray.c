/* $Id$ */
/* Copyright (c) 2010-2014 Pierre Pronchery <khorben@defora.org> */
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
#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(3, 0, 0)
# include <gtk/gtkx.h>
#endif
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include "Panel.h"


/* Systray */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * hbox;
	GtkWidget * owner;
	gulong source;
} Systray;


/* constants */
#define SYSTEM_TRAY_REQUEST_DOCK	0
#define SYSTEM_TRAY_BEGIN_MESSAGE	1
#define SYSTEM_TRAY_CANCEL_MESSAGE	2


/* prototypes */
static Systray * _systray_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _systray_destroy(Systray * systray);

/* callbacks */
static GdkFilterReturn _on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data);
static void _on_owner_destroy(gpointer data);
static void _on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"System tray",
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
	Systray * systray;
	gint height = 24;

	if((systray = malloc(sizeof(*systray))) == NULL)
	{
		helper->error(NULL, "malloc", 1);
		return NULL;
	}
	systray->helper = helper;
#if GTK_CHECK_VERSION(3, 0, 0)
	systray->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	systray->hbox = gtk_hbox_new(FALSE, 0);
#endif
	gtk_icon_size_lookup(helper->icon_size, NULL, &height);
	gtk_widget_set_size_request(systray->hbox, -1, height);
	systray->owner = NULL;
	systray->source = g_signal_connect(systray->hbox, "screen-changed",
			G_CALLBACK(_on_screen_changed), systray);
	gtk_widget_show(systray->hbox);
	*widget = systray->hbox;
	return systray;
}


/* systray_destroy */
static void _systray_destroy(Systray * systray)
{
	if(systray->source != 0)
		g_signal_handler_disconnect(systray->hbox, systray->source);
	systray->source = 0;
	if(systray->owner != NULL)
		gtk_widget_destroy(systray->owner);
	gtk_widget_destroy(systray->hbox);
	free(systray);
}


/* callbacks */
/* on_filter */
static GdkFilterReturn _filter_clientmessage(Systray * systray,
		XClientMessageEvent * xev);

static GdkFilterReturn _on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data)
{
	Systray * systray = data;
	XEvent * xev = xevent;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
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
#ifdef DEBUG
			fprintf(stderr, "DEBUG: %s() %ld\n", __func__,
					xev->data.l[2]);
#endif
			return GDK_FILTER_REMOVE;
	}
	return GDK_FILTER_CONTINUE;
}


/* on_owner_destroy */
static void _on_owner_destroy(gpointer data)
{
	Systray * systray = data;
	GdkWindow * window;

	window = gtk_widget_get_window(systray->owner);
	gdk_window_remove_filter(window, _on_filter, systray);
	systray->owner = NULL;
}


/* on_screen_changed */
static void _on_screen_changed(GtkWidget * widget, GdkScreen * previous,
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

	if(systray->owner != NULL)
		return;
	screen = gtk_widget_get_screen(widget);
	snprintf(buf, sizeof(buf), "%s%d", name, gdk_screen_get_number(screen));
	atom = gdk_atom_intern(buf, FALSE);
	systray->owner = gtk_invisible_new();
	g_signal_connect_swapped(systray->owner, "destroy", G_CALLBACK(
				_on_owner_destroy), systray);
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
	XSendEvent(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XID(root), False,
			StructureNotifyMask, &xev);
	gtk_widget_add_events(systray->owner, GDK_PROPERTY_CHANGE_MASK
			| GDK_STRUCTURE_MASK);
	gdk_window_add_filter(window, _on_filter, systray);
}
