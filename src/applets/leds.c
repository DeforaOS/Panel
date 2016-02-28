/* $Id$ */
/* Copyright (c) 2016 Pierre Pronchery <khorben@defora.org> */
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
#include <gdk/gdkx.h>
#include <X11/XKBlib.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* LEDs */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
	GtkWidget * leds[XkbNumIndicators];
	gulong source;

	GdkDisplay * display;
} LEDs;


/* prototypes */
/* leds */
static LEDs * _leds_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _leds_destroy(LEDs * leds);

/* callbacks */
static void _leds_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"LEDs",
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
		leds->leds[i] = NULL;
	leds->source = g_signal_connect(leds->widget, "screen-changed",
			G_CALLBACK(_leds_on_screen_changed), leds);
	leds->display = NULL;
	gtk_widget_show(leds->widget);
	*widget = leds->widget;
	return leds;
}


/* leds_destroy */
static void _leds_destroy(LEDs * leds)
{
	if(leds->source != 0)
		g_signal_handler_disconnect(leds->widget, leds->source);
	leds->source = 0;
	gtk_widget_destroy(leds->widget);
	object_delete(leds);
}


/* callbacks */
/* leds_on_screen_changed */
static void _leds_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data)
{
	LEDs * leds = data;
	PanelAppletHelper * helper = leds->helper;
	GdkScreen * screen;
	int major;
	int minor;
	char buf[64];
	int opcode;
	int event;
	int error;
	XkbDescPtr xkb;
	unsigned int i;
	unsigned int bit;
	unsigned int n;
	GtkIconSize iconsize;
	(void) previous;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
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
	if((xkb = XkbGetMap(GDK_DISPLAY_XDISPLAY(leds->display), 0,
					XkbUseCoreKbd)) == NULL)
	{
		snprintf(buf, sizeof(buf), "%s", _("Could not obtain XKB map"));
		helper->error(NULL, buf, 1);
		return;
	}
	/* XXX free xkb when returning? */
	if(XkbGetIndicatorMap(GDK_DISPLAY_XDISPLAY(leds->display),
				XkbAllIndicatorsMask, xkb) != Success)
	{
		snprintf(buf, sizeof(buf), "%s",
				_("Could not obtain XKB indicator map"));
		helper->error(NULL, buf, 1);
		return;
	}
	if(XkbGetNames(GDK_DISPLAY_XDISPLAY(leds->display), XkbAllNamesMask,
				xkb) != Success)
	{
		snprintf(buf, sizeof(buf), "%s",
				_("Could not obtain XKB names"));
		helper->error(NULL, buf, 1);
		return;
	}
	XkbSelectEvents(GDK_DISPLAY_XDISPLAY(leds->display), XkbUseCoreKbd,
			XkbIndicatorStateNotifyMask,
			XkbIndicatorStateNotifyMask);
	XkbGetIndicatorState(GDK_DISPLAY_XDISPLAY(leds->display), XkbUseCoreKbd,
			&n);
	iconsize = panel_window_get_icon_size(helper->window);
	for(i = 0, bit = 1; i < XkbNumIndicators; i++, bit <<= 1)
	{
		if(xkb->names->indicators[i] == None)
			continue;
		if((xkb->indicators->phys_indicators & bit) == 0)
			continue;
#if GTK_CHECK_VERSION(3, 10, 0)
		widget = gtk_image_new_from_icon_name(GTK_STOCK_DIALOG_INFO,
				iconsize);
#else
		widget = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
				iconsize);
#endif
		leds->leds[i] = widget;
#if GTK_CHECK_VERSION(2, 12, 0)
		snprintf(buf, sizeof(buf), _("LED %u"), i + 1);
		gtk_widget_set_tooltip_text(widget, buf);
#endif
		gtk_widget_set_sensitive(widget, (n & bit) ? TRUE : FALSE);
		gtk_box_pack_start(GTK_BOX(leds->widget), widget, FALSE, TRUE,
				0);
	}
}
