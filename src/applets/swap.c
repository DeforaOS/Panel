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



#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined(__linux__)
# include <sys/sysinfo.h>
#elif defined(__NetBSD__)
# include <sys/sysctl.h>
# include <uvm/uvm_extern.h>
#endif
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* Swap */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
	GtkWidget * scale;
	guint timeout;
} Swap;


/* prototypes */
static Swap * _swap_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _swap_destroy(Swap * swap);

/* callbacks */
static gboolean _swap_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Swap",
	"gnome-monitor",
	NULL,
	_swap_init,
	_swap_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* swap_init */
static Swap * _swap_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const int timeout = 5000;
	Swap * swap;
	PangoFontDescription * desc;
	GtkWidget * label;

	if((swap = malloc(sizeof(*swap))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	swap->helper = helper;
#if GTK_CHECK_VERSION(3, 0, 0)
	swap->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	swap->widget = gtk_hbox_new(FALSE, 0);
#endif
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	label = gtk_label_new(_("Swap:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(label, desc);
#else
	gtk_widget_modify_font(label, desc);
#endif
	gtk_box_pack_start(GTK_BOX(swap->widget), label, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	swap->scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100,
			1);
#else
	swap->scale = gtk_vscale_new_with_range(0, 100, 1);
#endif
	gtk_widget_set_sensitive(swap->scale, FALSE);
	gtk_range_set_inverted(GTK_RANGE(swap->scale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(swap->scale), GTK_POS_RIGHT);
	gtk_box_pack_start(GTK_BOX(swap->widget), swap->scale, FALSE, FALSE, 0);
	if(_swap_on_timeout(swap) == TRUE)
		swap->timeout = g_timeout_add(timeout, _swap_on_timeout, swap);
	pango_font_description_free(desc);
	gtk_widget_show_all(swap->widget);
	*widget = swap->widget;
	return swap;
}


/* swap_destroy */
static void _swap_destroy(Swap * swap)
{
	g_source_remove(swap->timeout);
	gtk_widget_destroy(swap->widget);
	free(swap);
}


/* callbacks */
/* swap_on_timeout */
static gboolean _swap_on_timeout(gpointer data)
{
#if defined(__linux__)
	Swap * swap = data;
	struct sysinfo sy;
	gdouble value;

	if(sysinfo(&sy) != 0)
		return swap->helper->error(swap->helper->panel, "sysinfo",
				TRUE);
	if((value = sy.totalswap - sy.freeswap) != 0.0 && sy.totalswap != 0)
		value /= sy.totalswap;
	gtk_range_set_value(GTK_RANGE(swap->scale), value);
	return TRUE;
#elif defined(__NetBSD__)
	Swap * swap = data;
	int mib[] = { CTL_VM, VM_UVMEXP };
	struct uvmexp ue;
	size_t size = sizeof(ue);
	gdouble value;

	if(sysctl(mib, 2, &ue, &size, NULL, 0) < 0)
		return TRUE;
	if((value = ue.swpgonly) != 0.0 && ue.swpages != 0)
		value /= ue.swpages;
	gtk_range_set_value(GTK_RANGE(swap->scale), value);
	return TRUE;
#else
	Swap * swap = data;

	/* FIXME not supported */
	swap->source = 0;
	return FALSE;
#endif
}
