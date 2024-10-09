/* $Id$ */
/* Copyright (c) 2016-2023 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <gdk/gdk.h>
#if defined(GDK_WINDOWING_X11)
# include <gdk/gdkx.h>
# include <X11/XKBlib.h>
# include <X11/extensions/XKBfile.h>
#endif
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string


/* LEDs */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
#if defined(GDK_WINDOWING_X11)
	GtkWidget * widget;
	GtkWidget * leds[XkbNumIndicators];
	gulong source;
	guint timeout;

	GdkDisplay * display;
	XkbDescPtr xkb;
#endif
} LEDs;


/* prototypes */
/* leds */
static LEDs * _leds_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _leds_destroy(LEDs * leds);

/* callbacks */
#if defined(GDK_WINDOWING_X11)
static void _leds_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data);
static gboolean _leds_on_timeout(gpointer data);
#endif


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("LEDs"),
	GTK_STOCK_DIALOG_INFO,
	NULL,
	_leds_init,
	_leds_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* leds_init */
static LEDs * _leds_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
#if defined(GDK_WINDOWING_X11)
	LEDs * leds;
	size_t i;

	if((leds = object_new(sizeof(*leds))) == NULL)
		return NULL;
	leds->helper = helper;
#if GTK_CHECK_VERSION(3, 0, 0)
	leds->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	leds->widget = gtk_hbox_new(TRUE, 4);
#endif
	for(i = 0; i < XkbNumIndicators; i++)
	{
		leds->leds[i] = gtk_image_new();
		gtk_widget_set_no_show_all(leds->leds[i], TRUE);
		gtk_box_pack_start(GTK_BOX(leds->widget), leds->leds[i], FALSE,
				TRUE, 0);
	}
	leds->source = g_signal_connect(leds->widget, "screen-changed",
			G_CALLBACK(_leds_on_screen_changed), leds);
	leds->timeout = 0;
	leds->display = NULL;
	gtk_widget_show(leds->widget);
	*widget = leds->widget;
	return leds;
#else
	(void) helper;
	(void) widget;

	error_set_code(-ENOSYS, "X11 support not detected");
	return NULL;
#endif
}


/* leds_destroy */
static void _leds_destroy(LEDs * leds)
{
#if defined(GDK_WINDOWING_X11)
	/* XXX free xkb? */
	if(leds->timeout != 0)
		g_source_remove(leds->timeout);
	if(leds->source != 0)
		g_signal_handler_disconnect(leds->widget, leds->source);
	gtk_widget_destroy(leds->widget);
	object_delete(leds);
#else
	(void) leds;
#endif
}


#if defined(GDK_WINDOWING_X11)
/* callbacks */
/* leds_on_screen_changed */
static void _leds_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data)
{
	const unsigned int timeout = 500;
	LEDs * leds = data;
	PanelAppletHelper * helper = leds->helper;
	GdkScreen * screen;
	int major;
	int minor;
	char buf[64];
	int opcode;
	int event;
	int error;
	(void) previous;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(leds->xkb != NULL)
		/* XXX free xkb? */
		leds->xkb = NULL;
	if(leds->timeout != 0)
		g_source_remove(leds->timeout);
	leds->timeout = 0;
	screen = gtk_widget_get_screen(widget);
	leds->display = gdk_screen_get_display(screen);
	major = XkbMajorVersion;
	minor = XkbMinorVersion;
	if(XkbLibraryVersion(&major, &minor) == 0)
	{
		snprintf(buf, sizeof(buf), _("XKB library mismatch (%d.%d)"),
				major, minor);
		helper->error(NULL, buf, 0);
	}
	if(XkbQueryExtension(GDK_DISPLAY_XDISPLAY(leds->display), &opcode,
				&event, &error, &major, &minor) == 0)
	{
		snprintf(buf, sizeof(buf), _("XKB extension mismatch (%d.%d)"),
				major, minor);
		helper->error(NULL, buf, 1);
		return;
	}
	if((leds->xkb = XkbGetMap(GDK_DISPLAY_XDISPLAY(leds->display), 0,
					XkbUseCoreKbd)) == NULL)
	{
		snprintf(buf, sizeof(buf), "%s", _("Could not obtain XKB map"));
		helper->error(NULL, buf, 1);
		return;
	}
	if(XkbGetIndicatorMap(GDK_DISPLAY_XDISPLAY(leds->display),
				XkbAllIndicatorsMask, leds->xkb) != Success)
	{
		snprintf(buf, sizeof(buf), "%s",
				_("Could not obtain XKB indicator map"));
		helper->error(NULL, buf, 1);
		return;
	}
	if(XkbGetNames(GDK_DISPLAY_XDISPLAY(leds->display), XkbAllNamesMask,
				leds->xkb) != Success)
	{
		snprintf(buf, sizeof(buf), "%s",
				_("Could not obtain XKB names"));
		helper->error(NULL, buf, 1);
		return;
	}
	XkbSelectEvents(GDK_DISPLAY_XDISPLAY(leds->display), XkbUseCoreKbd,
			XkbIndicatorStateNotifyMask,
			XkbIndicatorStateNotifyMask);
	/* FIXME react on XKB events instead */
	if(_leds_on_timeout(leds) != FALSE)
		leds->source = g_timeout_add(timeout, _leds_on_timeout, leds);
}


/* leds_on_timeout */
static gboolean _leds_on_timeout(gpointer data)
{
	LEDs * leds = data;
	PanelAppletHelper * helper = leds->helper;
	GtkIconSize iconsize;
	unsigned int n;
	unsigned int i;
	unsigned int bit;
#if GTK_CHECK_VERSION(2, 12, 0)
	char buf[16];
	char const * text;
#endif

	iconsize = panel_window_get_icon_size(helper->window);
	XkbGetIndicatorState(GDK_DISPLAY_XDISPLAY(leds->display), XkbUseCoreKbd,
			&n);
	for(i = 0, bit = 1; i < XkbNumIndicators; i++, bit <<= 1)
	{
		if(leds->xkb->names->indicators[i] == None
				|| (leds->xkb->indicators->phys_indicators
					& bit) == 0)
		{
			gtk_widget_hide(leds->leds[i]);
			continue;
		}
#if GTK_CHECK_VERSION(3, 10, 0)
		gtk_image_set_from_icon_name(GTK_IMAGE(leds->leds[i]),
				applet.icon, iconsize);
#else
		gtk_image_set_from_stock(GTK_IMAGE(leds->leds[i]),
				GTK_STOCK_DIALOG_INFO, iconsize);
#endif
#if GTK_CHECK_VERSION(2, 12, 0)
		if((text = XkbAtomText(GDK_DISPLAY_XDISPLAY(leds->display),
						leds->xkb->names->indicators[i],
						XkbMessage)) == NULL)
		{
			snprintf(buf, sizeof(buf), _("LED %u"), i + 1);
			text = buf;
		}
		gtk_widget_set_tooltip_text(leds->leds[i], text);
#endif
		gtk_widget_set_sensitive(leds->leds[i], (n & bit)
				? TRUE : FALSE);
		gtk_widget_show(leds->leds[i]);
	}
	return TRUE;
}
#endif
