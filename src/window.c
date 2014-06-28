/* $Id$ */
/* Copyright (c) 2011-2014 Pierre Pronchery <khorben@defora.org> */
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



#include <System.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include "window.h"
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) (string)

/* constants */
#ifndef PROGNAME
# define PROGNAME	"panel"
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef LIBDIR
# define LIBDIR		PREFIX "/lib"
#endif


/* PanelWindow */
/* private */
/* types */
struct _PanelApplet
{
	Plugin * plugin;
	PanelAppletDefinition * pad;
	PanelApplet * pa;
	GtkWidget * widget;
};

struct _PanelWindow
{
	PanelWindowPosition position;
	gint height;
	GdkRectangle root;

	/* applets */
	PanelAppletHelper * helper;
	PanelApplet * applets;
	size_t applets_cnt;

	/* widgets */
	GtkWidget * window;
	GtkWidget * box;
};


/* prototypes */
static void _panel_window_reset(PanelWindow * panel);
static void _panel_window_reset_strut(PanelWindow * panel);

/* callbacks */
static gboolean _panel_window_on_closex(gpointer data);
static gboolean _panel_window_on_configure_event(GtkWidget * widget,
		GdkEvent * event, gpointer data);


/* public */
/* functions */
/* panel_window_new */
PanelWindow * panel_window_new(PanelWindowPosition position,
		PanelAppletHelper * helper, GdkRectangle * root)
{
	PanelWindow * panel;
	int icon_width;
	int icon_height;
	GtkOrientation orientation;

	if(gtk_icon_size_lookup(helper->icon_size, &icon_width, &icon_height)
			!= TRUE)
	{
		error_set_code(1, _("Invalid panel size"));
		return NULL;
	}
	if((panel = object_new(sizeof(*panel))) == NULL)
		return NULL;
	panel->position = position;
	panel->helper = helper;
	panel->applets = NULL;
	panel->applets_cnt = 0;
	panel->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(panel->window), 4);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_window_set_has_resize_grip(GTK_WINDOW(panel->window), FALSE);
#endif
	panel->height = icon_height + (PANEL_BORDER_WIDTH * 4);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u height=%d\n", __func__, position,
			panel->height);
#endif
	panel->box = NULL;
	orientation = panel_window_get_orientation(panel);
#if GTK_CHECK_VERSION(3, 0, 0)
	panel->box = gtk_box_new(orientation, 2);
#else
	panel->box = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? gtk_hbox_new(FALSE, 2) : gtk_vbox_new(FALSE, 2);
#endif
	switch(position)
	{
		case PANEL_WINDOW_POSITION_TOP:
		case PANEL_WINDOW_POSITION_BOTTOM:
#if GTK_CHECK_VERSION(2, 6, 0)
			gtk_window_set_focus_on_map(GTK_WINDOW(panel->window),
					FALSE);
#endif
			gtk_window_set_type_hint(GTK_WINDOW(panel->window),
					GDK_WINDOW_TYPE_HINT_DOCK);
			gtk_window_stick(GTK_WINDOW(panel->window));
			g_signal_connect(panel->window, "configure-event",
					G_CALLBACK(_panel_window_on_configure_event), panel);
			break;
		case PANEL_WINDOW_POSITION_LEFT:
		case PANEL_WINDOW_POSITION_RIGHT:
#if GTK_CHECK_VERSION(2, 6, 0)
			gtk_window_set_focus_on_map(GTK_WINDOW(panel->window),
					FALSE);
#endif
			gtk_window_set_type_hint(GTK_WINDOW(panel->window),
					GDK_WINDOW_TYPE_HINT_DOCK);
			gtk_window_stick(GTK_WINDOW(panel->window));
			g_signal_connect(panel->window, "configure-event",
					G_CALLBACK(_panel_window_on_configure_event), panel);
			break;
		case PANEL_WINDOW_POSITION_CENTER:
			gtk_window_set_position(GTK_WINDOW(panel->window),
					GTK_WIN_POS_CENTER_ALWAYS);
			/* fallback */
		case PANEL_WINDOW_POSITION_FLOATING:
			gtk_window_set_accept_focus(GTK_WINDOW(panel->window),
					FALSE);
			gtk_window_set_decorated(GTK_WINDOW(panel->window),
					FALSE);
		case PANEL_WINDOW_POSITION_MANAGED:
			break;
	}
	g_signal_connect_swapped(panel->window, "delete-event", G_CALLBACK(
				_panel_window_on_closex), panel);
	gtk_container_add(GTK_CONTAINER(panel->window), panel->box);
	gtk_widget_show_all(panel->box);
	panel_window_reset(panel, position, root);
	return panel;
}


/* panel_window_delete */
void panel_window_delete(PanelWindow * panel)
{
	panel_window_remove_all(panel);
	gtk_widget_destroy(panel->window);
	object_delete(panel);
}


/* accessors */
/* panel_window_get_height */
int panel_window_get_height(PanelWindow * panel)
{
	return panel->height;
}


/* panel_window_get_orientation */
GtkOrientation panel_window_get_orientation(PanelWindow * panel)
{
#if GTK_CHECK_VERSION(2, 16, 0)
	if(panel->box != NULL)
		return gtk_orientable_get_orientation(GTK_ORIENTABLE(
					panel->box));
#endif
	switch(panel->position)
	{
		case PANEL_WINDOW_POSITION_LEFT:
		case PANEL_WINDOW_POSITION_RIGHT:
			return GTK_ORIENTATION_VERTICAL;
		case PANEL_WINDOW_POSITION_BOTTOM:
		case PANEL_WINDOW_POSITION_CENTER:
		case PANEL_WINDOW_POSITION_FLOATING:
		case PANEL_WINDOW_POSITION_MANAGED:
		case PANEL_WINDOW_POSITION_TOP:
		default:
			return GTK_ORIENTATION_HORIZONTAL;
	}
}


/* panel_window_get_position */
void panel_window_get_position(PanelWindow * panel, gint * x, gint * y)
{
	gtk_window_get_position(GTK_WINDOW(panel->window), x, y);
}


/* panel_window_get_size */
void panel_window_get_size(PanelWindow * panel, gint * width, gint * height)
{
	gtk_window_get_size(GTK_WINDOW(panel->window), width, height);
}


/* panel_window_get_width */
int panel_window_get_width(PanelWindow * panel)
{
	gint width;

	gtk_window_get_size(GTK_WINDOW(panel->window), &width, NULL);
	return width;
}


/* panel_window_set_accept_focus */
void panel_window_set_accept_focus(PanelWindow * panel, gboolean accept)
{
	gtk_window_set_accept_focus(GTK_WINDOW(panel->window), accept);
}


/* panel_window_set_keep_above */
void panel_window_set_keep_above(PanelWindow * panel, gboolean keep)
{
	gtk_window_set_keep_above(GTK_WINDOW(panel->window), keep);
}


/* panel_window_set_title */
void panel_window_set_title(PanelWindow * panel, char const * title)
{
	gtk_window_set_title(GTK_WINDOW(panel->window), title);
}


/* useful */
/* panel_window_append */
int panel_window_append(PanelWindow * panel, char const * applet)
{
	PanelAppletHelper * helper = panel->helper;
	PanelApplet * pa;

	if((pa = realloc(panel->applets, sizeof(*pa)
					* (panel->applets_cnt + 1))) == NULL)
		return -error_set_code(1, "%s", strerror(errno));
	panel->applets = pa;
	pa = &panel->applets[panel->applets_cnt];
	if((pa->plugin = plugin_new(LIBDIR, PACKAGE, "applets", applet))
			== NULL)
		return -1;
	pa->widget = NULL;
	if((pa->pad = plugin_lookup(pa->plugin, "applet")) == NULL
			|| (pa->pa = pa->pad->init(helper, &pa->widget)) == NULL
			|| pa->widget == NULL)
	{
		if(pa->pa != NULL)
			pa->pad->destroy(pa->pa);
		plugin_delete(pa->plugin);
		return -1;
	}
	gtk_box_pack_start(GTK_BOX(panel->box), pa->widget, pa->pad->expand,
			pa->pad->fill, 0);
	gtk_widget_show(pa->widget);
	panel->applets_cnt++;
	return 0;
}


/* panel_window_remove_all */
void panel_window_remove_all(PanelWindow * panel)
{
	size_t i;
	PanelApplet * pa;

	for(i = 0; i < panel->applets_cnt; i++)
	{
		pa = &panel->applets[i];
		pa->pad->destroy(pa->pa);
		plugin_delete(pa->plugin);
	}
	free(panel->applets);
	panel->applets = NULL;
	panel->applets_cnt = 0;
}


/* panel_window_reset */
void panel_window_reset(PanelWindow * panel, PanelWindowPosition position,
		GdkRectangle * root)
{
	panel->position = position;
	memcpy(&panel->root, root, sizeof(*root));
	_panel_window_reset(panel);
}


/* panel_window_show */
void panel_window_show(PanelWindow * panel, gboolean show)
{
	if(show)
		gtk_widget_show(panel->window);
	else
		gtk_widget_hide(panel->window);
}


/* private */
/* functions */
/* panel_window_reset */
static void _panel_window_reset(PanelWindow * panel)
{
	switch(panel->position)
	{
		case PANEL_WINDOW_POSITION_TOP:
			gtk_window_move(GTK_WINDOW(panel->window),
					panel->root.x, 0);
			gtk_window_resize(GTK_WINDOW(panel->window),
					panel->root.width, panel->height);
			break;
		case PANEL_WINDOW_POSITION_BOTTOM:
			gtk_window_move(GTK_WINDOW(panel->window),
					panel->root.x,
					panel->root.y + panel->root.height
					- panel->height);
			gtk_window_resize(GTK_WINDOW(panel->window),
					panel->root.width, panel->height);
			break;
		case PANEL_WINDOW_POSITION_LEFT:
			/* FIXME really implement */
			gtk_window_move(GTK_WINDOW(panel->window),
					panel->root.x, 0);
			gtk_window_resize(GTK_WINDOW(panel->window), 48,
					panel->root.height);
			break;
		case PANEL_WINDOW_POSITION_RIGHT:
			/* FIXME really implement */
			gtk_window_move(GTK_WINDOW(panel->window),
					panel->root.x + panel->root.width - 48,
					panel->root.y);
			gtk_window_resize(GTK_WINDOW(panel->window), 48,
					panel->root.height);
			break;
		case PANEL_WINDOW_POSITION_CENTER:
		case PANEL_WINDOW_POSITION_FLOATING:
		case PANEL_WINDOW_POSITION_MANAGED:
			break;
	}
}


/* panel_window_reset_strut */
static void _panel_window_reset_strut(PanelWindow * panel)
{
	GdkWindow * window;
	GdkAtom atom;
	GdkAtom cardinal;
	unsigned long strut[12];

#if GTK_CHECK_VERSION(2, 14, 0)
	window = gtk_widget_get_window(panel->window);
#else
	window = panel->window->window;
#endif
	memset(&strut, 0, sizeof(strut));
	/* FIXME check that this code is correct */
	switch(panel->position)
	{
		case PANEL_WINDOW_POSITION_TOP:
			strut[2] = panel->height;
			strut[8] = panel->root.x;
			strut[9] = panel->root.x + panel->root.width;
			break;
		case PANEL_WINDOW_POSITION_BOTTOM:
			strut[3] = panel->height;
			strut[10] = panel->root.x;
			strut[11] = panel->root.x + panel->root.width;
			break;
#if 0 /* FIXME implement */
		case PANEL_WINDOW_POSITION_LEFT:
		case PANEL_WINDOW_POSITION_RIGHT:
			break;
#endif
		case PANEL_WINDOW_POSITION_CENTER:
		case PANEL_WINDOW_POSITION_FLOATING:
		case PANEL_WINDOW_POSITION_MANAGED:
			break;
	}
	cardinal = gdk_atom_intern("CARDINAL", FALSE);
	atom = gdk_atom_intern("_NET_WM_STRUT", FALSE);
	gdk_property_change(window, atom, cardinal, 32, GDK_PROP_MODE_REPLACE,
			(guchar *)strut, 4);
	atom = gdk_atom_intern("_NET_WM_STRUT_PARTIAL", FALSE);
	gdk_property_change(window, atom, cardinal, 32, GDK_PROP_MODE_REPLACE,
			(guchar *)strut, 12);
}


/* callbacks */
/* panel_window_on_closex */
static gboolean _panel_window_on_closex(gpointer data)
{
	PanelWindow * panel = data;

	panel_window_show(panel, FALSE);
	gtk_main_quit();
	return TRUE;
}


/* panel_window_on_configure_event */
static gboolean _panel_window_on_configure_event(GtkWidget * widget,
		GdkEvent * event, gpointer data)
{
	PanelWindow * panel = data;
	GdkEventConfigure * cevent = &event->configure;

	if(event->type != GDK_CONFIGURE)
		return FALSE;
	panel->height = cevent->height;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u height=%d\n", __func__, panel->position,
			panel->height);
#endif
	/* move to the proper position again if necessary */
	if((panel->position == PANEL_WINDOW_POSITION_TOP
				&& cevent->y != panel->root.y)
			|| (panel->position == PANEL_WINDOW_POSITION_BOTTOM
				&& cevent->y + cevent->height
				!= panel->root.height))
		_panel_window_reset(panel);
	else
		_panel_window_reset_strut(panel);
	return FALSE;
}
