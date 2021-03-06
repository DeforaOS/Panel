/* $Id$ */
/* Copyright (c) 2015-2020 Pierre Pronchery <khorben@defora.org> */
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
# include <sys/param.h>
# include <sys/sysctl.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string


/* Brightness */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;

	/* widgets */
	GtkWidget * box;
	GtkWidget * image;
	GtkWidget * label;
	GtkWidget * progress;
	guint timeout;
} Brightness;


/* prototypes */
static Brightness * _brightness_init(PanelAppletHelper * helper,
		GtkWidget ** widget);
static void _brightness_destroy(Brightness * brightness);

static gboolean _brightness_get(Brightness * brightness, int * level);
static void _brightness_set(Brightness * brightness, int level);

/* callbacks */
static gboolean _brightness_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Brightness"),
	"video-display",
	NULL,
	_brightness_init,
	_brightness_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* brightness_init */
static Brightness * _brightness_init(PanelAppletHelper * helper,
		GtkWidget ** widget)
{
	const int timeout = 1000;
	Brightness * brightness;
	GtkIconSize iconsize;
	GtkWidget * vbox;
	GtkWidget * hbox;
	PangoFontDescription * bold;

	if((brightness = malloc(sizeof(*brightness))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	brightness->helper = helper;
	brightness->timeout = 0;
	iconsize = panel_window_get_icon_size(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	brightness->box = hbox;
	brightness->image = gtk_image_new_from_icon_name(applet.icon, iconsize);
	gtk_box_pack_start(GTK_BOX(hbox), brightness->image, TRUE, TRUE, 0);
	brightness->label = NULL;
	brightness->progress = NULL;
	if(panel_window_get_type(helper->window)
			== PANEL_WINDOW_TYPE_NOTIFICATION)
	{
		bold = pango_font_description_new();
		pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
#if GTK_CHECK_VERSION(3, 0, 0)
		vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
		vbox = gtk_vbox_new(FALSE, 4);
#endif
		gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
		gtk_widget_show(hbox);
		brightness->progress = gtk_progress_bar_new();
		gtk_box_pack_start(GTK_BOX(vbox), brightness->progress, TRUE,
				TRUE, 0);
		brightness->box = vbox;
		pango_font_description_free(bold);
	}
	else
	{
#ifndef EMBEDDED
		brightness->label = gtk_label_new(" ");
		gtk_box_pack_start(GTK_BOX(hbox), brightness->label, FALSE,
				TRUE, 0);
		gtk_widget_show(brightness->label);
#endif
		brightness->box = hbox;
	}
	brightness->timeout = g_timeout_add(timeout, _brightness_on_timeout,
			brightness);
	_brightness_on_timeout(brightness);
	gtk_widget_show(brightness->image);
	*widget = brightness->box;
	return brightness;
}


/* brightness_destroy */
static void _brightness_destroy(Brightness * brightness)
{
	if(brightness->timeout > 0)
		g_source_remove(brightness->timeout);
	gtk_widget_destroy(brightness->box);
	free(brightness);
}


/* brightness_set */
static void _brightness_set(Brightness * brightness, int level)
{
	char buf[64];
	double fraction;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%d)\n", __func__, level);
#endif
	/* XXX only show when necessary? */
	if(level >= 0 && level <= 100)
	{
#if GTK_CHECK_VERSION(2, 12, 0)
		snprintf(buf, sizeof(buf), _("Brightness: %d%%"), level);
		gtk_widget_set_tooltip_text(brightness->box, buf);
#endif
		snprintf(buf, sizeof(buf), "%d%% ", level);
		if(brightness->progress != NULL)
		{
			fraction = level;
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(
						brightness->progress),
					fraction / 100.0);
			gtk_widget_show(brightness->progress);
		}
	}
	else
	{
		/* the brightness is invalid */
#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_widget_set_tooltip_text(brightness->box, NULL);
#endif
		gtk_widget_hide(brightness->box);
		if(level < 0)
			snprintf(buf, sizeof(buf), "%s", _("Error"));
		else if(level > 100)
			snprintf(buf, sizeof(buf), "%s", _("Unknown"));
	}
	if(brightness->label != NULL)
		gtk_label_set_text(GTK_LABEL(brightness->label), buf);
	if(brightness->progress != NULL)
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(
					brightness->progress), buf);
	gtk_widget_show(brightness->box);
}


/* callbacks */
/* brightness_on_timeout */
static gboolean _brightness_get(Brightness * brightness, int * level)
{
#if defined(__NetBSD__)
	PanelAppletHelper * helper = brightness->helper;
	char const sysctl[] = "hw.acpi.acpiout0.brightness";
	size_t s;

	*level = -1;
	if(sysctlbyname(sysctl, level, &s, NULL, 0) != 0)
	{
		error_set("%s: %s: %s", applet.name, "sysctl", strerror(errno));
		helper->error(NULL, strerror(errno), 1);
	}
	return TRUE;
#else
	PanelAppletHelper * helper = brightness->helper;

	/* FIXME not supported */
	*level = -1;
	error_set("%s: %s", applet.name, strerror(ENOSYS));
	helper->error(NULL, error_get(NULL), 1);
	return FALSE;
#endif
}


/* callbacks */
/* brightness_on_timeout */
static gboolean _brightness_on_timeout(gpointer data)
{
	Brightness * brightness = data;
	int level = -1;
	int timeout;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(_brightness_get(brightness, &level) == FALSE)
	{
		brightness->timeout = 0;
		return FALSE;
	}
	if(level >= 0)
	{
		_brightness_set(brightness, level);
		timeout = 1000;
	}
	else
		timeout = 10000;
	brightness->timeout = g_timeout_add(timeout, _brightness_on_timeout,
			brightness);
	return FALSE;
}
