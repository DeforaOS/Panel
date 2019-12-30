/* $Id$ */
/* Copyright (c) 2010-2020 Pierre Pronchery <khorben@defora.org> */
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
#define N_(string) string


/* Suspend */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} Suspend;


/* prototypes */
static Suspend * _suspend_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _suspend_destroy(Suspend * suspend);

/* callbacks */
static void _suspend_on_clicked(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Suspend"),
	"gtk-media-pause",	/* XXX find a better icon */
	NULL,
	_suspend_init,
	_suspend_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* suspend_init */
static Suspend * _suspend_init(PanelAppletHelper * helper,
		GtkWidget ** widget)
{
	Suspend * suspend;
	GtkWidget * image;

	if((suspend = malloc(sizeof(*suspend))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	suspend->helper = helper;
	if(helper->suspend_dialog == NULL)
	{
		error_set("%s: %s", applet.name,
				_("Suspending is not allowed"));
		return NULL;
	}
	suspend->widget = gtk_button_new();
	image = gtk_image_new_from_icon_name(applet.icon,
			panel_window_get_icon_size(helper->window));
	gtk_button_set_image(GTK_BUTTON(suspend->widget), image);
	gtk_button_set_relief(GTK_BUTTON(suspend->widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(suspend->widget, _("Suspend"));
#endif
	g_signal_connect_swapped(suspend->widget, "clicked", G_CALLBACK(
				_suspend_on_clicked), suspend);
	gtk_widget_show_all(suspend->widget);
	*widget = suspend->widget;
	return suspend;
}


/* suspend_destroy */
static void _suspend_destroy(Suspend * suspend)
{
	gtk_widget_destroy(suspend->widget);
	free(suspend);
}


/* callbacks */
/* suspend_on_clicked */
static void _suspend_on_clicked(gpointer data)
{
	Suspend * suspend = data;

	suspend->helper->suspend_dialog(suspend->helper->panel);
}
