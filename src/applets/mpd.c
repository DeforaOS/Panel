/* $Id$ */
/* Copyright (c) 2014 Pierre Pronchery <khorben@defora.org> */
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



#include <libintl.h>
#include <System.h>
#include <Desktop.h>
#include "Panel.h"
#define _(string) gettext(string)

/* constants */
#ifndef MPD_COMMAND_FORWARD
# define MPD_COMMAND_FORWARD	"mpd --forward"
#endif
#ifndef MPD_COMMAND_NEXT
# define MPD_COMMAND_NEXT	"mpd --next"
#endif
#ifndef MPD_COMMAND_PAUSE
# define MPD_COMMAND_PAUSE	"mpd --play-pause"
#endif
#ifndef MPD_COMMAND_PLAY
# define MPD_COMMAND_PLAY	"mpd --play"
#endif
#ifndef MPD_COMMAND_PREVIOUS
# define MPD_COMMAND_PREVIOUS	"mpd --previous"
#endif
#ifndef MPD_COMMAND_REWIND
# define MPD_COMMAND_REWIND	"mpd --rewind"
#endif
#ifndef MPD_COMMAND_STOP
# define MPD_COMMAND_STOP	"mpd --stop"
#endif


/* MPD */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
} MPD;


/* prototypes */
static MPD * _mpd_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _mpd_destroy(MPD * mpd);

/* useful */
static void _mpd_exec(MPD * mpd, char const * command);

/* callbacks */
static void _mpd_on_forward(gpointer data);
static void _mpd_on_next(gpointer data);
static void _mpd_on_pause(gpointer data);
static void _mpd_on_play(gpointer data);
static void _mpd_on_previous(gpointer data);
static void _mpd_on_rewind(gpointer data);
static void _mpd_on_stop(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"MPD",
	"multimedia",
	NULL,
	_mpd_init,
	_mpd_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* mpd_init */
static void _init_add(MPD * mpd, char const * stock, GtkIconSize iconsize,
		char const * tooltip, GCallback callback);

static MPD * _mpd_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	MPD * mpd;

	if((mpd = object_new(sizeof(*mpd))) == NULL)
	{
		helper->error(NULL, error_get(), 1);
		return NULL;
	}
	mpd->helper = helper;
	/* FIXME choose the adequate orientation */
	mpd->widget = gtk_hbox_new(FALSE, 0);
	_init_add(mpd, GTK_STOCK_MEDIA_PREVIOUS, helper->icon_size,
			_("Previous"), G_CALLBACK(_mpd_on_previous));
	_init_add(mpd, GTK_STOCK_MEDIA_REWIND, helper->icon_size, _("Rewind"),
			G_CALLBACK(_mpd_on_rewind));
	_init_add(mpd, GTK_STOCK_MEDIA_PLAY, helper->icon_size, _("Play"),
			G_CALLBACK(_mpd_on_play));
	_init_add(mpd, GTK_STOCK_MEDIA_PAUSE, helper->icon_size, _("Pause"),
			G_CALLBACK(_mpd_on_pause));
	_init_add(mpd, GTK_STOCK_MEDIA_STOP, helper->icon_size, _("Stop"),
			G_CALLBACK(_mpd_on_stop));
	_init_add(mpd, GTK_STOCK_MEDIA_FORWARD, helper->icon_size, _("Forward"),
			G_CALLBACK(_mpd_on_forward));
	_init_add(mpd, GTK_STOCK_MEDIA_NEXT, helper->icon_size, _("Next"),
			G_CALLBACK(_mpd_on_next));
	gtk_widget_show_all(mpd->widget);
	*widget = mpd->widget;
	return mpd;
}

static void _init_add(MPD * mpd, char const * stock, GtkIconSize iconsize,
		char const * tooltip, GCallback callback)
{
	GtkWidget * button;
	GtkWidget * image;

	button = gtk_button_new();
	image = gtk_image_new_from_stock(stock, iconsize);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(button, tooltip);
#endif
	g_signal_connect_swapped(button, "clicked", callback, mpd);
	gtk_box_pack_start(GTK_BOX(mpd->widget), button, FALSE, TRUE, 0);
}


/* mpd_destroy */
static void _mpd_destroy(MPD * mpd)
{
	gtk_widget_destroy(mpd->widget);
	object_delete(mpd);
}


/* useful */
/* mpd_exec */
static void _mpd_exec(MPD * mpd, char const * command)
{
	GError * error = NULL;

	if(g_spawn_command_line_async(command, &error) == FALSE)
	{
		mpd->helper->error(NULL, error->message, 1);
		g_error_free(error);
	}
}


/* callbacks */
/* mpd_on_forward */
static void _mpd_on_forward(gpointer data)
{
	MPD * mpd = data;

	_mpd_exec(mpd, MPD_COMMAND_FORWARD);
}


/* mpd_on_next */
static void _mpd_on_next(gpointer data)
{
	MPD * mpd = data;

	_mpd_exec(mpd, MPD_COMMAND_NEXT);
}


/* mpd_on_pause */
static void _mpd_on_pause(gpointer data)
{
	MPD * mpd = data;

	_mpd_exec(mpd, MPD_COMMAND_PAUSE);
}


/* mpd_on_play */
static void _mpd_on_play(gpointer data)
{
	MPD * mpd = data;

	_mpd_exec(mpd, MPD_COMMAND_PLAY);
}


/* mpd_on_previous */
static void _mpd_on_previous(gpointer data)
{
	MPD * mpd = data;

	_mpd_exec(mpd, MPD_COMMAND_PREVIOUS);
}


/* mpd_on_rewind */
static void _mpd_on_rewind(gpointer data)
{
	MPD * mpd = data;

	_mpd_exec(mpd, MPD_COMMAND_REWIND);
}


/* mpd_on_stop */
static void _mpd_on_stop(gpointer data)
{
	MPD * mpd = data;

	_mpd_exec(mpd, MPD_COMMAND_STOP);
}
