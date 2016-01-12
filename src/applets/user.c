/* $Id$ */
/* Copyright (c) 2014-2016 Pierre Pronchery <khorben@defora.org> */
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
static struct passwd * _init_pw(void);
#if GTK_CHECK_VERSION(2, 12, 0)
static String * _init_tooltip(struct passwd * pw);
#endif
static String * _init_user(struct passwd * pw);

static User * _user_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	User * user;
	PangoFontDescription * desc;
	struct passwd * pw;
	String const * name = "Unknown user";
	String * p = NULL;
#if GTK_CHECK_VERSION(2, 12, 0)
	String const * tooltip = NULL;
	String * q = NULL;
#endif

	if((user = object_new(sizeof(*user))) == NULL)
		return NULL;
	user->helper = helper;
	if((pw = _init_pw()) != NULL)
	{
		if((p = _init_user(pw)) != NULL)
			name = p;
#if GTK_CHECK_VERSION(2, 12, 0)
		if((q = _init_tooltip(pw)) != NULL)
			tooltip = q;
#endif
	}
	user->widget = gtk_label_new(name);
	string_delete(p);
#if GTK_CHECK_VERSION(2, 12, 0)
	if(tooltip != NULL)
		gtk_widget_set_tooltip_text(user->widget, tooltip);
	string_delete(q);
#endif
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(user->widget, desc);
#else
	gtk_widget_modify_font(user->widget, desc);
#endif
	pango_font_description_free(desc);
	gtk_widget_show(user->widget);
	*widget = user->widget;
	return user;
}

static struct passwd * _init_pw(void)
{
	struct passwd * pw;

	if((pw = getpwuid(getuid())) == NULL)
		error_set("%s: %s", applet.name, strerror(errno));
	return pw;
}

#if GTK_CHECK_VERSION(2, 12, 0)
static String * _init_tooltip(struct passwd * pw)
{
	ssize_t len;

	if(pw->pw_gecos == NULL || strlen(pw->pw_gecos) == 0)
		return NULL;
	if((len = string_index(pw->pw_gecos, ",")) < 0)
		/* would be redundant */
		return NULL;
	/* FIXME:
	 * - ignore empty fields
	 * - return NULL if identical to _init_user() */
	return string_new_replace(pw->pw_gecos, ",", "\n");
}
#endif

static String * _init_user(struct passwd * pw)
{
	ssize_t len;

	if(pw->pw_gecos != NULL && strlen(pw->pw_gecos) > 0)
	{
		if((len = string_index(pw->pw_gecos, ",")) > 0)
			return string_new_length(pw->pw_gecos, len);
		else if(len != 0)
			return string_new(pw->pw_gecos);
	}
	return string_new(pw->pw_name);
}


/* user_destroy */
static void _user_destroy(User * user)
{
	gtk_widget_destroy(user->widget);
	object_delete(user);
}
