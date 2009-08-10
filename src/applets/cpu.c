/* $Id$ */
/* Copyright (c) 2009 Pierre Pronchery <khorben@defora.org> */
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
#ifdef __NetBSD__
# include <sys/sysctl.h>
#endif
#include "panel.h"
#include "../../config.h"


/* Cpu */
/* private */
/* types */
typedef struct _Cpu
{
	GtkWidget * scale;
	guint timeout;
#ifdef __NetBSD__
	int used;
	int total;
#endif
} Cpu;


/* prototypes */
static GtkWidget * _cpu_init(PanelApplet * applet);
static void _cpu_destroy(PanelApplet * applet);

/* callbacks */
static gboolean _on_timeout(gpointer data);


/* public */
/* variables */
PanelApplet applet =
{
	NULL,
	_cpu_init,
	_cpu_destroy,
	PANEL_APPLET_POSITION_END,
	FALSE,
	TRUE,
	NULL
};


/* private */
/* functions */
/* cpu_init */
static GtkWidget * _cpu_init(PanelApplet * applet)
{
	GtkWidget * ret;
	Cpu * cpu;
	PangoFontDescription * desc;
	GtkWidget * widget;

	if((cpu = malloc(sizeof(*cpu))) == NULL)
		return NULL;
	applet->priv = cpu;
	ret = gtk_hbox_new(FALSE, 0);
	desc = pango_font_description_new();
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	widget = gtk_label_new("CPU:");
	gtk_widget_modify_font(widget, desc);
	gtk_box_pack_start(GTK_BOX(ret), widget, FALSE, FALSE, 0);
	cpu->scale = gtk_vscale_new_with_range(0, 100, 1);
	gtk_widget_set_sensitive(cpu->scale, FALSE);
	gtk_range_set_inverted(GTK_RANGE(cpu->scale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(cpu->scale), GTK_POS_RIGHT);
	gtk_box_pack_start(GTK_BOX(ret), cpu->scale, FALSE, FALSE, 0);
	cpu->timeout = g_timeout_add(500, _on_timeout, cpu);
#ifdef __NetBSD__
	cpu->used = 0;
	cpu->total = 0;
#endif
	_on_timeout(cpu);
	pango_font_description_free(desc);
	return ret;
}


/* cpu_destroy */
static void _cpu_destroy(PanelApplet * applet)
{
	Cpu * cpu = applet->priv;

	g_source_remove(cpu->timeout);
	free(cpu);
}


/* callbacks */
/* on_timeout */
static gboolean _on_timeout(gpointer data)
{
#ifdef __NetBSD__
	Cpu * cpu = data;
	int mib[] = { CTL_KERN, KERN_CP_TIME };
	uint64_t cpu_time[CPUSTATES];
	size_t size = sizeof(cpu_time);
	int used;
	int total;
	gdouble value;

	if(sysctl(mib, 2, &cpu_time, &size, NULL, 0) < 0)
		return TRUE;
	used = cpu_time[CP_USER] + cpu_time[CP_SYS] + cpu_time[CP_NICE]
		+ cpu_time[CP_INTR];
	total = used + cpu_time[CP_IDLE];
	if(cpu->used == 0)
		value = 0;
	else
		value = 100 * (used - cpu->used) / (total - cpu->total);
	cpu->used = used;
	cpu->total = total;
	gtk_range_set_value(GTK_RANGE(cpu->scale), value);
#endif
	return TRUE;
}