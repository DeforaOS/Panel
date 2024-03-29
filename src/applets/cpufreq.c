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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#if defined(__APPLE__)
# include <sys/param.h>
# include <sys/sysctl.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
# include <sys/param.h>
# include <sys/sched.h>
# include <sys/sysctl.h>
#endif
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string


/* Cpufreq */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GtkWidget * hbox;
	GtkWidget * label;
	guint timeout;
	int64_t min;
	int64_t max;
	int64_t current;
	int64_t step;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
	char const * name;
#endif
} Cpufreq;


/* prototypes */
static Cpufreq * _cpufreq_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _cpufreq_destroy(Cpufreq * cpufreq);

/* callbacks */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
static gboolean _cpufreq_on_timeout(gpointer data);
#endif


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("CPU frequency"),
	"gnome-monitor",
	NULL,
	_cpufreq_init,
	_cpufreq_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* cpufreq_init */
static Cpufreq * _cpufreq_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const int timeout = 1000;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
	Cpufreq * cpufreq;
	PangoFontDescription * desc;
	GtkWidget * image;
	GtkWidget * label;
	char const * p;
# if defined(__APPLE__)
	int64_t i;
	int64_t imin;
	int64_t imax;
	size_t isize = sizeof(i);

	p = "hw.cpufrequency";
	if(sysctlbyname(p, &i, &isize, NULL, 0) != 0
			|| sysctlbyname("hw.cpufrequency_min", &imin, &isize,
				NULL, 0) != 0
			|| sysctlbyname("hw.cpufrequency_max", &imax, &isize,
				NULL, 0) != 0)
	{
		error_set("%s: %s", applet.name, _("No support detected"));
		return NULL;
	}
# else
	char freq[256];
	size_t freqsize = sizeof(freq);
	char const * q;

	/* detect the correct sysctl */
	if(sysctlbyname("hw.clockrate", &freq, &freqsize, NULL, 0) == 0)
		p = "hw.clockrate";
	else if(sysctlbyname("machdep.est.frequency.available", &freq,
				&freqsize, NULL, 0) == 0)
		p = "machdep.est.frequency.current";
	else if(sysctlbyname("machdep.powernow.frequency.available", &freq,
				&freqsize, NULL, 0) == 0)
		p = "machdep.powernow.frequency.current";
	else if(sysctlbyname("machdep.frequency.available", &freq, &freqsize,
				NULL, 0) == 0)
		p = "machdep.frequency.current";
	else if(sysctlbyname("machdep.cpu.frequency.available", &freq,
				&freqsize, NULL, 0) == 0)
		p = "machdep.cpu.frequency.current";
	else
	{
		error_set("%s: %s", applet.name, _("No support detected"));
		return NULL;
	}
# endif
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, p);
# endif
	if((cpufreq = malloc(sizeof(*cpufreq))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	cpufreq->helper = helper;
	desc = pango_font_description_new();
	pango_font_description_set_family(desc, "Monospace");
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
# if GTK_CHECK_VERSION(3, 0, 0)
	cpufreq->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
# else
	cpufreq->hbox = gtk_hbox_new(FALSE, 4);
# endif
	image = gtk_image_new_from_icon_name(applet.icon,
			panel_window_get_icon_size(helper->window));
	gtk_box_pack_start(GTK_BOX(cpufreq->hbox), image, FALSE, TRUE, 0);
# if defined(__APPLE__)
	cpufreq->max = imax / 1000000;
	cpufreq->min = imin / 1000000;
# else
	cpufreq->max = atoll(freq);
	cpufreq->min = (q = strrchr(freq, ' ')) != NULL ? atoll(q)
		: cpufreq->max;
# endif
	cpufreq->current = -1;
	cpufreq->step = 1;
	cpufreq->name = p;
	cpufreq->label = gtk_label_new(" ");
# if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_override_font(cpufreq->label, desc);
# else
	gtk_widget_modify_font(cpufreq->label, desc);
# endif
	gtk_box_pack_start(GTK_BOX(cpufreq->hbox), cpufreq->label, FALSE, TRUE,
			0);
	label = gtk_label_new(_("MHz"));
	gtk_box_pack_start(GTK_BOX(cpufreq->hbox), label, FALSE, TRUE, 0);
	if(_cpufreq_on_timeout(cpufreq) == TRUE)
		cpufreq->timeout = g_timeout_add(timeout, _cpufreq_on_timeout,
				cpufreq);
	pango_font_description_free(desc);
	gtk_widget_show_all(cpufreq->hbox);
	*widget = cpufreq->hbox;
	return cpufreq;
#else
	error_set("%s: %s", applet.name, _("Unsupported platform"));
	return NULL;
#endif
}


/* cpufreq_destroy */
static void _cpufreq_destroy(Cpufreq * cpufreq)
{
	g_source_remove(cpufreq->timeout);
	gtk_widget_destroy(cpufreq->hbox);
	free(cpufreq);
}


/* callbacks */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
/* cpufreq_on_timeout */
static gboolean _cpufreq_on_timeout(gpointer data)
{
	Cpufreq * cpufreq = data;
	PanelAppletHelper * helper = cpufreq->helper;
	char buf[256];
	int64_t freq;
# if defined(__APPLE__)
	size_t freqsize = sizeof(freqsize);

	if(sysctlbyname(cpufreq->name, &freq, &freqsize, NULL, 0) == 0)
		freq /= 1000000;
	else
# else
	int i;
	size_t isize = sizeof(i);

	if(sysctlbyname(cpufreq->name, &i, &isize, NULL, 0) == 0)
		freq = i;
	else
# endif
	{
		error_set("%s: %s: %s", applet.name, cpufreq->name,
				strerror(errno));
		helper->error(NULL, error_get(NULL), 1);
		return TRUE;
	}
	if(freq < 0)
	{
		error_set("%s: %s: %s", applet.name, cpufreq->name,
				strerror(ERANGE));
		helper->error(NULL, error_get(NULL), 1);
		return TRUE;
	}
	if(cpufreq->current == freq)
		return TRUE;
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %d\n", __func__, freq);
# endif
	cpufreq->current = freq;
	snprintf(buf, sizeof(buf), "%4" PRId64, cpufreq->current);
	gtk_label_set_text(GTK_LABEL(cpufreq->label), buf);
# if GTK_CHECK_VERSION(2, 12, 0)
	snprintf(buf, sizeof(buf), "%s%" PRId64 " %s", _("CPU frequency: "),
			cpufreq->current, _("MHz"));
	gtk_widget_set_tooltip_text(cpufreq->hbox, buf);
# endif
	return TRUE;
}
#endif
