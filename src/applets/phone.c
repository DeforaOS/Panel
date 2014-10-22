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



#include <stdlib.h>
#include <Desktop.h>
#if GTK_CHECK_VERSION(3, 0, 0)
# include <gtk/gtkx.h>
#endif
#include <gdk/gdkx.h>
#include "Panel.h"


/* Phone */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} Phone;


/* constants */
#define PHONE_EMBED_MESSAGE	"DEFORAOS_DESKTOP_PHONE_EMBED"


/* prototypes */
static Phone * _phone_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _phone_destroy(Phone * phone);

static void _phone_embed(GtkWidget * widget, unsigned long window);

/* callbacks */
static int _on_message(void * data, uint32_t value1, uint32_t value2,
		uint32_t value3);
static void _on_plug_added(GtkWidget * widget);
static gboolean _on_plug_removed(GtkWidget * widget);
static void _on_screen_changed(GtkWidget * widget, GdkScreen * previous);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Phone",
	"phone-dialer",
	NULL,
	_phone_init,
	_phone_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* Phone */
/* phone_init */
static Phone * _phone_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Phone * phone;

	if((phone = malloc(sizeof(*phone))) == NULL)
		return NULL;
	phone->helper = helper;
	phone->widget = gtk_socket_new();
	g_signal_connect(phone->widget, "plug-added", G_CALLBACK(
				_on_plug_added), NULL);
	g_signal_connect(phone->widget, "plug-removed", G_CALLBACK(
				_on_plug_removed), NULL);
	g_signal_connect(phone->widget, "screen-changed", G_CALLBACK(
				_on_screen_changed), NULL);
	*widget = phone->widget;
	return phone;
}


/* phone_destroy */
static void _phone_destroy(Phone * phone)
{
	desktop_message_unregister(NULL, _on_message, phone->widget);
	gtk_widget_destroy(phone->widget);
	free(phone);
}


/* phone_embed */
static void _phone_embed(GtkWidget * widget, unsigned long window)
{
	gtk_socket_add_id(GTK_SOCKET(widget), window);
}


/* callbacks */
/* on_message */
static int _on_message(void * data, uint32_t value1, uint32_t value2,
		uint32_t value3)
{
	GtkWidget * widget = data;

	_phone_embed(widget, value1);
	return 0;
}


/* on_plug_added */
static void _on_plug_added(GtkWidget * widget)
{
	gtk_widget_show(widget);
}


/* on_plug_removed */
static gboolean _on_plug_removed(GtkWidget * widget)
{
	gtk_widget_hide(widget);
	return TRUE;
}


/* on_screen_changed */
static void _on_screen_changed(GtkWidget * widget, GdkScreen * previous)
{
	if(previous != NULL)
		return;
	desktop_message_unregister(NULL, _on_message, widget);
	desktop_message_register(NULL, PHONE_EMBED_MESSAGE, _on_message,
			widget);
}
