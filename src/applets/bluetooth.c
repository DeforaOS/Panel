/* $Id$ */
/* Copyright (c) 2010-2015 Pierre Pronchery <khorben@defora.org> */
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



#if defined(__NetBSD__)
# include <sys/types.h>
# include <sys/ioctl.h>
# include <bluetooth.h>
# include <unistd.h>
# include <stdio.h>
# include <string.h>
# include <errno.h>
#elif defined(__linux__)
# include <fcntl.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
#endif
#include <stdlib.h>
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* Bluetooth */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * image;
	guint timeout;
#if defined(__NetBSD__) || defined(__linux__)
	int fd;
#endif
} Bluetooth;


/* prototypes */
static Bluetooth * _bluetooth_init(PanelAppletHelper * helper,
		GtkWidget ** widget);
static void _bluetooth_destroy(Bluetooth * bluetooth);

/* accessors */
static gboolean _bluetooth_get(Bluetooth * bluetooth, gboolean * active);
static void _bluetooth_set(Bluetooth * bluetooth, gboolean active);

/* callbacks */
static gboolean _bluetooth_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Bluetooth",
	"panel-applet-bluetooth",
	NULL,
	_bluetooth_init,
	_bluetooth_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* bluetooth_init */
static Bluetooth * _bluetooth_init(PanelAppletHelper * helper,
		GtkWidget ** widget)
{
	const int timeout = 1000;
	Bluetooth * bluetooth;
	GtkIconSize iconsize;

	if((bluetooth = object_new(sizeof(*bluetooth))) == NULL)
		return NULL;
	bluetooth->helper = helper;
#if defined(__NetBSD__) || defined(__linux__)
	bluetooth->fd = -1;
#endif
	iconsize = panel_window_get_icon_size(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	*widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	*widget = gtk_hbox_new(FALSE, 0);
#endif
	bluetooth->image = gtk_image_new_from_icon_name(
			"panel-applet-bluetooth", iconsize);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(bluetooth->image,
			_("Bluetooth is enabled"));
#endif
	gtk_box_pack_start(GTK_BOX(*widget), bluetooth->image, TRUE, TRUE, 0);
	gtk_widget_set_no_show_all(*widget, TRUE);
	bluetooth->timeout = (_bluetooth_on_timeout(bluetooth) == TRUE)
		? g_timeout_add(timeout, _bluetooth_on_timeout, bluetooth) : 0;
	return bluetooth;
}


/* bluetooth_destroy */
static void _bluetooth_destroy(Bluetooth * bluetooth)
{
	if(bluetooth->timeout > 0)
		g_source_remove(bluetooth->timeout);
#if defined(__NetBSD__) || defined(__linux__)
	if(bluetooth->fd >= 0)
		close(bluetooth->fd);
#endif
	gtk_widget_destroy(bluetooth->image);
	object_delete(bluetooth);
}


/* accessors */
/* bluetooth_get */
static gboolean _bluetooth_get(Bluetooth * bluetooth, gboolean * active)
{
#if defined(__NetBSD__)
	struct btreq btr;
	const char name[] = "ubt0";

	if(bluetooth->fd < 0 && (bluetooth->fd = socket(AF_BLUETOOTH,
					SOCK_RAW, BTPROTO_HCI)) < 0)
	{
		*active = FALSE;
		error_set("%s: %s: %s", applet.name, "socket", strerror(errno));
		return TRUE;
	}
	memset(&btr, 0, sizeof(btr));
	strncpy(btr.btr_name, name, sizeof(name));
	if(ioctl(bluetooth->fd, SIOCGBTINFO, &btr) == -1)
	{
		*active = FALSE;
		error_set("%s: %s: %s", applet.name, name, strerror(errno));
		close(bluetooth->fd);
		bluetooth->fd = -1;
	}
	else
	{
		*active = TRUE;
		/* XXX should not be needed but EBADF happens once otherwise */
		close(bluetooth->fd);
		bluetooth->fd = -1;
	}
	return TRUE;
#elif defined(__linux__)
	/* XXX currently hard-coded for the Openmoko Freerunner */
	char const dv1[] = "/sys/bus/platform/devices/gta02-pm-bt.0/power_on";
	char const dv2[] = "/sys/bus/platform/devices/neo1973-pm-bt.0/power_on";
	char const * dev = dv1;
	char on;

	if(bluetooth->fd < 0)
	{
		if((bluetooth->fd = open(dev, O_RDONLY)) < 0)
		{
			dev = dv2;
			bluetooth->fd = open(dev, O_RDONLY);
		}
		if(bluetooth->fd < 0)
		{
			*active = FALSE;
			error_set("%s: %s: %s", applet.name, dev,
					strerror(errno));
			return TRUE;
		}
	}
	errno = ENODATA; /* in case the pseudo-file is empty */
	if(lseek(bluetooth->fd, 0, SEEK_SET) != 0
			|| read(bluetooth->fd, &on, sizeof(on)) != sizeof(on))
	{
		*active = FALSE;
		error_set("%s: %s: %s", applet.name, dev, strerror(errno));
		close(bluetooth->fd);
		bluetooth->fd = -1;
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


/* bluetooth_set */
static void _bluetooth_set(Bluetooth * bluetooth, gboolean active)
{
	if(active == TRUE)
		gtk_widget_show(bluetooth->image);
	else
		gtk_widget_hide(bluetooth->image);
}


/* callbacks */
/* bluetooth_on_timeout */
static gboolean _bluetooth_on_timeout(gpointer data)
{
	Bluetooth * bluetooth = data;
	gboolean active;

	if(_bluetooth_get(bluetooth, &active) == FALSE)
	{
		bluetooth->helper->error(NULL, error_get(NULL), 1);
		_bluetooth_set(bluetooth, FALSE);
		bluetooth->timeout = 0;
		return FALSE;
	}
	_bluetooth_set(bluetooth, active);
	return TRUE;
}
