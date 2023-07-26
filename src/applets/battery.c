/* $Id$ */
/* Copyright (c) 2010-2023 Pierre Pronchery <khorben@defora.org> */
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
#if defined(__NetBSD__)
# include <sys/types.h>
# include <sys/ioctl.h>
# include <sys/envsys.h>
# include <fcntl.h>
# include <unistd.h>
# include <paths.h>
#elif defined(__linux__)
# include <fcntl.h>
# include <unistd.h>
# include <stdio.h>
#endif
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)
#define N_(string) string


/* Battery */
/* private */
/* types */
typedef enum _BatteryLevel
{
	BATTERY_LEVEL_UNKNOWN = 0,
	BATTERY_LEVEL_ERROR,
	BATTERY_LEVEL_EMPTY,
	BATTERY_LEVEL_CAUTION,
	BATTERY_LEVEL_LOW,
	BATTERY_LEVEL_GOOD,
	BATTERY_LEVEL_FULL
} BatteryLevel;
#define BATTERY_LEVEL_LAST	BATTERY_LEVEL_FULL
#define BATTERY_LEVEL_COUNT	(BATTERY_LEVEL_LAST + 1)

typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	BatteryLevel level;
	int charging;

	/* widgets */
	GtkWidget * box;
	GtkWidget * image;
	GtkWidget * label;
	GtkWidget * progress;
	guint timeout;

	/* preferences */
	GtkWidget * pr_level;

	/* platform-specific */
#if defined(__NetBSD__) || defined(__linux__)
	int fd;
#endif
} Battery;


/* prototypes */
static Battery * _battery_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _battery_destroy(Battery * battery);
static GtkWidget * _battery_settings(Battery * battery, gboolean apply,
		gboolean reset);

/* accessors */
static gboolean _battery_get(Battery * battery, gdouble * level,
		gboolean * charging);
static void _battery_set(Battery * battery, gdouble value, gboolean charging);

/* callbacks */
static gboolean _battery_on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Battery"),
	"battery",
	NULL,
	_battery_init,
	_battery_destroy,
	_battery_settings,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* battery_init */
static Battery * _battery_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const int timeout = 5000;
	Battery * battery;
	GtkIconSize iconsize;
	GtkWidget * vbox;
	GtkWidget * hbox;
	PangoFontDescription * bold;

	if((battery = object_new(sizeof(*battery))) == NULL)
		return NULL;
	battery->helper = helper;
	battery->level = -1;
	battery->charging = -1;
	battery->timeout = 0;
#if defined(__NetBSD__) || defined(__linux__)
	battery->fd = -1;
#endif
	iconsize = panel_window_get_icon_size(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	battery->box = hbox;
	battery->image = gtk_image_new_from_icon_name(applet.icon, iconsize);
	gtk_box_pack_start(GTK_BOX(hbox), battery->image, TRUE, TRUE, 0);
	battery->label = NULL;
	battery->progress = NULL;
	battery->pr_level = NULL;
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
		battery->progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION(3, 0, 0)
		gtk_progress_bar_set_show_text(
				GTK_PROGRESS_BAR(battery->progress), TRUE);
#endif
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(battery->progress),
				"");
		gtk_box_pack_start(GTK_BOX(vbox), battery->progress, TRUE, TRUE,
				0);
		battery->box = vbox;
		pango_font_description_free(bold);
	}
	else
	{
#ifndef EMBEDDED
		battery->label = gtk_label_new(" ");
		gtk_box_pack_start(GTK_BOX(hbox), battery->label, FALSE, TRUE,
				0);
		gtk_widget_show(battery->label);
#endif
		battery->box = hbox;
	}
	battery->timeout = g_timeout_add(timeout, _battery_on_timeout, battery);
	_battery_on_timeout(battery);
	gtk_widget_show(battery->image);
	*widget = battery->box;
	return battery;
}


/* battery_destroy */
static void _battery_destroy(Battery * battery)
{
	if(battery->timeout > 0)
		g_source_remove(battery->timeout);
#if defined(__NetBSD__) || defined(__linux__)
	if(battery->fd != -1)
		close(battery->fd);
#endif
	gtk_widget_destroy(battery->box);
	object_delete(battery);
}


/* battery_settings */
static void _settings_apply(Battery * battery, PanelAppletHelper * helper);
static void _settings_reset(Battery * battery, PanelAppletHelper * helper);

static GtkWidget * _battery_settings(Battery * battery, gboolean apply,
		gboolean reset)
{
	PanelAppletHelper * helper = battery->helper;

	if(battery->pr_level == NULL)
	{
		battery->pr_level = gtk_check_button_new_with_label(
				_("Show the battery level"));
		gtk_widget_show(battery->pr_level);
		reset = TRUE;
	}
	if(reset == TRUE)
		_settings_reset(battery, helper);
	if(apply == TRUE)
		_settings_apply(battery, helper);
	return battery->pr_level;
}

static void _settings_apply(Battery * battery, PanelAppletHelper * helper)
{
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				battery->pr_level));
	if(battery->label != NULL)
	{
		if(active)
			gtk_widget_show(battery->label);
		else
			gtk_widget_hide(battery->label);
	}
	helper->config_set(helper->panel, "battery", "level",
			active ? "1" : "0");
}

static void _settings_reset(Battery * battery, PanelAppletHelper * helper)
{
#ifndef EMBEDDED
	gboolean active = TRUE;
#else
	gboolean active = FALSE;
#endif
	char const * p;

	if((p = helper->config_get(helper->panel, "battery", "level")) != NULL)
		active = strtol(p, NULL, 10) ? TRUE : FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(battery->pr_level),
			active);
}


/* accessors */
/* battery_get */
#if defined(__NetBSD__)
static int _get_tre(int fd, int sensor, envsys_tre_data_t * tre);

static gboolean _battery_get(Battery * battery, gdouble * level,
		gboolean * charging)
{
	int i;
	envsys_basic_info_t info;
	envsys_tre_data_t tre;
	unsigned int rate = 0;
	unsigned int charge = 0;
	unsigned int maxcharge = 0;

	*charging = FALSE;
	if(battery->fd < 0 && (battery->fd = open(_PATH_SYSMON, O_RDONLY)) < 0)
	{
		error_set("%s: %s: %s", applet.name, _PATH_SYSMON,
				strerror(errno));
		*level = -1.0;
		return TRUE;
	}
	for(i = 0; i >= 0; i++)
	{
		memset(&info, 0, sizeof(info));
		info.sensor = i;
		if(ioctl(battery->fd, ENVSYS_GTREINFO, &info) == -1)
		{
			close(battery->fd);
			battery->fd = -1;
			error_set("%s: %s: %s", applet.name, "ENVSYS_GTREINFO",
					strerror(errno));
			*level = -1.0;
			return TRUE;
		}
		if(!(info.validflags & ENVSYS_FVALID))
			break;
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() %d \"%s\"\n", __func__, i,
				info.desc);
#endif
		if(strcmp("acpiacad0 connected", info.desc) == 0
				&& _get_tre(battery->fd, i, &tre) == 0
				&& tre.validflags & ENVSYS_FCURVALID)
			/* FIXME implement */
			continue;
		if(strncmp("acpibat", info.desc, 7) != 0
				|| info.desc[7] == '\0'
				|| info.desc[8] != ' ')
			continue;
		if(strcmp("charge", &info.desc[9]) == 0
				&& _get_tre(battery->fd, i, &tre) == 0
				&& tre.validflags & ENVSYS_FCURVALID
				&& tre.validflags & ENVSYS_FMAXVALID)
		{
			charge += tre.cur.data_us;
			maxcharge += tre.max.data_us;
		}
		else if(strcmp("charge rate", &info.desc[9]) == 0
				&& _get_tre(battery->fd, i, &tre) == 0
				&& tre.validflags & ENVSYS_FCURVALID)
			rate += tre.cur.data_us;
		else if(strcmp("charging", &info.desc[9]) == 0
				&& _get_tre(battery->fd, i, &tre) == 0
				&& tre.validflags & ENVSYS_FCURVALID
				&& tre.cur.data_us > 0)
		{
			*charging = TRUE;
			continue;
		}
		else if(strcmp("discharge rate", &info.desc[9]) == 0
				&& _get_tre(battery->fd, i, &tre) == 0
				&& tre.validflags & ENVSYS_FCURVALID)
			rate += tre.cur.data_us;
	}
	*level = (charge * 100.0) / maxcharge;
	return TRUE;
}

static int _get_tre(int fd, int sensor, envsys_tre_data_t * tre)
{
	memset(tre, 0, sizeof(*tre));
	tre->sensor = sensor;
	if(ioctl(fd, ENVSYS_GTREDATA, tre) == -1)
		return 1;
	return !(tre->validflags & ENVSYS_FVALID);
}
#elif defined(__linux__)
static gboolean _battery_get(Battery * battery, gdouble * level,
		gboolean * charging)
{
	const char apm[] = "/proc/apm";
	char buf[80];
	ssize_t buf_cnt;
	double d;
	unsigned int u;
	unsigned int x = 0;
	int i;
	int b;

	*charging = FALSE;
	if(battery->fd < 0 && (battery->fd = open(apm, O_RDONLY)) < 0)
	{
		error_set("%s: %s: %s", applet.name, apm, strerror(errno));
		*level = 0.0 / 0.0;
		return TRUE;
	}
	errno = ENODATA;
	if((buf_cnt = read(battery->fd, buf, sizeof(buf))) <= 0)
	{
		error_set("%s: %s: %s", applet.name, apm, strerror(errno));
		close(battery->fd);
		battery->fd = -1;
		*level = 0.0 / 0.0;
		return TRUE;
	}
	buf[--buf_cnt] = '\0';
	if(sscanf(buf, "%lf %lf %x %x %x %x %d%% %d min", &d, &d, &u, &x, &u,
				&u, &b, &i) != 8)
	{
		error_set("%s: %s: %s", applet.name, apm, strerror(errno));
		*level = 0.0 / 0.0;
	}
	else
		*level = b;
	*charging = (x != 0) ? TRUE : FALSE;
	close(battery->fd);
	battery->fd = -1;
	return TRUE;
}
#else
# warning Unsupported platform: battery is not available
static gboolean _battery_get(Battery * battery, gdouble * level,
		gboolean * charging)
{
	const gdouble error = 0.0 / 0.0;
	(void) battery;

	*level = error;
	*charging = FALSE;
	error_set("%s: %s", applet.name, strerror(ENOSYS));
	return FALSE;
}
#endif


/* battery_set */
static void _set_image(Battery * battery, BatteryLevel level,
		gboolean charging);

static void _battery_set(Battery * battery, gdouble value, gboolean charging)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "%.0f%% ", value);
	/* XXX only show when necessary? */
	if(value >= 0.0 && value <= 100.0)
	{
		gtk_widget_show(battery->box);
		if(battery->progress != NULL)
		{
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(
						battery->progress),
					value / 100.0);
			gtk_widget_show(battery->progress);
		}
	}
	else if(battery->progress != NULL)
		gtk_widget_hide(battery->progress);
	if(value < 0.0)
	{
		_set_image(battery, BATTERY_LEVEL_UNKNOWN, FALSE);
		value = 0.0;
		snprintf(buf, sizeof(buf), "%s", _("Unknown"));
	}
	else if(value <= 1.0)
		_set_image(battery, BATTERY_LEVEL_EMPTY, charging);
	else if(value <= 10.0)
		_set_image(battery, BATTERY_LEVEL_CAUTION, charging);
	else if(value <= 20.0)
		_set_image(battery, BATTERY_LEVEL_LOW, charging);
	else if(value <= 75.0)
		_set_image(battery, BATTERY_LEVEL_GOOD, charging);
	else if(value <= 100.0)
		_set_image(battery, BATTERY_LEVEL_FULL, charging);
	else
	{
		_set_image(battery, BATTERY_LEVEL_ERROR, FALSE);
		value = 0.0;
		snprintf(buf, sizeof(buf), "%s", _("Error"));
	}
	if(battery->label != NULL)
		gtk_label_set_text(GTK_LABEL(battery->label), buf);
	if(battery->progress != NULL)
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(battery->progress),
				buf);
#if GTK_CHECK_VERSION(2, 12, 0)
	snprintf(buf, sizeof(buf), _("Battery level: %.0f%%%s"), value,
			charging ? _(" (charging)") : "");
	gtk_widget_set_tooltip_text(battery->box, buf);
#endif
}

static void _set_image(Battery * battery, BatteryLevel level, gboolean charging)
{
	char const * icons[BATTERY_LEVEL_COUNT][2] =
	{
		{ "stock_dialog-question", "stock_dialog-question"	},
		{ "battery-missing", "battery-missing"			},
		{ "battery-empty", "battery-caution-charging"		},
		{ "battery-caution", "battery-caution-charging"		},
		{ "battery-low", "battery-low-charging"			},
		{ "battery-good", "battery-good-charging"		},
		{ "battery-full", "battery-full-charging"		}
	};

	if(battery->level == level && battery->charging == charging)
		return;
	battery->level = level;
	battery->charging = charging;
	gtk_image_set_from_icon_name(GTK_IMAGE(battery->image),
			icons[level][charging ? 1 : 0],
			panel_window_get_icon_size(battery->helper->window));
}


/* callbacks */
/* battery_on_timeout */
static gboolean _battery_on_timeout(gpointer data)
{
	const gdouble error = 0.0 / 0.0;
	Battery * battery = data;
	PanelAppletHelper * helper = battery->helper;
	gdouble level;
	gboolean charging;
	int timeout;

	if(_battery_get(battery, &level, &charging) == FALSE)
	{
		helper->error(NULL, error_get(NULL), 1);
		timeout = 0;
	}
	else if(level == error || level < 0.0)
	{
		helper->error(NULL, error_get(NULL), 1);
		timeout = 30000;
	}
	else
		timeout = 5000;
	_battery_set(battery, level, charging);
	battery->timeout = (timeout > 0)
		? g_timeout_add(timeout, _battery_on_timeout, battery) : 0;
	return FALSE;
}
