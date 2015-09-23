/* $Id$ */
/* Copyright (c) 2014-2015 Pierre Pronchery <khorben@defora.org> */
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



#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <System.h>
#include "Panel/applet.h"


/* User */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} User;


/* prototypes */
static User * _user_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _user_destroy(User * user);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"User",
	"user-info",
	NULL,
	_user_init,
	_user_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* user_init */
static User * _user_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	User * user;
	struct passwd * pw;
	PangoFontDescription * desc;

	if((pw = getpwuid(getuid())) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	if((user = object_new(sizeof(*user))) == NULL)
		return NULL;
	user->helper = helper;
	user->widget = gtk_label_new(pw->pw_name);
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	gtk_widget_modify_font(user->widget, desc);
	pango_font_description_free(desc);
	gtk_widget_show(user->widget);
	*widget = user->widget;
	return user;
}


/* user_destroy */
static void _user_destroy(User * user)
{
	gtk_widget_destroy(user->widget);
	object_delete(user);
}
