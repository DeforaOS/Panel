/* $Id$ */
/* Copyright (c) 2010-2023 Pierre Pronchery <khorben@defora.org> */
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
#include <string.h>
#include <errno.h>
#include <libintl.h>
#ifdef GDK_WINDOWING_X11
# include <gdk/gdkx.h>
# include <X11/Xatom.h>
#endif
#include <System.h>
#include <Desktop.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string

#if !GTK_CHECK_VERSION(3, 0, 0)
# define gdk_error_trap_pop_ignored() gdk_error_trap_pop()
#endif


/* Pager */
/* private */
/* types */
typedef enum _PagerAtom
{
	PAGER_ATOM_NET_CURRENT_DESKTOP = 0,
	PAGER_ATOM_NET_DESKTOP_NAMES,
	PAGER_ATOM_NET_NUMBER_OF_DESKTOPS,
	PAGER_ATOM_UTF8_STRING
} PagerAtom;
#define PAGER_ATOM_LAST PAGER_ATOM_UTF8_STRING
#define PAGER_ATOM_COUNT (PAGER_ATOM_LAST + 1)

typedef struct _PanelApplet
{
	PanelAppletHelper * helper;

	GtkWidget * box;
	gulong source;

	GtkWidget ** widgets;
	size_t widgets_cnt;

	Atom atoms[PAGER_ATOM_COUNT];
	GdkDisplay * display;
	GdkScreen * screen;
	GdkWindow * root;
} Pager;


/* constants */
static const char * _pager_atom[PAGER_ATOM_COUNT] =
{
	"_NET_CURRENT_DESKTOP",
	"_NET_DESKTOP_NAMES",
	"_NET_NUMBER_OF_DESKTOPS",
	"UTF8_STRING"
};


/* prototypes */
static Pager * _pager_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _pager_destroy(Pager * pager);

/* accessors */
static int _pager_get_current_desktop(Pager * pager);
static char ** _pager_get_desktop_names(Pager * pager);
static int _pager_get_window_property(Pager * pager, Window window,
		PagerAtom property, Atom atom, unsigned long * cnt,
		unsigned char ** ret);

/* useful */
static void _pager_do(Pager * pager);
static void _pager_refresh(Pager * pager);

/* callbacks */
static void _pager_on_clicked(GtkWidget * widget, gpointer data);
static GdkFilterReturn _pager_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data);
static void _pager_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Pager"),
	NULL,
	NULL,
	_pager_init,
	_pager_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* pager_init */
static Pager * _pager_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Pager * pager;
	GtkOrientation orientation;

	if((pager = malloc(sizeof(*pager))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	pager->helper = helper;
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	pager->box = gtk_box_new(orientation, 0);
	gtk_box_set_homogeneous(GTK_BOX(pager->box), TRUE);
#else
	pager->box = (orientation == GTK_ORIENTATION_VERTICAL)
		? gtk_vbox_new(TRUE, 0) : gtk_hbox_new(TRUE, 0);
#endif
	pager->source = g_signal_connect(pager->box, "screen-changed",
			G_CALLBACK(_pager_on_screen_changed), pager);
	pager->widgets = NULL;
	pager->widgets_cnt = 0;
	pager->display = NULL;
	pager->screen = NULL;
	pager->root = NULL;
	*widget = pager->box;
	return pager;
}


/* pager_destroy */
static void _pager_destroy(Pager * pager)
{
	if(pager->source != 0)
		g_signal_handler_disconnect(pager->box, pager->source);
	pager->source = 0;
	if(pager->root != NULL)
		gdk_window_remove_filter(pager->root, _pager_on_filter, pager);
	gtk_widget_destroy(pager->box);
	free(pager);
}


/* accessors */
/* pager_get_current_desktop */
static int _pager_get_current_desktop(Pager * pager)
{
	unsigned long cnt;
	unsigned long * p;

	if(_pager_get_window_property(pager, GDK_WINDOW_XID(pager->root),
				PAGER_ATOM_NET_CURRENT_DESKTOP, XA_CARDINAL,
				&cnt, (void*)&p) != 0)
		return -1;
	cnt = *p;
	XFree(p);
	return cnt;
}


/* pager_get_desktop_names */
static char ** _pager_get_desktop_names(Pager * pager)
{
	char ** ret = NULL;
	size_t ret_cnt = 0;
	unsigned long cnt;
	char * p;
	unsigned long i;
	unsigned long last = 0;
	char ** q;

	if(_pager_get_window_property(pager, GDK_WINDOW_XID(pager->root),
				PAGER_ATOM_NET_DESKTOP_NAMES,
				pager->atoms[PAGER_ATOM_UTF8_STRING], &cnt,
				(void*)&p) != 0)
		return NULL;
	for(i = 0; i < cnt; i++)
	{
		if(p[i] != '\0')
			continue;
		if((q = realloc(ret, (ret_cnt + 2) * (sizeof(*q)))) == NULL)
		{
			free(ret);
			XFree(p);
			return NULL;
		}
		ret = q;
		/* FIXME validate the UTF8 string */
		ret[ret_cnt++] = g_strdup(&p[last]);
		last = i + 1;
	}
	XFree(p);
	if(ret == NULL)
		return ret;
	ret[ret_cnt] = NULL;
	return ret;
}


/* pager_get_window_property */
static int _pager_get_window_property(Pager * pager, Window window,
		PagerAtom property, Atom atom, unsigned long * cnt,
		unsigned char ** ret)
{
	int res;
	Atom type;
	int format;
	unsigned long bytes;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(pager, window, %s, %lu)\n", __func__,
			_pager_atom[property], atom);
#endif
	gdk_error_trap_push();
	res = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(pager->display), window,
			pager->atoms[property], 0, G_MAXLONG, False, atom,
			&type, &format, cnt, &bytes, ret);
	if(gdk_error_trap_pop() != 0 || res != Success)
		return -1;
	if(type != atom)
	{
		if(*ret != NULL)
			XFree(*ret);
		*ret = NULL;
		return 1;
	}
	return 0;
}


/* useful */
/* pager_do */
static void _pager_do(Pager * pager)
{
	unsigned long cnt = 0;
	unsigned long l;
	unsigned long * p;
	unsigned long i;
	GtkWidget ** q;
	char ** names;
	char buf[64];

	if(_pager_get_window_property(pager, GDK_WINDOW_XID(pager->root),
				PAGER_ATOM_NET_NUMBER_OF_DESKTOPS,
				XA_CARDINAL, &cnt, (void*)&p) != 0)
		return;
	l = *p;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() l=%ld\n", __func__, l);
#endif
	XFree(p);
	for(i = l; i < pager->widgets_cnt; i++)
		if(pager->widgets[i] != NULL)
			gtk_widget_destroy(pager->widgets[i]);
	if((q = realloc(pager->widgets, l * sizeof(*q))) == NULL
			&& l != 0)
		return;
	pager->widgets = q;
	names = _pager_get_desktop_names(pager);
	for(i = 0; i < l; i++)
	{
		if(names != NULL && names[i] != NULL)
		{
			snprintf(buf, sizeof(buf), "%s", names[i]);
			g_free(names[i]);
		}
		else
			snprintf(buf, sizeof(buf), _("Desk %lu"), i + 1);
		if(i < pager->widgets_cnt)
			gtk_button_set_label(GTK_BUTTON(pager->widgets[i]),
					buf);
		else
		{
			pager->widgets[i] = gtk_button_new_with_label(buf);
			g_signal_connect(pager->widgets[i], "clicked",
					G_CALLBACK( _pager_on_clicked), pager);
			gtk_box_pack_start(GTK_BOX(pager->box),
					pager->widgets[i], FALSE, TRUE, 0);
		}
	}
	free(names);
	pager->widgets_cnt = l;
	_pager_refresh(pager);
	if(pager->widgets_cnt <= 1)
		gtk_widget_hide(pager->box);
	else
		gtk_widget_show_all(pager->box);
}


/* pager_refresh */
static void _pager_refresh(Pager * pager)
{
	size_t i;
	int cur;
	char buf[64];

	cur = _pager_get_current_desktop(pager);
	for(i = 0; i < pager->widgets_cnt; i++)
		if(cur < 0 || i != (unsigned int)cur)
		{
			gtk_widget_set_sensitive(pager->widgets[i], TRUE);
#if GTK_CHECK_VERSION(2, 12, 0)
			snprintf(buf, sizeof(buf), _("Switch to %s"),
					gtk_button_get_label(
						GTK_BUTTON(pager->widgets[i])));
			gtk_widget_set_tooltip_text(pager->widgets[i], buf);
#endif
		}
		else
		{
			gtk_widget_set_sensitive(pager->widgets[i], FALSE);
#if GTK_CHECK_VERSION(2, 12, 0)
			snprintf(buf, sizeof(buf), _("On %s"),
					gtk_button_get_label(
						GTK_BUTTON(pager->widgets[i])));
			gtk_widget_set_tooltip_text(pager->widgets[i], buf);
#endif
		}
}


/* callbacks */
/* pager_on_clicked */
static void _pager_on_clicked(GtkWidget * widget, gpointer data)
{
	Pager * pager = data;
	size_t i;
	GdkScreen * screen;
	GdkDisplay * display;
	GdkWindow * root;
	XEvent xev;

	for(i = 0; i < pager->widgets_cnt; i++)
		if(pager->widgets[i] == widget)
			break;
	if(i == pager->widgets_cnt)
		return;
	screen = gtk_widget_get_screen(widget);
	display = gtk_widget_get_display(widget);
	root = gdk_screen_get_root_window(screen);
	xev.xclient.type = ClientMessage;
	xev.xclient.window = GDK_WINDOW_XID(root);
	xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display(
			display, "_NET_CURRENT_DESKTOP");
	xev.xclient.format = 32;
	memset(&xev.xclient.data, 0, sizeof(xev.xclient.data));
	xev.xclient.data.l[0] = i;
	xev.xclient.data.l[1] = gdk_x11_display_get_user_time(display);
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XID(root),
			False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
	gdk_error_trap_pop_ignored();
}


/* pager_on_filter */
static GdkFilterReturn _pager_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data)
{
	Pager * pager = data;
	XEvent * xev = xevent;
	int cur;
	(void) event;

	if(xev->type != PropertyNotify)
		return GDK_FILTER_CONTINUE;
	if(xev->xproperty.atom == pager->atoms[PAGER_ATOM_NET_CURRENT_DESKTOP])
	{
		if((cur = _pager_get_current_desktop(pager)) >= 0)
			_pager_refresh(pager);
		return GDK_FILTER_CONTINUE;
	}
	if(xev->xproperty.atom == pager->atoms[
			PAGER_ATOM_NET_NUMBER_OF_DESKTOPS]
			|| xev->xproperty.atom == pager->atoms[
			PAGER_ATOM_NET_DESKTOP_NAMES])
		_pager_do(pager);
	return GDK_FILTER_CONTINUE;
}


/* pager_on_screen_changed */
static void _pager_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data)
{
	Pager * pager = data;
	GdkEventMask events;
	size_t i;
	(void) previous;

	if(pager->root != NULL)
		gdk_window_remove_filter(pager->root, _pager_on_filter, pager);
	pager->screen = gtk_widget_get_screen(widget);
	pager->display = gdk_screen_get_display(pager->screen);
	pager->root = gdk_screen_get_root_window(pager->screen);
	events = gdk_window_get_events(pager->root);
	gdk_window_set_events(pager->root, events | GDK_PROPERTY_CHANGE_MASK);
	gdk_window_add_filter(pager->root, _pager_on_filter, pager);
	/* atoms */
	for(i = 0; i < PAGER_ATOM_COUNT; i++)
		pager->atoms[i] = gdk_x11_get_xatom_by_name_for_display(
				pager->display, _pager_atom[i]);
	_pager_do(pager);
}
