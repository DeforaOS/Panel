/* $Id$ */
/* Copyright (c) 2010-2018 Pierre Pronchery <khorben@defora.org> */
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
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* Shutdown */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} Shutdown;


/* prototypes */
static Shutdown * _shutdown_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _shutdown_destroy(Shutdown * shutdown);

/* callbacks */
static void _shutdown_on_clicked(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Shutdown",
	"gnome-shutdown",
	NULL,
	_shutdown_init,
	_shutdown_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* shutdown_init */
static Shutdown * _shutdown_init(PanelAppletHelper * helper,
		GtkWidget ** widget)
{
	Shutdown * shutdown;
	GtkWidget * image;

	if((shutdown = malloc(sizeof(*shutdown))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	shutdown->helper = helper;
	if(helper->shutdown_dialog == NULL)
	{
		error_set("%s: %s", applet.name,
				_("Shutting down is not allowed"));
		return NULL;
	}
	shutdown->widget = gtk_button_new();
	image = gtk_image_new_from_icon_name(applet.icon,
			panel_window_get_icon_size(helper->window));
	gtk_button_set_image(GTK_BUTTON(shutdown->widget), image);
	gtk_button_set_relief(GTK_BUTTON(shutdown->widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(shutdown->widget, _("Shutdown"));
#endif
	g_signal_connect_swapped(shutdown->widget, "clicked", G_CALLBACK(
				_shutdown_on_clicked), shutdown);
	gtk_widget_show_all(shutdown->widget);
	*widget = shutdown->widget;
	return shutdown;
}


/* shutdown_destroy */
static void _shutdown_destroy(Shutdown * shutdown)
{
	gtk_widget_destroy(shutdown->widget);
	free(shutdown);
}


/* callbacks */
/* shutdown_on_clicked */
static void _shutdown_on_clicked(gpointer data)
{
	Shutdown * shutdown = data;

	shutdown->helper->shutdown_dialog(shutdown->helper->panel);
}
