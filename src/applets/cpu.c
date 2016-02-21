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
#include <time.h>
#include <errno.h>
#if defined(__FreeBSD__) || defined(__NetBSD__)
# if defined(__FreeBSD__)
#  include <sys/resource.h>
# endif
# include <sys/sysctl.h>
#endif
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* CPU */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * widget;
	GtkWidget ** scales;
	size_t scales_cnt;
	guint timeout;
#if defined(__FreeBSD__) || defined(__NetBSD__)
	int used;
	int total;
#endif
} CPU;


/* prototypes */
static CPU * _cpu_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _cpu_destroy(CPU * cpu);

/* accessors */
static gboolean _cpu_get(CPU * cpu, size_t index, gdouble * level);
static size_t _cpu_get_count(void);
static void _cpu_set(CPU * cpu, size_t index, gdouble level);

/* callbacks */
static gboolean _cpu_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"CPU",
	"gnome-monitor",
	NULL,
	_cpu_init,
	_cpu_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* cpu_init */
static CPU * _cpu_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const int timeout = 500;
	CPU * cpu;
	GtkOrientation orientation;
	PangoFontDescription * desc;
	GtkWidget * label;
	size_t i;

	if((cpu = malloc(sizeof(*cpu))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	cpu->scales_cnt = _cpu_get_count();
	if((cpu->scales = malloc(sizeof(*cpu->scales) * cpu->scales_cnt))
			== NULL)
	{
		_cpu_destroy(cpu);
		return NULL;
	}
	cpu->helper = helper;
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	cpu->widget = gtk_box_new(orientation, 0);
#else
	cpu->widget = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? gtk_hbox_new(FALSE, 0) : gtk_vbox_new(FALSE, 0);
#endif
	/* invert the orientation for meters */
	orientation = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	label = gtk_label_new(_("CPU:"));
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(label, desc);
#else
	gtk_widget_modify_font(label, desc);
#endif
	gtk_box_pack_start(GTK_BOX(cpu->widget), label, FALSE, FALSE, 0);
	for(i = 0; i < cpu->scales_cnt; i++)
	{
#if GTK_CHECK_VERSION(3, 6, 0)
		cpu->scales[i] = gtk_level_bar_new_for_interval(0.0, 100.0);
# if GTK_CHECK_VERSION(3, 8, 0)
		gtk_level_bar_set_inverted(GTK_LEVEL_BAR(cpu->scales[i]), TRUE);
# endif
		gtk_orientable_set_orientation(GTK_ORIENTABLE(
					cpu->scales[i]), orientation);
#else
# if GTK_CHECK_VERSION(3, 0, 0)
		cpu->scales[i] = gtk_scale_new_with_range(orientation, 0, 100,
				1);
# else
		cpu->scales[i] = gtk_vscale_new_with_range(0, 100, 1);
# endif
		gtk_widget_set_sensitive(cpu->scales[i], FALSE);
		gtk_range_set_inverted(GTK_RANGE(cpu->scales[i]),
				TRUE);
		gtk_scale_set_value_pos(GTK_SCALE(cpu->scales[i]),
				GTK_POS_RIGHT);
		gtk_widget_set_sensitive(cpu->scales[i], FALSE);
		gtk_range_set_inverted(GTK_RANGE(cpu->scales[i]), TRUE);
		gtk_scale_set_value_pos(GTK_SCALE(cpu->scales[i]),
				GTK_POS_RIGHT);
#endif
		gtk_box_pack_start(GTK_BOX(cpu->widget), cpu->scales[i], FALSE,
				FALSE, 0);
	}
	cpu->timeout = g_timeout_add(timeout, _cpu_on_timeout, cpu);
	cpu->used = 0;
	cpu->total = 0;
	_cpu_on_timeout(cpu);
	pango_font_description_free(desc);
	gtk_widget_show_all(cpu->widget);
	*widget = cpu->widget;
	return cpu;
}


/* cpu_destroy */
static void _cpu_destroy(CPU * cpu)
{
	free(cpu->scales);
	if(cpu->timeout > 0)
		g_source_remove(cpu->timeout);
	gtk_widget_destroy(cpu->widget);
	free(cpu);
}


/* accessors */
/* cpu_get */
static gboolean _cpu_get(CPU * cpu, size_t index, gdouble * level)
{
#if defined(__FreeBSD__) || defined(__NetBSD__)
# if defined(__FreeBSD__)
	char const name[] = "kern.cp_time";
# elif defined(__NetBSD__)
	int mib[] = { CTL_KERN, KERN_CP_TIME };
# endif
	uint64_t cpu_time[CPUSTATES];
	size_t size = sizeof(cpu_time);
	int used;
	int total;

	if(index >= cpu->scales_cnt)
	{
		error_set("%s %zu: %s", applet.name, index, strerror(ERANGE));
		return FALSE;
	}
# if defined(__FreeBSD__)
	if(sysctlbyname(name, &cpu_time, &size, NULL, 0) < 0)
# elif defined(__NetBSD__)
	if(sysctl(mib, sizeof(mib) / sizeof(*mib), &cpu_time, &size, NULL, 0)
			< 0)
# endif
	{
		*level = 0.0 / 0.0;
		error_set("%s: %s: %s", applet.name, "sysctl", strerror(errno));
		return TRUE;
	}
	used = cpu_time[CP_USER] + cpu_time[CP_SYS] + cpu_time[CP_NICE]
		+ cpu_time[CP_INTR];
	total = used + cpu_time[CP_IDLE];
	if(cpu->used == 0 || total == cpu->total)
		*level = 0.0;
	else
		*level = 100.0 * (used - cpu->used) / (total - cpu->total);
	cpu->used = used;
	cpu->total = total;
	return TRUE;
#else
	(void) cpu;

	if(index >= cpu->scales_cnt)
	{
		error_set("%s %zu: %s", applet.name, index, strerror(ERANGE));
		return FALSE;
	}
	*level = 0.0 / 0.0;
	error_set("%s: %s", applet.name, strerror(ENOSYS));
	return FALSE;
#endif
}


/* cpu_get_count */
static size_t _cpu_get_count(void)
{
	return 1;
}


/* cpu_set */
static void _cpu_set(CPU * cpu, size_t index, gdouble level)
{
	if(index >= cpu->scales_cnt)
		return;
#if GTK_CHECK_VERSION(3, 8, 0)
	gtk_level_bar_set_value(GTK_LEVEL_BAR(cpu->scales[index]), level);
#elif GTK_CHECK_VERSION(3, 6, 0)
	/* invert the value ourselves */
	gtk_level_bar_set_value(GTK_LEVEL_BAR(cpu->scales[index]),
			100.0 - level);
#else
	gtk_range_set_value(GTK_RANGE(cpu->scales[index]), level);
#endif
}


/* callbacks */
/* cpu_on_timeout */
static gboolean _cpu_on_timeout(gpointer data)
{
	CPU * cpu = data;
	PanelAppletHelper * helper = cpu->helper;
	size_t i;
	gdouble level;

	for(i = 0; i < cpu->scales_cnt; i++)
	{
		if(_cpu_get(cpu, i, &level) == FALSE)
		{
			helper->error(NULL, error_get(NULL), 1);
			level = 0;
		}
		_cpu_set(cpu, i, level);
	}
	return TRUE;
}
