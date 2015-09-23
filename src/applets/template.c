/* $Id$ */
/* Copyright (c) 2012-2015 Pierre Pronchery <khorben@defora.org> */
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
#include "Panel/applet.h"


/* Template */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} Template;


/* prototypes */
static Template * _template_init(PanelAppletHelper * helper,
		GtkWidget ** widget);
static void _template_destroy(Template * template);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Template",
	"image-missing",
	NULL,
	_template_init,
	_template_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* template_init */
static Template * _template_init(PanelAppletHelper * helper,
		GtkWidget ** widget)
{
	Template * template;

	if((template = object_new(sizeof(*template))) == NULL)
		return NULL;
	template->helper = helper;
	template->widget = gtk_label_new("Template");
	gtk_widget_show(template->widget);
	*widget = template->widget;
	return template;
}


/* template_destroy */
static void _template_destroy(Template * template)
{
	gtk_widget_destroy(template->widget);
	object_delete(template);
}
