/* $Id$ */
/* Copyright (c) 2010-2022 Pierre Pronchery <khorben@defora.org> */
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
#if defined(__APPLE__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/vmmeter.h>
#elif defined(__FreeBSD__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/vmmeter.h>
# include <vm/vm_param.h>
#elif defined(__linux__)
# include <sys/sysinfo.h>
#elif defined(__NetBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
# include <uvm/uvm_extern.h>
#endif
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string


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

/* accessors */
static void _swap_set(Swap * swap, gdouble level);

/* callbacks */
static gboolean _swap_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Swap"),
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
	GtkOrientation orientation;
	PangoFontDescription * desc;
	GtkWidget * label;

	if((swap = malloc(sizeof(*swap))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	swap->helper = helper;
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	swap->widget = gtk_box_new(orientation, 0);
#else
	swap->widget = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? gtk_hbox_new(FALSE, 0) : gtk_vbox_new(FALSE, 0);
#endif
	/* invert the orientation for the meter */
	orientation = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	label = gtk_label_new(_("Swap:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(label, desc);
#else
	gtk_widget_modify_font(label, desc);
#endif
	gtk_box_pack_start(GTK_BOX(swap->widget), label, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(3, 6, 0)
	swap->scale = gtk_level_bar_new_for_interval(0.0, 100.0);
# if GTK_CHECK_VERSION(3, 8, 0)
	gtk_level_bar_set_inverted(GTK_LEVEL_BAR(swap->scale), TRUE);
# endif
	gtk_orientable_set_orientation(GTK_ORIENTABLE(swap->scale),
			orientation);
#else
# if GTK_CHECK_VERSION(3, 0, 0)
	swap->scale = gtk_scale_new_with_range(orientation, 0, 100, 1);
# else
	swap->scale = gtk_vscale_new_with_range(0, 100, 1);
# endif
	gtk_widget_set_sensitive(swap->scale, FALSE);
	gtk_range_set_inverted(GTK_RANGE(swap->scale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(swap->scale), GTK_POS_RIGHT);
#endif
	gtk_box_pack_start(GTK_BOX(swap->widget), swap->scale, FALSE, FALSE, 0);
	swap->timeout = (_swap_on_timeout(swap) == TRUE)
		? g_timeout_add(timeout, _swap_on_timeout, swap) : 0;
	pango_font_description_free(desc);
	gtk_widget_show_all(swap->widget);
	*widget = swap->widget;
	return swap;
}


/* swap_destroy */
static void _swap_destroy(Swap * swap)
{
	if(swap->timeout != 0)
		g_source_remove(swap->timeout);
	gtk_widget_destroy(swap->widget);
	free(swap);
}

/* accessors */
static void _swap_set(Swap * swap, gdouble level)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%f)\n", __func__, level);
#endif
#if GTK_CHECK_VERSION(3, 8, 0)
	gtk_level_bar_set_value(GTK_LEVEL_BAR(swap->scale), level);
#elif GTK_CHECK_VERSION(3, 6, 0)
	/* invert the value ourselves */
	gtk_level_bar_set_value(GTK_LEVEL_BAR(swap->scale), 100.0 - level);
#else
	gtk_range_set_value(GTK_RANGE(swap->scale), level);
#endif
}


/* callbacks */
/* swap_on_timeout */
static gboolean _swap_on_timeout(gpointer data)
{
	Swap * swap = data;
#if defined(__APPLE__)
	int mib[] = { CTL_VM, VM_SWAPUSAGE };
	struct xsw_usage sw;
	size_t size = sizeof(sw);
	gdouble value;

	if(sysctl(mib, 2, &sw, &size, NULL, 0) != 0)
	{
		error_set("%s: %s: %s", applet.name, "sysctl",
				strerror(errno));
		return swap->helper->error(NULL, error_get(NULL), TRUE);
	}
	value = sw.xsu_used;
	value /= sw.xsu_total;
	_swap_set(swap, value);
	return TRUE;
#elif defined(__FreeBSD__)
	int mib[] = { CTL_VM, VM_TOTAL };
	struct vmtotal vm;
	size_t size = sizeof(vm);
	gdouble value;

	if(sysctl(mib, 2, &vm, &size, NULL, 0) < 0)
		return TRUE;
	value = vm.t_avm;
	value /= vm.t_vm;
	_swap_set(swap, value);
	return TRUE;
#elif defined(__linux__)
	struct sysinfo sy;
	gdouble value;

	if(sysinfo(&sy) != 0)
		return swap->helper->error(swap->helper->panel, "sysinfo",
				TRUE);
	if((value = sy.totalswap - sy.freeswap) != 0.0 && sy.totalswap != 0)
		value /= sy.totalswap;
	_swap_set(swap, value);
	return TRUE;
#elif defined(__NetBSD__)
	int mib[] = { CTL_VM, VM_UVMEXP };
	struct uvmexp ue;
	size_t size = sizeof(ue);
	gdouble value;

	if(sysctl(mib, 2, &ue, &size, NULL, 0) < 0)
		return TRUE;
	if((value = ue.swpgonly) != 0.0 && ue.swpages != 0)
		value /= ue.swpages;
	_swap_set(swap, value);
	return TRUE;
#else

	/* FIXME not supported */
	swap->timeout = 0;
	return FALSE;
#endif
}
