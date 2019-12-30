/* $Id$ */
/* Copyright (c) 2010-2018 Pierre Pronchery <khorben@defora.org> */
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


/* GSM */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * hbox;
	GtkWidget * image;
	guint timeout;
#if defined(__linux__)
	int fd;
#endif
} GSM;


/* prototypes */
static GSM * _gsm_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _gsm_destroy(GSM * gsm);

/* accessors */
static gboolean _gsm_get(GSM * gsm, gboolean * active);
static void _gsm_set(GSM * gsm, gboolean active);
#if 0
static void _gsm_set_operator(GSM * gsm, char const * operator);
#endif

/* callbacks */
static gboolean _gsm_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("GSM"),
	"phone",	/* XXX find a better image */
	NULL,
	_gsm_init,
	_gsm_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* gsm_init */
static GSM * _gsm_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const int timeout = 1000;
	GSM * gsm;

	if((gsm = malloc(sizeof(*gsm))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	gsm->helper = helper;
#if defined(__linux__)
	gsm->fd = -1;
#endif
#if GTK_CHECK_VERSION(3, 0, 0)
	gsm->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	gsm->hbox = gtk_hbox_new(FALSE, 0);
#endif
	gsm->image = gtk_image_new_from_icon_name(applet.icon,
			panel_window_get_icon_size(helper->window));
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(gsm->image, _("GSM is enabled"));
#endif
	gtk_widget_show(gsm->image);
	gtk_box_pack_start(GTK_BOX(gsm->hbox), gsm->image, FALSE, TRUE, 0);
	gsm->timeout = (_gsm_on_timeout(gsm) == TRUE)
		? g_timeout_add(timeout, _gsm_on_timeout, gsm) : 0;
	gtk_widget_set_no_show_all(gsm->hbox, TRUE);
	*widget = gsm->hbox;
	return gsm;
}


/* gsm_destroy */
static void _gsm_destroy(GSM * gsm)
{
	if(gsm->timeout > 0)
		g_source_remove(gsm->timeout);
#if defined(__linux__)
	if(gsm->fd != -1)
		close(gsm->fd);
#endif
	gtk_widget_destroy(gsm->hbox);
	free(gsm);
}


/* accessors */
/* gsm_get */
static gboolean _gsm_get(GSM * gsm, gboolean * active)
{
#if defined(__linux__)
	/* XXX currently hard-coded for the Openmoko Freerunner */
	char const dv1[] = "/sys/bus/platform/devices/gta02-pm-gsm.0/"
		"power_on";
	char const dv2[] = "/sys/bus/platform/devices/neo1973-pm-gsm.0/"
		"power_on";
	char const * dev = dv1;
	char on;

	if(gsm->fd < 0)
	{
		if((gsm->fd = open(dev, O_RDONLY)) < 0)
		{
			dev = dv2;
			gsm->fd = open(dev, O_RDONLY);
		}
		if(gsm->fd < 0)
		{
			error_set("%s: %s: %s", applet.name, dev,
					strerror(errno));
			*active = FALSE;
			return TRUE;
		}
	}
	errno = ENODATA; /* in case the pseudo-file is empty */
	if(lseek(gsm->fd, 0, SEEK_SET) != 0
			|| read(gsm->fd, &on, sizeof(on)) != 1)
	{
		error_set("%s: %s: %s", applet.name, dev, strerror(errno));
		close(gsm->fd);
		gsm->fd = -1;
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


/* gsm_set */
static void _gsm_set(GSM * gsm, gboolean active)
{
	if(active == TRUE)
		gtk_widget_show(gsm->hbox);
	else
		gtk_widget_hide(gsm->hbox);
}


#if 0
/* gsm_set_operator */
static void _gsm_set_operator(GSM * gsm, char const * operator)
{
	gtk_label_set_text(GTK_LABEL(gsm->operator), operator);
}
#endif


/* callbacks */
/* gsm_on_timeout */
static gboolean _gsm_on_timeout(gpointer data)
{
	GSM * gsm = data;
	gboolean active;

	if(_gsm_get(gsm, &active) == FALSE)
	{
		gsm->helper->error(NULL, error_get(NULL), 1);
		_gsm_set(gsm, FALSE);
		gsm->timeout = 0;
		return FALSE;
	}
	_gsm_set(gsm, active);
	return TRUE;
}
