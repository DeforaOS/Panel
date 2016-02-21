/* $Id$ */
/* Copyright (c) 2010-2016 Pierre Pronchery <khorben@defora.org> */
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
#elif defined(__FreeBSD__)
# include <sys/sysctl.h>
# include <sys/vmmeter.h>
# include <vm/vm_param.h>
#elif defined(__NetBSD__)
# include <sys/sysctl.h>
# include <sys/vmmeter.h>
#endif
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* Memory */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
	GtkWidget * scale;
	guint timeout;
} Memory;


/* prototypes */
static Memory * _memory_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _memory_destroy(Memory * memory);

/* accessors */
static void _memory_set(Memory * memory, gdouble level);

/* callbacks */
#if defined(__FreeBSD__) || defined(__linux__) || defined(__NetBSD__)
static gboolean _memory_on_timeout(gpointer data);
#endif


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Memory",
	"gnome-monitor",
	NULL,
	_memory_init,
	_memory_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* memory_init */
static Memory * _memory_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
#if defined(__FreeBSD__) || defined(__linux__) || defined(__NetBSD__)
	const int timeout = 5000;
	Memory * memory;
	GtkOrientation orientation;
	PangoFontDescription * desc;
	GtkWidget * label;

	if((memory = malloc(sizeof(*memory))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	memory->helper = helper;
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	memory->widget = gtk_box_new(orientation, 0);
#else
	memory->widget = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? gtk_hbox_new(FALSE, 0) : gtk_vbox_new(FALSE, 0);
#endif
	/* invert the orientation for the meter */
	orientation = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	label = gtk_label_new(_("RAM:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(label, desc);
#else
	gtk_widget_modify_font(label, desc);
#endif
	gtk_box_pack_start(GTK_BOX(memory->widget), label, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(3, 6, 0)
	memory->scale = gtk_level_bar_new_for_interval(0.0, 100.0);
# if GTK_CHECK_VERSION(3, 8, 0)
	gtk_level_bar_set_inverted(GTK_LEVEL_BAR(memory->scale), TRUE);
# endif
	gtk_orientable_set_orientation(GTK_ORIENTABLE(memory->scale),
			orientation);
#else
# if GTK_CHECK_VERSION(3, 0, 0)
	memory->scale = gtk_scale_new_with_range(orientation, 0, 100, 1);
# else
	memory->scale = gtk_vscale_new_with_range(0, 100, 1);
# endif
	gtk_widget_set_sensitive(memory->scale, FALSE);
	gtk_range_set_inverted(GTK_RANGE(memory->scale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(memory->scale), GTK_POS_RIGHT);
#endif
	gtk_box_pack_start(GTK_BOX(memory->widget), memory->scale, FALSE, FALSE,
			0);
	memory->timeout = g_timeout_add(timeout, _memory_on_timeout, memory);
	_memory_on_timeout(memory);
	pango_font_description_free(desc);
	gtk_widget_show_all(memory->widget);
	*widget = memory->widget;
	return memory;
#else
	error_set("%s: %s", applet.name, _("Unsupported platform"));
	return NULL;
#endif
}


/* memory_destroy */
static void _memory_destroy(Memory * memory)
{
	g_source_remove(memory->timeout);
	gtk_widget_destroy(memory->widget);
	free(memory);
}


/* accessors */
/* memory_set */
static void _memory_set(Memory * memory, gdouble level)
{
#if GTK_CHECK_VERSION(3, 8, 0)
	gtk_level_bar_set_value(GTK_LEVEL_BAR(memory->scale), level);
#elif GTK_CHECK_VERSION(3, 6, 0)
	/* invert the value ourselves */
	gtk_level_bar_set_value(GTK_LEVEL_BAR(memory->scale), 100.0 - level);
#else
	gtk_range_set_value(GTK_RANGE(memory->scale), level);
#endif
}


/* callbacks */
/* memory_on_timeout */
#if defined(__linux__)
static gboolean _memory_on_timeout(gpointer data)
{
	Memory * memory = data;
	struct sysinfo sy;
	gdouble value;

	if(sysinfo(&sy) != 0)
	{
		error_set("%s: %s: %s", applet.name, "sysinfo",
				strerror(errno));
		return memory->helper->error(NULL, error_get(NULL), TRUE);
	}
	value = sy.sharedram;
	value /= sy.totalram;
	_memory_set(memory, value);
	return TRUE;
}
#elif defined(__FreeBSD__) || defined(__NetBSD__)
static gboolean _memory_on_timeout(gpointer data)
{
	Memory * memory = data;
	int mib[] = { CTL_VM, VM_METER };
	struct vmtotal vm;
	size_t size = sizeof(vm);
	gdouble value;

	if(sysctl(mib, 2, &vm, &size, NULL, 0) < 0)
	{
		error_set("%s: %s: %s", applet.name, "sysctl",
				strerror(errno));
		return TRUE;
	}
	value = vm.t_arm * 100;
	value /= (vm.t_rm + vm.t_free);
	_memory_set(memory, value);
	return TRUE;
}
#endif
