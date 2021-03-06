/* $Id$ */
/* Copyright (c) 2011-2020 Pierre Pronchery <khorben@defora.org> */
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
#include <System.h>
#include "Panel/applet.h"
#define N_(string) string


/* Separator */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} Separator;


/* prototypes */
/* plug-in */
static Separator * _separator_init(PanelAppletHelper * helper,
		GtkWidget ** widget);
static void _separator_destroy(Separator * separator);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Separator"),
	NULL,
	NULL,
	_separator_init,
	_separator_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
static Separator * _separator_init(PanelAppletHelper * helper,
		GtkWidget ** widget)
{
	Separator * separator;
	GtkOrientation orientation;

	if((separator = malloc(sizeof(*separator))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	separator->helper = helper;
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	separator->widget = gtk_separator_new(orientation);
#else
	separator->widget = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? gtk_vseparator_new() : gtk_hseparator_new();
#endif
	gtk_widget_show(separator->widget);
	*widget = separator->widget;
	return separator;
}


/* separator_destroy */
static void _separator_destroy(Separator * separator)
{
	gtk_widget_destroy(separator->widget);
	free(separator);
}
