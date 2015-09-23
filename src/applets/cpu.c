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
	GtkWidget * scale;
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
static gboolean _cpu_get(CPU * cpu, gdouble * level);
static void _cpu_set(CPU * cpu, gdouble level);

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
	PangoFontDescription * desc;
	GtkWidget * label;

	if((cpu = malloc(sizeof(*cpu))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	cpu->helper = helper;
#if GTK_CHECK_VERSION(3, 0, 0)
	cpu->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	cpu->widget = gtk_hbox_new(FALSE, 0);
#endif
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	label = gtk_label_new(_("CPU:"));
	gtk_widget_modify_font(label, desc);
	gtk_box_pack_start(GTK_BOX(cpu->widget), label, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	cpu->scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100,
			1);
#else
	cpu->scale = gtk_vscale_new_with_range(0, 100, 1);
#endif
	gtk_widget_set_sensitive(cpu->scale, FALSE);
	gtk_range_set_inverted(GTK_RANGE(cpu->scale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(cpu->scale), GTK_POS_RIGHT);
	gtk_box_pack_start(GTK_BOX(cpu->widget), cpu->scale, FALSE, FALSE, 0);
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
	if(cpu->timeout > 0)
		g_source_remove(cpu->timeout);
	gtk_widget_destroy(cpu->widget);
	free(cpu);
}


/* accessors */
/* cpu_get */
static gboolean _cpu_get(CPU * cpu, gdouble * level)
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

# if defined(__FreeBSD__)
	if(sysctlbyname(name, &cpu_time, &size, NULL, 0) < 0)
# elif defined(__NetBSD__)
	if(sysctl(mib, 2, &cpu_time, &size, NULL, 0) < 0)
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
	*level = 0.0 / 0.0;
	error_set("%s: %s", applet.name, strerror(ENOSYS));
	return FALSE;
#endif
}


/* cpu_set */
static void _cpu_set(CPU * cpu, gdouble level)
{
	gtk_range_set_value(GTK_RANGE(cpu->scale), level);
}


/* callbacks */
/* cpu_on_timeout */
static gboolean _cpu_on_timeout(gpointer data)
{
	CPU * cpu = data;
	PanelAppletHelper * helper = cpu->helper;
	gdouble level;

	if(_cpu_get(cpu, &level) == FALSE)
	{
		cpu->timeout = 0;
		helper->error(NULL, error_get(), 1);
		return FALSE;
	}
	_cpu_set(cpu, level);
	return TRUE;
}
