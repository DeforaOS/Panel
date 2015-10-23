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
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* Lock */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;

	/* preferences */
	GtkWidget * pr_box;
	GtkWidget * pr_command;
} Lock;


/* prototypes */
static Lock * _lock_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _lock_destroy(Lock * lock);
static GtkWidget * _lock_settings(Lock * lock, gboolean apply, gboolean reset);

/* callbacks */
static void _lock_on_clicked(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Lock screen",
	"gnome-lockscreen",
	NULL,
	_lock_init,
	_lock_destroy,
	_lock_settings,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* lock_init */
static Lock * _lock_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Lock * lock;
	GtkIconSize iconsize;
	GtkWidget * image;

	if((lock = malloc(sizeof(*lock))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	lock->helper = helper;
	lock->pr_box = NULL;
	lock->pr_command = NULL;
	lock->widget = gtk_button_new();
	iconsize = panel_window_get_icon_size(helper->window);
	image = gtk_image_new_from_icon_name("gnome-lockscreen", iconsize);
	gtk_button_set_image(GTK_BUTTON(lock->widget), image);
	gtk_button_set_relief(GTK_BUTTON(lock->widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(lock->widget, _("Lock screen"));
#endif
	g_signal_connect_swapped(lock->widget, "clicked", G_CALLBACK(
				_lock_on_clicked), helper);
	gtk_widget_show_all(lock->widget);
	*widget = lock->widget;
	return lock;
}


/* lock_destroy */
static void _lock_destroy(Lock * lock)
{
	gtk_widget_destroy(lock->widget);
	free(lock);
}


/* lock_settings */
static GtkWidget * _lock_settings(Lock * lock, gboolean apply, gboolean reset)
{
	GtkWidget * hbox;
	GtkWidget * widget;
	char const * p;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p, %s, %s)\n", __func__, (void *)lock,
			apply ? "TRUE" : "FALSE", reset ? "TRUE" : "FALSE");
#endif
	if(lock->pr_box == NULL)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		lock->pr_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
		lock->pr_box = gtk_vbox_new(FALSE, 4);
		hbox = gtk_hbox_new(FALSE, 4);
#endif
		widget = gtk_label_new(_("Command:"));
		gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
		lock->pr_command = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(hbox), lock->pr_command, TRUE, TRUE,
				0);
		gtk_box_pack_start(GTK_BOX(lock->pr_box), hbox, FALSE, TRUE, 0);
		gtk_widget_show_all(lock->pr_box);
		reset = TRUE;
	}
	if(reset == TRUE)
	{
		if((p = lock->helper->config_get(lock->helper->panel, "lock",
						"command")) == NULL)
			p = "xset s activate";
		gtk_entry_set_text(GTK_ENTRY(lock->pr_command), p);
	}
	if(apply == TRUE)
	{
		p = gtk_entry_get_text(GTK_ENTRY(lock->pr_command));
		lock->helper->config_set(lock->helper->panel, "lock", "command",
				p);
	}
	return lock->pr_box;
}


/* callbacks */
/* lock_on_clicked */
static void _lock_on_clicked(gpointer data)
{
	PanelAppletHelper * helper = data;

	helper->lock(helper->panel);
}
