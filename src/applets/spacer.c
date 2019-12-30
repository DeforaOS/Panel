/* $Id$ */
/* Copyright (c) 2011-2015 Pierre Pronchery <khorben@defora.org> */
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


/* Spacer */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} Spacer;


/* prototypes */
/* plug-in */
static Spacer * _spacer_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _spacer_destroy(Spacer * spacer);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Spacer"),
	NULL,
	NULL,
	_spacer_init,
	_spacer_destroy,
	NULL,
	TRUE,
	TRUE
};


/* private */
/* functions */
static Spacer * _spacer_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Spacer * spacer;

	if((spacer = malloc(sizeof(*spacer))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	spacer->helper = helper;
	spacer->widget = gtk_label_new(NULL);
	gtk_widget_show(spacer->widget);
	*widget = spacer->widget;
	return spacer;
}


/* spacer_destroy */
static void _spacer_destroy(Spacer * spacer)
{
	gtk_widget_destroy(spacer->widget);
	free(spacer);
}
