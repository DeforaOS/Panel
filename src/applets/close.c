/* $Id$ */
/* Copyright (c) 2011-2024 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
# include <gdk/gdkx.h>
# include <X11/Xatom.h>
#endif
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string

#if !GTK_CHECK_VERSION(3, 0, 0)
# define gdk_error_trap_pop_ignored() gdk_error_trap_pop()
#endif


/* Close */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
	gulong source;

	GdkDisplay * display;
	GdkScreen * screen;
	GdkWindow * root;

#if defined(GDK_WINDOWING_X11)
	Atom atom_active;
	Atom atom_close;
	Window window;
	Window panel;
#endif
} Close;


/* prototypes */
/* close */
static Close * _close_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _close_destroy(Close * close);

/* useful */
static void _close_do(Close * close);

/* callbacks */
#if defined(GDK_WINDOWING_X11)
static void _close_on_close(gpointer data);
static GdkFilterReturn _close_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data);
static void _close_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data);
#endif


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Close"),
	GTK_STOCK_CLOSE,
	NULL,
	_close_init,
	_close_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* close_init */
static Close * _close_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Close * close;
	GtkIconSize iconsize;
	GtkWidget * image;

	if((close = object_new(sizeof(*close))) == NULL)
		return NULL;
	close->helper = helper;
	close->widget = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(close->widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(close->widget, _("Close"));
#endif
	iconsize = panel_window_get_icon_size(helper->window);
#if GTK_CHECK_VERSION(3, 10, 0)
	image = gtk_image_new_from_icon_name(applet.icon, iconsize);
#else
	image = gtk_image_new_from_stock(applet.icon, iconsize);
#endif
	gtk_button_set_image(GTK_BUTTON(close->widget), image);
#if defined(GDK_WINDOWING_X11)
	g_signal_connect_swapped(close->widget, "clicked", G_CALLBACK(
				_close_on_close), close);
	close->source = g_signal_connect(close->widget, "screen-changed",
			G_CALLBACK(_close_on_screen_changed), close);
#else
	close->source = 0;
#endif
	close->display = NULL;
	close->screen = NULL;
	close->root = NULL;
#if defined(GDK_WINDOWING_X11)
	close->atom_active = 0;
	close->atom_close = 0;
	close->window = None;
	close->panel = None;
#endif
	gtk_widget_show(close->widget);
	*widget = close->widget;
	return close;
}


/* close_destroy */
static void _close_destroy(Close * close)
{
	if(close->source != 0)
		g_signal_handler_disconnect(close->widget, close->source);
#if defined(GDK_WINDOWING_X11)
	if(close->root != NULL)
		gdk_window_remove_filter(close->root, _close_on_filter, close);
#endif
	gtk_widget_destroy(close->widget);
	object_delete(close);
}


#if defined(GDK_WINDOWING_X11)
/* accessors */
/* close_get_window_property */
static int _close_get_window_property(Close * close, Window window,
		Atom property, Atom atom, unsigned long * cnt,
		unsigned char ** ret)
{
	int res;
	Atom type;
	int format;
	unsigned long bytes;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(close, window, %lu, %lu)\n", __func__,
			property, atom);
#endif
	gdk_error_trap_push();
	res = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(close->display), window,
			property, 0, G_MAXLONG, False, atom, &type, &format,
			cnt, &bytes, ret);
	if(gdk_error_trap_pop() != 0 || res != Success)
		return 1;
	if(type != atom)
	{
		if(*ret != NULL)
			XFree(*ret);
		*ret = NULL;
		return 1;
	}
	return 0;
}


/* close_do */
static void _close_do(Close * close)
{
	unsigned long cnt = 0;
	Window * window;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(_close_get_window_property(close, GDK_WINDOW_XID(close->root),
				close->atom_active, XA_WINDOW, &cnt,
				(void*)&window) != 0 || cnt != 1)
		return;
	if(*window != close->panel)
		close->window = *window;
	XFree(window);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u\n", __func__, close->window);
#endif
}


/* callbacks */
/* on_close */
static void _close_on_close(gpointer data)
{
	Close * close = data;
	GdkDisplay * display;
	XEvent xev;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u\n", __func__, close->window);
#endif
	if(close->window == None)
		return;
	display = close->display;
	memset(&xev, 0, sizeof(xev));
	xev.xclient.type = ClientMessage;
	xev.xclient.window = close->window;
	xev.xclient.message_type = close->atom_close;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = gdk_x11_display_get_user_time(display);
	xev.xclient.data.l[1] = 2;
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XID(close->root),
			False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
	gdk_error_trap_pop_ignored();
}


/* close_on_filter */
static GdkFilterReturn _close_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data)
{
	Close * close = data;
	XEvent * xev = xevent;
	(void) event;

	if(xev->type != PropertyNotify)
		return GDK_FILTER_CONTINUE;
	if(xev->xproperty.atom != close->atom_active)
		return GDK_FILTER_CONTINUE;
	_close_do(close);
	return GDK_FILTER_CONTINUE;
}


/* close_on_screen_changed */
static void _close_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data)
{
	Close * close = data;
	GdkWindow * window;
	GdkEventMask events;
	(void) previous;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(close->root != NULL)
		gdk_window_remove_filter(close->root, _close_on_filter, close);
	close->screen = gtk_widget_get_screen(widget);
	close->display = gdk_screen_get_display(close->screen);
	close->root = gdk_screen_get_root_window(close->screen);
	close->panel = ((window = gtk_widget_get_parent_window(widget)) != NULL)
		? GDK_WINDOW_XID(window) : None;
	events = gdk_window_get_events(close->root);
	gdk_window_set_events(close->root, events
			| GDK_PROPERTY_CHANGE_MASK);
	gdk_window_add_filter(close->root, _close_on_filter, close);
	close->atom_active = gdk_x11_get_xatom_by_name_for_display(
			close->display, "_NET_ACTIVE_WINDOW");
	close->atom_close = gdk_x11_get_xatom_by_name_for_display(
			close->display, "_NET_CLOSE_WINDOW");
	_close_do(close);
}
#endif
