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



#if defined(__linux__)
# include <fcntl.h>
# include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string


/* GPS */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * image;
	guint timeout;
#if defined(__linux__)
	int fd;
#endif
} GPS;


/* prototypes */
static GPS * _gps_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _gps_destroy(GPS * gps);

/* accessors */
static gboolean _gps_get(GPS * gps, gboolean * active);
static void _gps_set(GPS * gps, gboolean active);

/* callbacks */
static gboolean _gps_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("GPS"),
	"network-wireless", /* XXX find a better image */
	NULL,
	_gps_init,
	_gps_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* gps_init */
static GPS * _gps_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const int timeout = 1000;
	GPS * gps;

	if((gps = malloc(sizeof(*gps))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	gps->helper = helper;
#if defined(__linux__)
	gps->fd = -1;
#endif
	gps->image = gtk_image_new_from_icon_name(applet.icon,
			panel_window_get_icon_size(helper->window));
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(gps->image, _("GPS is enabled"));
#endif
	gps->timeout = (_gps_on_timeout(gps) == TRUE)
		? g_timeout_add(timeout, _gps_on_timeout, gps) : 0;
	gtk_widget_set_no_show_all(gps->image, TRUE);
	*widget = gps->image;
	return gps;
}


/* gps_destroy */
static void _gps_destroy(GPS * gps)
{
	if(gps->timeout > 0)
		g_source_remove(gps->timeout);
#if defined(__linux__)
	if(gps->fd != -1)
		close(gps->fd);
#endif
	gtk_widget_destroy(gps->image);
	free(gps);
}


/* accessors */
/* gps_get */
static gboolean _gps_get(GPS * gps, gboolean * active)
{
#if defined(__linux__)
	/* XXX currently hard-coded for the Openmoko Freerunner */
	char const p1[] = "/sys/bus/platform/devices/gta02-pm-gps.0/power_on";
	char const p2[] = "/sys/bus/platform/devices/neo1973-pm-gps.0/power_on";
	char const p3[] = "/sys/bus/platform/drivers/neo1973-pm-gps/"
		"neo1973-pm-gps.0/pwron";
	char on;

	if(gps->fd < 0 && (gps->fd = open(p1, O_RDONLY)) < 0
			&& (gps->fd = open(p2, O_RDONLY)) < 0
			&& (gps->fd = open(p3, O_RDONLY)) < 0)
	{
		error_set("%s: %s: %s", applet.name, p1, strerror(errno));
		*active = FALSE;
		return TRUE;
	}
	errno = ENODATA; /* in case the pseudo-file is empty */
	if(lseek(gps->fd, 0, SEEK_SET) != 0
			|| read(gps->fd, &on, sizeof(on)) != 1)
	{
		error_set("%s: %s: %s", applet.name, p1, strerror(errno));
		close(gps->fd);
		gps->fd = -1;
		*active = FALSE;
		return TRUE;
	}
	*active = (on == '1') ? TRUE : FALSE;
	return TRUE;
#else
	/* FIXME not supported */
	*active = FALSE;
	error_set("%s: %s", applet.name, strerror(ENOSYS));
	return FALSE;
#endif
}


/* gps_set */
static void _gps_set(GPS * gps, gboolean active)
{
	if(active == TRUE)
		gtk_widget_show(gps->image);
	else
		gtk_widget_hide(gps->image);
}


/* callbacks */
/* gps_on_timeout */
static gboolean _gps_on_timeout(gpointer data)
{
	GPS * gps = data;
	gboolean active;

	if(_gps_get(gps, &active) == FALSE)
	{
		gps->helper->error(NULL, error_get(NULL), 1);
		_gps_set(gps, FALSE);
		gps->timeout = 0;
		return FALSE;
	}
	_gps_set(gps, active);
	return TRUE;
}
