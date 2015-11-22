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



#if defined(__NetBSD__) || defined(__linux__)
# include <sys/types.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <unistd.h>
# include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* USB */
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
} USB;


/* prototypes */
static USB * _usb_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _usb_destroy(USB * usb);

/* accessors */
static gboolean _usb_get(USB * usb, gboolean * active);
static void _usb_set(USB * usb, gboolean active);

/* callbacks */
static gboolean _usb_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"USB",
	"panel-applet-usb",
	NULL,
	_usb_init,
	_usb_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* usb_init */
static USB * _usb_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const int timeout = 1000;
	USB * usb;
#if GTK_CHECK_VERSION(2, 12, 0)
	char const * tooltip = NULL;
#endif

	if((usb = malloc(sizeof(*usb))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	usb->helper = helper;
#if defined(__NetBSD__) || defined(__linux__)
	usb->fd = -1;
	tooltip = _("USB networking device connected");
#endif
	usb->image = gtk_image_new_from_icon_name("panel-applet-usb",
			panel_window_get_icon_size(helper->window));
#if GTK_CHECK_VERSION(2, 12, 0)
	if(tooltip != NULL)
		gtk_widget_set_tooltip_text(usb->image, tooltip);
#endif
	usb->timeout = (_usb_on_timeout(usb) == TRUE)
		? g_timeout_add(timeout, _usb_on_timeout, usb) : 0;
	gtk_widget_set_no_show_all(usb->image, TRUE);
	*widget = usb->image;
	return usb;
}


/* usb_destroy */
static void _usb_destroy(USB * usb)
{
	if(usb->timeout > 0)
		g_source_remove(usb->timeout);
#if defined(__NetBSD__) || defined(__linux__)
	if(usb->fd >= 0)
		close(usb->fd);
#endif
	gtk_widget_destroy(usb->image);
	free(usb);
}


/* accessors */
/* usb_get */
static gboolean _usb_get(USB * usb, gboolean * active)
{
#if defined(__NetBSD__) || defined(__linux__)
	struct ifreq ifr;
# if defined(__NetBSD__)
	const char name[] = "cdce0";
# elif defined(__linux__)
	const char name[] = "usb0";
#endif

	if(usb->fd < 0 && (usb->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		error_set("%s: %s: %s", applet.name, "socket", strerror(errno));
		*active = FALSE;
		return TRUE;
	}
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);
	if(ioctl(usb->fd, SIOCGIFFLAGS, &ifr) == -1)
	{
		error_set("%s: %s: %s", applet.name, name, strerror(errno));
		close(usb->fd);
		usb->fd = -1;
		*active = FALSE;
		return TRUE;
	}
	close(usb->fd);
	usb->fd = -1;
	*active = TRUE;
	return TRUE;
#else
	/* FIXME not supported */
	*active = FALSE;
	error_set("%s: %s", applet.name, strerror(ENOSYS));
	return FALSE;
#endif
}


/* usb_set */
static void _usb_set(USB * usb, gboolean active)
{
	if(active == TRUE)
		gtk_widget_show(usb->image);
	else
		gtk_widget_hide(usb->image);
}


/* callbacks */
/* usb_on_timeout */
static gboolean _usb_on_timeout(gpointer data)
{
	USB * usb = data;
	gboolean active;

	if(_usb_get(usb, &active) == FALSE)
	{
		usb->helper->error(NULL, error_get(NULL), 1);
		_usb_set(usb, FALSE);
		usb->timeout = 0;
		return FALSE;
	}
	_usb_set(usb, active);
	return TRUE;
}
