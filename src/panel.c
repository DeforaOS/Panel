/* $Id$ */
/* Copyright (c) 2009-2015 Pierre Pronchery <khorben@defora.org> */
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



#include <System.h>
#include <Desktop.h>
#include <sys/stat.h>
#ifdef __NetBSD__
# include <sys/param.h>
# include <sys/sysctl.h>
#else
# include <fcntl.h>
#endif
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include "window.h"
#include "panel.h"
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) string

/* constants */
#ifndef PROGNAME
# define PROGNAME	"panel"
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef LIBDIR
# define LIBDIR		PREFIX "/lib"
#endif


/* Panel */
/* private */
/* types */
struct _Panel
{
	Config * config;

	PanelPrefs prefs;

	PanelAppletHelper helpers[PANEL_POSITION_COUNT];
	PanelWindow * windows[PANEL_POSITION_COUNT];

	GdkScreen * screen;
	GdkWindow * root;
	gint root_width;		/* width of the root window	*/
	gint root_height;		/* height of the root window	*/
	guint source;
#if 1 /* XXX for tests */
	guint timeout;
#endif

	/* preferences */
	GtkWidget * pr_window;
	GtkWidget * pr_notebook;
	GtkWidget * pr_accept_focus;
	GtkWidget * pr_keep_above;
	GtkListStore * pr_store;
	GtkWidget * pr_view;
	GtkWidget * pr_panels_panel;
	GtkWidget * pr_panels_view;
	struct
	{
		GtkWidget * enabled;
		GtkWidget * size;
		GtkListStore * store;
	} pr_panels[PANEL_POSITION_COUNT];

	/* dialogs */
	GtkWidget * ab_window;
	GtkWidget * lo_window;
	GtkWidget * sh_window;
};

typedef void (*PanelPositionMenuHelper)(Panel * panel, GtkMenu * menu, gint * x,
		gint * y, gboolean * push_in);


/* constants */
static char const * _authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};

static const struct
{
	char const * name;
	char const * alias;
	GtkIconSize iconsize;
	gint size;
} _panel_sizes[] =
{
	{ "panel-large",   N_("Large"),   GTK_ICON_SIZE_LARGE_TOOLBAR, 48 },
	{ "panel-small",   N_("Small"),   GTK_ICON_SIZE_SMALL_TOOLBAR, 24 },
	{ "panel-smaller", N_("Smaller"), GTK_ICON_SIZE_MENU,          16 }
};


/* prototypes */
/* accessors */
static gboolean _panel_can_suspend(void);

static char const * _panel_get_applets(Panel * panel, PanelPosition position);
static char const * _panel_get_section(Panel * panel, PanelPosition position);

/* useful */
static void _panel_reset(Panel * panel, GdkRectangle * rect);

/* helpers */
#include "helper.c"

static char * _config_get_filename(void);


/* public */
/* panel_new */
static int _new_config(Panel * panel);
static void _new_helper(Panel * panel, PanelPosition position);
static void _new_prefs(Config * config, GdkScreen * screen, PanelPrefs * prefs,
		PanelPrefs const * user);
/* callbacks */
static int _new_on_message(void * data, uint32_t value1, uint32_t value2,
		uint32_t value3);
static GdkFilterReturn _on_root_event(GdkXEvent * xevent, GdkEvent * event,
		gpointer data);
static GdkFilterReturn _event_configure_notify(Panel * panel);

Panel * panel_new(PanelPrefs const * prefs)
{
	Panel * panel;
	size_t i;

	if((panel = object_new(sizeof(*panel))) == NULL)
		return NULL;
	panel->screen = gdk_screen_get_default();
	if(_new_config(panel) == 0)
		_new_prefs(panel->config, panel->screen, &panel->prefs, prefs);
	/* helpers */
	for(i = 0; i < PANEL_POSITION_COUNT; i++)
	{
		panel->windows[i] = NULL;
		_new_helper(panel, i);
	}
	panel->pr_window = NULL;
	panel->ab_window = NULL;
#ifndef EMBEDDED
	panel->lo_window = NULL;
#endif
	panel->sh_window = NULL;
	if(panel->config == NULL)
	{
		panel_error(NULL, NULL, 0); /* XXX put up a dialog box */
		panel_delete(panel);
		return NULL;
	}
	/* root window */
	panel->root = gdk_screen_get_root_window(panel->screen);
	panel->source = 0;
	panel->timeout = 0;
	/* panel windows */
	if(panel_reset(panel) != 0)
	{
		panel_error(NULL, NULL, 0); /* XXX as above */
		panel_delete(panel);
		return NULL;
	}
	/* messages */
	desktop_message_register(NULL, PANEL_CLIENT_MESSAGE, _new_on_message,
			panel);
	/* manage root window events */
	gdk_window_set_events(panel->root, gdk_window_get_events(panel->root)
			| GDK_PROPERTY_CHANGE_MASK);
	gdk_window_add_filter(panel->root, _on_root_event, panel);
	return panel;
}

static int _new_config(Panel * panel)
{
	char * filename;

	if((panel->config = config_new()) == NULL)
		return -1;
	if((filename = _config_get_filename()) == NULL)
		return -1;
	if(config_load(panel->config, filename) != 0)
		/* we can ignore this error */
		panel_error(NULL, _("Could not load configuration"), 1);
	free(filename);
	return 0;
}

static void _new_helper(Panel * panel, PanelPosition position)
{
	PanelAppletHelper * helper = &panel->helpers[position];
	const PanelPositionMenuHelper positions[PANEL_POSITION_COUNT] =
	{
		_panel_helper_position_menu_bottom,
		_panel_helper_position_menu_top,
		_panel_helper_position_menu_top, /* XXX */
		_panel_helper_position_menu_top  /* XXX */
	};
	String const * p;

	helper->panel = panel;
	helper->window = panel->windows[position];
	helper->config_get = _panel_helper_config_get;
	helper->config_set = _panel_helper_config_set;
	helper->error = _panel_helper_error;
	helper->about_dialog = _panel_helper_about_dialog;
	helper->lock = _panel_helper_lock;
#ifndef EMBEDDED
	if((p = panel_get_config(panel, NULL, "logout")) == NULL
			|| strtol(p, NULL, 0) != 0)
#else
	if((p = panel_get_config(panel, NULL, "logout")) != NULL
			&& strtol(p, NULL, 0) != 0)
#endif
		helper->logout_dialog = _panel_helper_logout_dialog;
	else
		helper->logout_dialog = NULL;
	helper->position_menu = positions[position];
	helper->preferences_dialog = _panel_helper_preferences_dialog;
	helper->rotate_screen = _panel_helper_rotate_screen;
	helper->shutdown_dialog = _panel_helper_shutdown_dialog;
	helper->suspend = (_panel_can_suspend()) ? _panel_helper_suspend : NULL;
}

static void _new_prefs(Config * config, GdkScreen * screen, PanelPrefs * prefs,
		PanelPrefs const * user)
{
	size_t i;
	gint width;
	gint height;
	char const * p;
	char * q;

	for(i = 0; i < sizeof(_panel_sizes) / sizeof(*_panel_sizes); i++)
	{
		if(gtk_icon_size_from_name(_panel_sizes[i].name)
				!= GTK_ICON_SIZE_INVALID)
			continue;
		if(gtk_icon_size_lookup(_panel_sizes[i].iconsize, &width,
					&height) != TRUE)
			width = height = _panel_sizes[i].size;
		gtk_icon_size_register(_panel_sizes[i].name, width, height);
	}
	if(user != NULL)
		memcpy(prefs, user, sizeof(*prefs));
	else
	{
		prefs->iconsize = PANEL_ICON_SIZE_DEFAULT;
		prefs->monitor = -1;
	}
	if((p = config_get(config, NULL, "monitor")) != NULL)
	{
		prefs->monitor = strtol(p, &q, 0);
		if(p[0] == '\0' || *q != '\0')
			prefs->monitor = -1;
	}
#if GTK_CHECK_VERSION(2, 20, 0)
	if(prefs->monitor == -1)
		prefs->monitor = gdk_screen_get_primary_monitor(screen);
#endif
}

static int _new_on_message(void * data, uint32_t value1, uint32_t value2,
		uint32_t value3)
{
	Panel * panel = data;
	PanelMessage message = value1;
	PanelMessageShow what;
	gboolean show;
	PanelPosition position;
	PanelWindow * window;

	switch(message)
	{
		case PANEL_MESSAGE_SHOW:
			what = value2;
			show = value3;
			if(what & PANEL_MESSAGE_SHOW_PANEL_BOTTOM)
			{
				position = PANEL_POSITION_BOTTOM;
				if((window = panel->windows[position]) != NULL)
					panel_window_show(window, show);
			}
			if(what & PANEL_MESSAGE_SHOW_PANEL_LEFT)
			{
				position = PANEL_POSITION_LEFT;
				if((window = panel->windows[position]) != NULL)
					panel_window_show(window, show);
			}
			if(what & PANEL_MESSAGE_SHOW_PANEL_RIGHT)
			{
				position = PANEL_POSITION_RIGHT;
				if((window = panel->windows[position]) != NULL)
					panel_window_show(window, show);
			}
			if(what & PANEL_MESSAGE_SHOW_PANEL_TOP)
			{
				position = PANEL_POSITION_TOP;
				if((window = panel->windows[position]) != NULL)
					panel_window_show(window, show);
			}
			if(what & PANEL_MESSAGE_SHOW_SETTINGS)
				panel_show_preferences(panel, show);
			break;
		case PANEL_MESSAGE_EMBED:
			/* ignore it (not meant to be handled here) */
			break;
	}
	return 0;
}

static GdkFilterReturn _on_root_event(GdkXEvent * xevent, GdkEvent * event,
		gpointer data)
{
	Panel * panel = data;
	XEvent * xe = xevent;
	(void) event;

	if(xe->type == ConfigureNotify)
		return _event_configure_notify(panel);
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn _event_configure_notify(Panel * panel)
{
	GdkRectangle rect;
	size_t i;

	_panel_reset(panel, &rect);
	for(i = 0; i < sizeof(panel->windows) / sizeof(*panel->windows); i++)
		if(panel->windows[i] != NULL)
			panel_window_reset(panel->windows[i], &rect);
	return GDK_FILTER_CONTINUE;
}


/* panel_delete */
void panel_delete(Panel * panel)
{
	size_t i;

	if(panel->timeout != 0)
		g_source_remove(panel->timeout);
	if(panel->source != 0)
		g_source_remove(panel->source);
	for(i = 0; i < sizeof(panel->windows) / sizeof(*panel->windows); i++)
		if(panel->windows[i] != NULL)
			panel_window_delete(panel->windows[i]);
	if(panel->config != NULL)
		config_delete(panel->config);
	object_delete(panel);
}


/* accessors */
/* panel_get_config */
char const * panel_get_config(Panel * panel, char const * section,
		char const * variable)
{
	/* XXX implement */
	return config_get(panel->config, section, variable);
}


/* useful */
/* panel_error */
static int _error_text(char const * message, int ret);
static gboolean _error_on_closex(GtkWidget * widget);
static void _error_on_response(GtkWidget * widget);

int panel_error(Panel * panel, char const * message, int ret)
{
	GtkWidget * dialog;

	if(panel == NULL)
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s", (message != NULL) ? message : error_get(NULL));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	g_signal_connect(dialog, "delete-event", G_CALLBACK(_error_on_closex),
			NULL);
	g_signal_connect(dialog, "response", G_CALLBACK(_error_on_response),
			NULL);
	gtk_widget_show_all(dialog);
	return ret;
}

static int _error_text(char const * message, int ret)
{
	if(message == NULL)
		error_print(PROGNAME);
	else
		fprintf(stderr, "%s: %s\n", PROGNAME, message);
	return ret;
}

static gboolean _error_on_closex(GtkWidget * widget)
{
	gtk_widget_hide(widget);
	return FALSE;
}

static void _error_on_response(GtkWidget * widget)
{
	gtk_widget_destroy(widget);
}


/* panel_load */
int panel_load(Panel * panel, PanelPosition position, char const * applet)
{
	int ret;
	PanelWindow * window = panel->windows[position];

	ret = panel_window_append(window, applet);
#if 0 /* FIXME re-implement */
	if(pad->settings != NULL
			&& (widget = pad->settings(pa, FALSE, FALSE)) != NULL)
	{
# if GTK_CHECK_VERSION(3, 0, 0)
		vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
# else
		vbox = gtk_vbox_new(FALSE, 4);
# endif
		/* XXX ugly */
		g_object_set_data(G_OBJECT(vbox), "definition", pad);
		g_object_set_data(G_OBJECT(vbox), "applet", pa);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
		gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);
		gtk_widget_show(vbox);
		gtk_notebook_append_page(GTK_NOTEBOOK(panel->pr_notebook),
				vbox, gtk_label_new(pad->name));
	}
#endif
	if(ret == 0)
		panel_window_show(window, TRUE);
	return (ret == 0) ? 0 : -1;
}


/* panel_reset */
static GtkIconSize _reset_iconsize(Panel * panel, String const * section);
static void _reset_window_delete(Panel * panel, PanelPosition position);
static int _reset_window_new(Panel * panel, PanelAppletHelper * helper,
		PanelPosition position, GtkIconSize iconsize,
		GdkRectangle * rect, gboolean focus, gboolean above);
/* callbacks */
static gboolean _reset_on_idle(gpointer data);
static void _reset_on_idle_load(Panel * panel, PanelPosition position);

int panel_reset(Panel * panel)
{
	GdkRectangle rect;
	PanelWindowPosition position;
	GtkIconSize iconsize;
	String const * section;
	gboolean focus;
	gboolean above;
	gboolean enabled;
	String const * applets;
	String const * p;
	String * s;

	_panel_reset(panel, &rect);
	focus = ((p = panel_get_config(panel, NULL, "accept_focus")) == NULL
			|| strcmp(p, "1") == 0) ? TRUE : FALSE;
	above = ((p = panel_get_config(panel, NULL, "keep_above")) == NULL
			|| strcmp(p, "1") == 0) ? TRUE : FALSE;
	for(position = 0; position < PANEL_POSITION_COUNT; position++)
	{
		if((section = _panel_get_section(panel, position)) == NULL
				|| (s = string_new_append("panel::", section,
						NULL)) == NULL)
			return -1;
		/* enabled */
		enabled = ((p = panel_get_config(panel, s, "enabled"))
				== NULL || strcmp(p, "1") == 0) ? TRUE : FALSE;
		iconsize = _reset_iconsize(panel, s);
		/* applets */
		if((applets = panel_get_config(panel, s, "applets")) != NULL
				&& string_length(applets) == 0)
			applets = NULL;
		string_delete(s);
		if(enabled == FALSE || applets == NULL)
		{
			_reset_window_delete(panel, position);
			continue;
		}
		if(_reset_window_new(panel, &panel->helpers[position], position,
					iconsize, &rect, focus, above) != 0)
			return -panel_error(NULL, NULL, 1);
	}
	/* create at least PANEL_POSITION_DEFAULT */
	for(position = 0; position < PANEL_POSITION_COUNT; position++)
		if(panel->windows[position] != NULL)
			break;
	if(position == PANEL_POSITION_COUNT)
	{
		position = PANEL_POSITION_DEFAULT;
		iconsize = _reset_iconsize(panel, NULL);
		if(_reset_window_new(panel, &panel->helpers[position], position,
					iconsize, &rect, focus, above) != 0)
			return -1;
	}
	/* load applets when idle */
	if(panel->source != 0)
		g_source_remove(panel->source);
	panel->source = g_idle_add(_reset_on_idle, panel);
	return 0;
}

static GtkIconSize _reset_iconsize(Panel * panel, String const * section)
{
	GtkIconSize ret = GTK_ICON_SIZE_INVALID;
	String const * p = NULL;

	p = panel_get_config(panel, section, "size");
	if(p == NULL && section != NULL)
		p = panel_get_config(panel, NULL, "size");
	if(p != NULL)
		ret = gtk_icon_size_from_name(p);
	if(ret == GTK_ICON_SIZE_INVALID)
		ret = GTK_ICON_SIZE_SMALL_TOOLBAR;
	return ret;
}

static int _reset_window_new(Panel * panel, PanelAppletHelper * helper,
		PanelPosition position, GtkIconSize iconsize,
		GdkRectangle * rect, gboolean focus, gboolean above)
{
	if(panel->windows[position] == NULL
			&& (panel->windows[position] = panel_window_new(helper,
					PANEL_WINDOW_TYPE_NORMAL, position,
					iconsize, rect)) == NULL)
		return -1;
	panel->helpers[position].window = panel->windows[position];
	panel_window_set_accept_focus(panel->windows[position], focus);
	panel_window_set_keep_above(panel->windows[position], above);
	return 0;
}

static void _reset_window_delete(Panel * panel, PanelPosition position)
{
	if(panel->windows[position] != NULL)
		panel_window_delete(panel->windows[position]);
	panel->windows[position] = NULL;
	panel->helpers[position].window = NULL;
}

static gboolean _reset_on_idle(gpointer data)
{
	Panel * panel = data;
	PanelPosition position;

	panel->source = 0;
	if(panel->pr_window == NULL)
		panel_show_preferences(panel, FALSE);
	for(position = 0; position < PANEL_POSITION_COUNT; position++)
		if(panel->windows[position] != NULL)
			_reset_on_idle_load(panel, position);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->pr_notebook), 0);
	return FALSE;
}

static void _reset_on_idle_load(Panel * panel, PanelPosition position)
{
	char const * applets;
	char * p;
	char * q;
	size_t i;

	if((applets = _panel_get_applets(panel, position)) == NULL
			|| strlen(applets) == 0)
		return;
	if((p = string_new(applets)) == NULL)
	{
		panel_error(panel, NULL, FALSE);
		return;
	}
	for(q = p, i = 0;;)
	{
		if(q[i] == '\0')
		{
			if(panel_load(panel, position, q) != 0)
				/* ignore errors */
				error_print(PROGNAME);
			break;
		}
		if(q[i++] != ',')
			continue;
		q[i - 1] = '\0';
		if(panel_load(panel, position, q) != 0)
			/* ignore errors */
			error_print(PROGNAME);
		q += i;
		i = 0;
	}
	free(p);
}


/* panel_save */
int panel_save(Panel * panel)
{
	int ret;
	String * filename;

	if((filename = _config_get_filename()) == NULL)
		return -1;
	ret = config_save(panel->config, filename);
	string_delete(filename);
	return ret;
}


/* panel_show_preferences */
static void _show_preferences_window(Panel * panel);
static GtkWidget * _preferences_window_general(Panel * panel);
static GtkWidget * _preferences_window_panel(Panel * panel);
static GtkWidget * _preferences_window_panels(Panel * panel);
static void _preferences_window_panels_add(GtkListStore * store,
		char const * name);
static GtkListStore * _preferences_window_panels_model(void);
static GtkWidget * _preferences_window_panels_view(GtkListStore * store,
		gboolean reorderable);
static gboolean _preferences_on_closex(gpointer data);
static void _preferences_on_response(GtkWidget * widget, gint response,
		gpointer data);
static void _preferences_on_response_apply(gpointer data);
static void _preferences_on_response_apply_panel(Panel * panel,
		PanelPosition position);
static void _preferences_on_response_cancel(gpointer data);
static void _cancel_general(Panel * panel);
static void _cancel_applets(Panel * panel);
static void _preferences_on_panel_toggled(gpointer data);
static void _preferences_on_response_ok(gpointer data);
static void _preferences_on_panel_add(gpointer data);
static void _preferences_on_panel_changed(gpointer data);
static void _preferences_on_panel_down(gpointer data);
static void _preferences_on_panel_remove(gpointer data);
static void _preferences_on_panel_up(gpointer data);

void panel_show_preferences(Panel * panel, gboolean show)
{
	if(panel->pr_window == NULL)
		_show_preferences_window(panel);
	if(show == FALSE)
		gtk_widget_hide(panel->pr_window);
	else
		gtk_window_present(GTK_WINDOW(panel->pr_window));
}

static void _show_preferences_window(Panel * panel)
{
	GtkWidget * vbox;
	GtkWidget * widget;

	panel->pr_window = gtk_dialog_new_with_buttons(_("Panel preferences"),
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
			GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_window_set_default_size(GTK_WINDOW(panel->pr_window), 400, 300);
	gtk_window_set_position(GTK_WINDOW(panel->pr_window),
			GTK_WIN_POS_CENTER);
	g_signal_connect_swapped(panel->pr_window, "delete-event", G_CALLBACK(
				_preferences_on_closex), panel);
	g_signal_connect(panel->pr_window, "response", G_CALLBACK(
				_preferences_on_response), panel);
	panel->pr_notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(panel->pr_notebook), TRUE);
	/* applets */
	widget = _preferences_window_general(panel);
	gtk_notebook_append_page(GTK_NOTEBOOK(panel->pr_notebook), widget,
			gtk_label_new(_("General")));
	/* panels */
	widget = _preferences_window_panels(panel);
	gtk_notebook_append_page(GTK_NOTEBOOK(panel->pr_notebook), widget,
			gtk_label_new(_("Panels")));
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(panel->pr_window));
#else
	vbox = GTK_DIALOG(panel->pr_window)->vbox;
#endif
	gtk_box_pack_start(GTK_BOX(vbox), panel->pr_notebook, TRUE, TRUE, 0);
	_preferences_on_response_cancel(panel);
	gtk_widget_show_all(vbox);
}

static GtkWidget * _preferences_window_general(Panel * panel)
{
	GtkWidget * vbox;

#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox = gtk_vbox_new(FALSE, 4);
#endif
	panel->pr_accept_focus = gtk_check_button_new_with_mnemonic(
			_("Accept focus"));
	gtk_box_pack_start(GTK_BOX(vbox), panel->pr_accept_focus, FALSE, TRUE,
			0);
	panel->pr_keep_above = gtk_check_button_new_with_mnemonic(
			_("Keep above other windows"));
	gtk_box_pack_start(GTK_BOX(vbox), panel->pr_keep_above, FALSE, TRUE, 0);
	return vbox;
}

static GtkWidget * _preferences_window_panel(Panel * panel)
{
	const char * titles[PANEL_POSITION_COUNT] = {
		N_("Bottom panel:"), N_("Top panel:"),
		N_("Left panel:"), N_("Right panel:")
	};
	GtkWidget * frame;
	GtkWidget * vbox3;
	GtkWidget * widget;
	size_t i;
	size_t j;

	frame = gtk_frame_new(NULL);
#if GTK_CHECK_VERSION(2, 24, 0)
	widget = gtk_combo_box_text_new();
#else
	widget = gtk_combo_box_new_text();
#endif
	panel->pr_panels_panel = widget;
	gtk_frame_set_label_widget(GTK_FRAME(frame), widget);
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox3 = gtk_vbox_new(FALSE, 4);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), 4);
	for(i = 0; i < sizeof(titles) / sizeof(*titles); i++)
	{
#if GTK_CHECK_VERSION(2, 24, 0)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget),
				_(titles[i]));
#else
		gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _(titles[i]));
#endif
		/* enabled */
		panel->pr_panels[i].enabled = gtk_check_button_new_with_mnemonic(
				_("_Enabled"));
		g_signal_connect_swapped(panel->pr_panels[i].enabled, "toggled",
				G_CALLBACK(_preferences_on_panel_toggled),
				panel);
		gtk_box_pack_start(GTK_BOX(vbox3), panel->pr_panels[i].enabled,
				FALSE, TRUE, 0);
		gtk_widget_set_no_show_all(panel->pr_panels[i].enabled, TRUE);
		/* size */
#if GTK_CHECK_VERSION(2, 24, 0)
		panel->pr_panels[i].size = gtk_combo_box_text_new();
		gtk_combo_box_text_append_text(
				GTK_COMBO_BOX_TEXT(panel->pr_panels[i].size),
				_("Default"));
#else
		panel->pr_panels[i].size = gtk_combo_box_new_text();
		gtk_combo_box_append_text(
				GTK_COMBO_BOX(panel->pr_panels[i].size),
				_("Default"));
#endif
		for(j = 0; j < sizeof(_panel_sizes) / sizeof(*_panel_sizes);
				j++)
		{
#if GTK_CHECK_VERSION(2, 24, 0)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(
						panel->pr_panels[i].size),
					_(_panel_sizes[j].alias));
#else
			gtk_combo_box_append_text(
					GTK_COMBO_BOX(panel->pr_panels[i].size),
					_(_panel_sizes[j].alias));
#endif
		}
		gtk_widget_set_no_show_all(panel->pr_panels[i].size, TRUE);
		gtk_box_pack_start(GTK_BOX(vbox3), panel->pr_panels[i].size,
				FALSE, TRUE, 0);
		panel->pr_panels[i].store = _preferences_window_panels_model();
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
	g_signal_connect_swapped(widget, "changed", G_CALLBACK(
				_preferences_on_panel_changed), panel);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(widget),
			GTK_SHADOW_ETCHED_IN);
	panel->pr_panels_view = _preferences_window_panels_view(NULL, TRUE);
	gtk_container_add(GTK_CONTAINER(widget), panel->pr_panels_view);
	gtk_box_pack_start(GTK_BOX(vbox3), widget, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox3);
	return frame;
}

static GtkWidget * _preferences_window_panels(Panel * panel)
{
	GtkWidget * vbox;
	GtkWidget * vbox2;
	GtkWidget * hbox;
	GtkWidget * frame;
	GtkWidget * widget;

#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox = gtk_vbox_new(FALSE, 4);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	/* applets */
	frame = gtk_frame_new(_("Applets:"));
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(widget), 4);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(widget),
			GTK_SHADOW_ETCHED_IN);
	panel->pr_store = _preferences_window_panels_model();
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(panel->pr_store),
			2, GTK_SORT_ASCENDING);
	panel->pr_view = _preferences_window_panels_view(panel->pr_store,
			FALSE);
	gtk_container_add(GTK_CONTAINER(widget), panel->pr_view);
	gtk_container_add(GTK_CONTAINER(frame), widget);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
	/* controls */
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox2 = gtk_vbox_new(FALSE, 4);
#endif
	/* remove */
	widget = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(widget),
#if GTK_CHECK_VERSION(3, 10, 0)
			gtk_image_new_from_icon_name(
#else
			gtk_image_new_from_stock(
#endif
				GTK_STOCK_DELETE, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(
				_preferences_on_panel_remove), panel);
	gtk_box_pack_end(GTK_BOX(vbox2), widget, FALSE, TRUE, 0);
#ifndef EMBEDDED
	/* bottom */
	widget = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(widget),
#if GTK_CHECK_VERSION(3, 10, 0)
			gtk_image_new_from_icon_name(
#else
			gtk_image_new_from_stock(
#endif
				GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(
				_preferences_on_panel_down), panel);
	gtk_box_pack_end(GTK_BOX(vbox2), widget, FALSE, TRUE, 0);
	/* top */
	widget = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(widget),
#if GTK_CHECK_VERSION(3, 10, 0)
			gtk_image_new_from_icon_name(
#else
			gtk_image_new_from_stock(
#endif
				GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(
				_preferences_on_panel_up), panel);
	gtk_box_pack_end(GTK_BOX(vbox2), widget, FALSE, TRUE, 0);
#endif
	/* add */
	widget = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(widget),
#if GTK_CHECK_VERSION(3, 10, 0)
			gtk_image_new_from_icon_name(
#else
			gtk_image_new_from_stock(
#endif
				GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON));
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(
				_preferences_on_panel_add), panel);
	gtk_box_pack_end(GTK_BOX(vbox2), widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
	vbox2 = gtk_vbox_new(FALSE, 4);
#endif
	/* panel applets */
	frame = _preferences_window_panel(panel);
	gtk_box_pack_start(GTK_BOX(vbox2), frame, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	return vbox;
}

static void _preferences_window_panels_add(GtkListStore * store,
		char const * name)
{
	Plugin * p;
	PanelAppletDefinition * pad;
	GtkTreeIter iter;
	GtkIconTheme * theme;
	GdkPixbuf * pixbuf = NULL;

	if((p = plugin_new(LIBDIR, PACKAGE, "applets", name)) == NULL)
		return;
	if((pad = plugin_lookup(p, "applet")) == NULL)
	{
		plugin_delete(p);
		return;
	}
	theme = gtk_icon_theme_get_default();
	if(pad->icon != NULL)
		pixbuf = gtk_icon_theme_load_icon(theme, pad->icon, 24, 0,
				NULL);
	if(pixbuf == NULL)
		pixbuf = gtk_icon_theme_load_icon(theme, "gnome-settings", 24,
				0, NULL);
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, name, 1, pixbuf, 2, pad->name, -1);
	plugin_delete(p);
}

static GtkListStore * _preferences_window_panels_model(void)
{
	return gtk_list_store_new(3, G_TYPE_STRING,	/* name */
			GDK_TYPE_PIXBUF,		/* icon */
			G_TYPE_STRING);			/* full name */
}

static GtkWidget * _preferences_window_panels_view(GtkListStore * store,
		gboolean reorderable)
{
	GtkWidget * view;
	GtkTreeSelection * treesel;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	view = (store != NULL)
		? gtk_tree_view_new_with_model(GTK_TREE_MODEL(store))
		: gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), reorderable);
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer,
			"pixbuf", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer,
			"text", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	return view;
}

static gboolean _preferences_on_closex(gpointer data)
{
	Panel * panel = data;

	_preferences_on_response_cancel(panel);
	return TRUE;
}

static void _preferences_on_response(GtkWidget * widget, gint response,
		gpointer data)
{
	if(response == GTK_RESPONSE_APPLY)
		_preferences_on_response_apply(data);
	else
	{
		if(response == GTK_RESPONSE_OK)
			_preferences_on_response_ok(data);
		else if(response == GTK_RESPONSE_CANCEL)
			_preferences_on_response_cancel(data);
		gtk_widget_hide(widget);
	}
}

static void _preferences_on_response_apply(gpointer data)
{
	Panel * panel = data;
	gint i;
	gint cnt;
	GtkWidget * widget;
	PanelAppletDefinition * pad;
	PanelApplet * pa;
	size_t j;

	/* general */
	if(config_set(panel->config, NULL, "accept_focus",
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						panel->pr_accept_focus))
				? "1" : "0") != 0)
		panel_error(NULL, NULL, 1);
	if(config_set(panel->config, NULL, "keep_above",
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						panel->pr_keep_above))
				? "1" : "0") != 0)
		panel_error(NULL, NULL, 1);
	/* panels */
	for(j = 0; j < sizeof(panel->pr_panels) / sizeof(*panel->pr_panels);
			j++)
		_preferences_on_response_apply_panel(panel, j);
	/* XXX applets should be known from Panel already */
	cnt = gtk_notebook_get_n_pages(GTK_NOTEBOOK(panel->pr_notebook));
	for(i = 1; i < cnt; i++)
	{
		widget = gtk_notebook_get_nth_page(GTK_NOTEBOOK(
					panel->pr_notebook), i);
		if(widget == NULL || (pad = g_object_get_data(G_OBJECT(widget),
						"definition")) == NULL
				|| (pa = g_object_get_data(G_OBJECT(widget),
						"applet")) == NULL)
			continue;
		pad->settings(pa, TRUE, FALSE);
	}
	/* remove the applets from every active panel */
	for(j = 0; j < sizeof(panel->windows) / sizeof(*panel->windows); j++)
		if(panel->windows[j] != NULL)
			panel_window_remove_all(panel->windows[j]);
	panel_reset(panel);
}

static void _preferences_on_response_apply_panel(Panel * panel,
		PanelPosition position)
{
	char const * section;
	gint i;
	const gint cnt = sizeof(_panel_sizes) / sizeof(*_panel_sizes);
	GtkTreeModel * model;
	GtkTreeIter iter;
	gboolean valid;
	gboolean enabled;
	gchar * p;
	String * value;
	String * sep;
	String * s;

	section = _panel_get_section(panel, position);
	if((s = string_new_append("panel::", section, NULL)) == NULL)
	{
		panel_error(NULL, NULL, 1);
		return;
	}
	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				panel->pr_panels[position].enabled));
	if(config_set(panel->config, s, "enabled", enabled ? "1" : "0") != 0)
		panel_error(NULL, NULL, 1);
	i = gtk_combo_box_get_active(
			GTK_COMBO_BOX(panel->pr_panels[position].size));
	if(i >= 0 && i <= cnt)
		if(config_set(panel->config, s, "size", (i > 0)
					? _panel_sizes[i - 1].name : NULL) != 0)
			panel_error(NULL, NULL, 1);
	model = GTK_TREE_MODEL(panel->pr_panels[position].store);
	value = NULL;
	sep = "";
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, 0, &p, -1);
		string_append(&value, sep);
		string_append(&value, p);
		sep = ",";
		g_free(p);
	}
	if(config_set(panel->config, s, "applets", value) != 0)
		panel_error(NULL, NULL, 1);
	string_delete(value);
	string_delete(s);
}

static void _preferences_on_response_cancel(gpointer data)
{
	Panel * panel = data;
	size_t i;
	size_t cnt = sizeof(_panel_sizes) / sizeof(*_panel_sizes);
	GtkWidget * widget;
	PanelAppletDefinition * pad;
	PanelApplet * pa;

	gtk_widget_hide(panel->pr_window);
	/* general */
	_cancel_general(panel);
	/* applets */
	_cancel_applets(panel);
	/* XXX applets should be known from Panel already */
	cnt = gtk_notebook_get_n_pages(GTK_NOTEBOOK(panel->pr_notebook));
	for(i = 1; i < cnt; i++)
	{
		widget = gtk_notebook_get_nth_page(GTK_NOTEBOOK(
					panel->pr_notebook), i);
		if(widget == NULL || (pad = g_object_get_data(G_OBJECT(widget),
						"definition")) == NULL
				|| (pa = g_object_get_data(G_OBJECT(widget),
						"applet")) == NULL)
			continue;
		pad->settings(pa, FALSE, TRUE);
	}
}

static void _cancel_applets(Panel * panel)
{
	DIR * dir;
	struct dirent * de;
#ifdef __APPLE__
	char const ext[] = ".dylib";
#else
	char const ext[] = ".so";
#endif
	size_t len;
	String const * section;
	String * s;
	char const * p;
	char * q;
	gboolean enabled;
	char c;
	size_t i;
	size_t j;
	const size_t cnt = sizeof(_panel_sizes) / sizeof(*_panel_sizes);

	gtk_list_store_clear(panel->pr_store);
	for(j = 0; j < sizeof(panel->pr_panels) / sizeof(*panel->pr_panels);
			j++)
		gtk_list_store_clear(panel->pr_panels[j].store);
	if((dir = opendir(LIBDIR "/" PACKAGE "/applets")) == NULL)
		return;
	/* applets */
	while((de = readdir(dir)) != NULL)
	{
		if((len = strlen(de->d_name)) < sizeof(ext))
			continue;
		if(strcmp(&de->d_name[len - sizeof(ext) + 1], ext) != 0)
			continue;
		de->d_name[len - sizeof(ext) + 1] = '\0';
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, de->d_name);
#endif
		_preferences_window_panels_add(panel->pr_store, de->d_name);
	}
	closedir(dir);
	/* panels */
	for(j = 0; j < sizeof(panel->pr_panels) / sizeof(*panel->pr_panels);
			j++)
	{
		section = _panel_get_section(panel, j);
		if((s = string_new_append("panel::", section, NULL)) == NULL)
		{
			panel_error(NULL, NULL, 1);
			continue;
		}
		/* enabled */
		enabled = ((p = panel_get_config(panel, s, "enabled")) != NULL
				&& strtol(p, NULL, 0) != 0) ? TRUE : FALSE;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
					panel->pr_panels[j].enabled), enabled);
		/* applets */
		p = _panel_get_applets(panel, j);
		q = (p != NULL) ? strdup(p) : NULL;
		for(i = 0, p = q; q != NULL; i++)
		{
			if(q[i] != '\0' && q[i] != ',')
				continue;
			c = q[i];
			q[i] = '\0';
			_preferences_window_panels_add(
					panel->pr_panels[j].store, p);
			if(c == '\0')
				break;
			p = &q[i + 1];
		}
		free(q);
		/* size */
		if((p = panel_get_config(panel, s, "size")) == NULL
				&& (p = panel_get_config(panel, NULL, "size"))
				== NULL)
			gtk_combo_box_set_active(GTK_COMBO_BOX(
						panel->pr_panels[j].size), 0);
		else
			for(i = 0; i < cnt; i++)
			{
				if(strcmp(p, _panel_sizes[i].name) != 0)
					continue;
				gtk_combo_box_set_active(GTK_COMBO_BOX(
							panel->pr_panels[j].size),
						i + 1);
				break;
			}
		string_delete(s);
	}
	_preferences_on_panel_changed(panel);
}

static void _cancel_general(Panel * panel)
{
	char const * p;
	gboolean b;

	b = ((p = panel_get_config(panel, NULL, "accept_focus")) == NULL
			|| strcmp(p, "1") == 0) ? TRUE : FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->pr_accept_focus),
			b);
	b = ((p = panel_get_config(panel, NULL, "keep_above")) == NULL
			|| strcmp(p, "1") == 0) ? TRUE : FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->pr_keep_above),
			b);
}

static void _preferences_on_panel_toggled(gpointer data)
{
	Panel * panel = data;
	PanelPosition position;
	size_t i;
	gboolean active;

	position = gtk_combo_box_get_active(GTK_COMBO_BOX(
				panel->pr_panels_panel));
	for(i = 0; i < sizeof(panel->pr_panels) / sizeof(*panel->pr_panels);
			i++)
	{
		active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					panel->pr_panels[i].enabled));
		gtk_widget_set_sensitive(panel->pr_panels[i].size, active);
		if(i == position)
			gtk_widget_set_sensitive(panel->pr_panels_view, active);
	}
}

static void _preferences_on_response_ok(gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(panel->pr_window);
	_preferences_on_response_apply(panel);
	panel_save(panel);
}

static void _preferences_on_panel_add(gpointer data)
{
	Panel * panel = data;
	PanelPosition position;
	GtkTreeModel * model;
	GtkTreeIter iter;
	GtkTreeSelection * treesel;
	gchar * p;

	position = gtk_combo_box_get_active(GTK_COMBO_BOX(
				panel->pr_panels_panel));
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(panel->pr_view));
	if(!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, 0, &p, -1);
	_preferences_window_panels_add(panel->pr_panels[position].store, p);
	g_free(p);
}

static void _preferences_on_panel_changed(gpointer data)
{
	Panel * panel = data;
	PanelPosition position;
	size_t i;
	gboolean active;

	position = gtk_combo_box_get_active(GTK_COMBO_BOX(
				panel->pr_panels_panel));
	for(i = 0; i < PANEL_POSITION_COUNT; i++)
		if(i == position)
		{
			gtk_widget_show(panel->pr_panels[i].enabled);
			gtk_widget_show(panel->pr_panels[i].size);
		}
		else
		{
			gtk_widget_hide(panel->pr_panels[i].enabled);
			gtk_widget_hide(panel->pr_panels[i].size);
		}
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				panel->pr_panels[position].enabled));
	gtk_widget_set_sensitive(panel->pr_panels_view, active);
	gtk_tree_view_set_model(GTK_TREE_VIEW(panel->pr_panels_view),
			GTK_TREE_MODEL(panel->pr_panels[position].store));
}

static void _preferences_on_panel_down(gpointer data)
{
	Panel * panel = data;
	PanelPosition position;
	GtkTreeModel * model;
	GtkTreeIter iter;
	GtkTreeIter iter2;
	GtkTreeSelection * treesel;

	position = gtk_combo_box_get_active(GTK_COMBO_BOX(
				panel->pr_panels_panel));
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				panel->pr_panels_view));
	if(!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;
	iter2 = iter;
	if(!gtk_tree_model_iter_next(model, &iter))
		return;
	gtk_list_store_swap(panel->pr_panels[position].store, &iter, &iter2);
}

static void _preferences_on_panel_remove(gpointer data)
{
	Panel * panel = data;
	GtkTreeModel * model;
	GtkTreeIter iter;
	GtkTreeSelection * treesel;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				panel->pr_panels_view));
	if(gtk_tree_selection_get_selected(treesel, &model, &iter))
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}

static void _preferences_on_panel_up(gpointer data)
{
	Panel * panel = data;
	PanelPosition position;
	GtkTreeModel * model;
	GtkTreeIter iter;
	GtkTreeIter iter2;
	GtkTreePath * path;
	GtkTreeSelection * treesel;

	position = gtk_combo_box_get_active(GTK_COMBO_BOX(
				panel->pr_panels_panel));
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				panel->pr_panels_view));
	if(!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;
	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_path_prev(path);
	gtk_tree_model_get_iter(model, &iter2, path);
	gtk_tree_path_free(path);
	gtk_list_store_swap(panel->pr_panels[position].store, &iter, &iter2);
}


/* private */
/* functions */
/* config_get_filename */
static char * _config_get_filename(void)
{
	char const * homedir;

	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	return string_new_format("%s/%s", homedir, PANEL_CONFIG_FILE);
}


/* accessors */
/* panel_can_suspend */
static gboolean _panel_can_suspend(void)
{
#ifdef __NetBSD__
	char const * names[] = { "machdep.sleep_state", "hw.acpi.sleep.state" };
	int sleep_state = -1;
	size_t size = sizeof(sleep_state);

	/* FIXME check that this works properly */
	if(sysctlbyname(names[0], &sleep_state, &size, NULL, 0) == 0
			&& sleep_state == 0
			&& sysctlbyname(names[0], &sleep_state, &size,
				&sleep_state, size) == 0)
		return TRUE;
	if(sysctlbyname(names[1], &sleep_state, &size, NULL, 0) == 0
			&& sleep_state == 0
			&& sysctlbyname(names[1], &sleep_state,
				&size, &sleep_state, size) == 0)
		return TRUE;
#else
	struct stat st;

	if(access("/sys/power/state", W_OK) == 0)
		return TRUE;
	if(lstat("/proc/apm", &st) == 0)
		return TRUE;
#endif
	return FALSE;
}


/* panel_get_applets */
static char const * _panel_get_applets(Panel * panel, PanelPosition position)
{
#ifndef EMBEDDED
	char const * applets = "menu,desktop,lock,logout,pager,tasks"
		",gsm,gps,bluetooth,battery,cpufreq,volume,embed,systray,clock";
#else /* EMBEDDED */
	/* XXX the "keyboard" applet is in a separate repository now */
	char const * applets = "menu,desktop,keyboard,tasks,spacer"
		",gsm,gps,bluetooth,battery,cpufreq,volume,embed,systray,clock"
		",close";
#endif
	String const * section;
	String * s;
	String const * p = NULL;

	section = _panel_get_section(panel, position);
	if((s = string_new_append("panel::", section, NULL)) == NULL)
		return NULL;
	switch(position)
	{
		case PANEL_POSITION_LEFT:
		case PANEL_POSITION_RIGHT:
		case PANEL_POSITION_TOP:
			p = panel_get_config(panel, s, "applets");
			break;
		case PANEL_POSITION_BOTTOM:
			p = panel_get_config(panel, s, "applets");
			if(p == NULL)
				p = panel_get_config(panel, NULL, "applets");
			if(p == NULL)
				p = applets;
			break;
	}
	string_delete(s);
	return p;
}


/* panel_get_section */
static char const * _panel_get_section(Panel * panel, PanelPosition position)
{
	char const * sections[PANEL_POSITION_COUNT] = {
		"bottom", "top", "left", "right" };
	(void) panel;

	return sections[position];
}


/* useful */
/* panel_reset */
static void _panel_reset(Panel * panel, GdkRectangle * rect)
{
	gdk_screen_get_monitor_geometry(panel->screen, (panel->prefs.monitor > 0
				&& panel->prefs.monitor
				< gdk_screen_get_n_monitors(panel->screen))
			? panel->prefs.monitor : 0, rect);
	panel->root_height = rect->height;
	panel->root_width = rect->width;
}
