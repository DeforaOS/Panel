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



#include <System.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "panel.h"


/* Panel */
/* private */
/* types */
typedef struct _PanelMenu
{
	char const * category;
	char const * label;
	char const * stock;
} PanelMenu;

struct _Panel
{
	GSList * apps;

	GdkWindow * root;
	GtkWidget * window;
	GtkWidget * clock;

	gint width;			/* width of the root window	*/
	gint height;			/* height of the root window	*/
};


/* constants */
#ifndef PREFIX
# define PREFIX "/usr/local"
#endif
#define PANEL_BORDER_WIDTH		4
#define PANEL_ICON_SIZE			48

static const PanelMenu _panel_menus[] =
{
	{ "Audio;",	"Audio",	NULL,				},
	{ "Development;","Development",	"applications-development",	},
	{ "Education;",	"Education",	NULL,				},
	{ "Game;",	"Games",	"applications-games",		},
	{ "Graphics;",	"Graphics",	"applications-graphics",	},
	{ "AudioVideo;","Multimedia",	"applications-multimedia",	},
	{ "Network;",	"Network",	"applications-internet",	},
	{ "Office;",	"Office",	"applications-office",		},
	{ "Settings;",	"Settings",	"gnome-settings",		},
	{ "System;",	"System",	"applications-system",		},
	{ "Utility;",	"Utilities",	"applications-utilities",	},
	{ "Video;",	"Video",	"video",			},
	{ NULL,		NULL,		NULL,				}
};
#define PANEL_MENUS_CNT (sizeof(_panel_menus) / sizeof(*_panel_menus))


/* prototypes */
static int _panel_exec(Panel * panel, char const * command);
static gboolean _panel_idle_apps(gpointer data);
static GtkWidget * _panel_image(char const * name);
static GtkWidget * _panel_menuitem(char const * label, char const * stock);


/* public */
/* panel_new */
static GtkWidget * _new_button(char const * stock);
static gboolean _on_button_press(GtkWidget * widget, GdkEventButton * event,
		gpointer data);
static void _on_lock(GtkWidget * widget, gpointer data);
static void _on_logout(GtkWidget * widget, gpointer data);
static void _on_menu(GtkWidget * widget, gpointer data);
static void _on_menu_position(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data);
static void _on_run(GtkWidget * widget, gpointer data);
static gboolean _on_timeout_clock(gpointer data);

Panel * panel_new(void)
{
	Panel * panel;
	GtkWidget * event;
	GtkWidget * hbox;
	GtkWidget * widget;
	gint x;
	gint y;
	gint depth;

	if((panel = malloc(sizeof(*panel))) == NULL)
	{
		/* FIXME visually warn the user */
		panel_error(NULL, "malloc", 1);
		return NULL;
	}
	/* applications */
	panel->apps = NULL;
	g_idle_add(_panel_idle_apps, panel);
	/* root window */
	panel->root = gdk_screen_get_root_window(
			gdk_display_get_default_screen(
				gdk_display_get_default()));
	gdk_window_get_geometry(panel->root, &x, &y, &panel->width,
			&panel->height, &depth);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() x=%d y=%d width=%d height=%d depth=%d\n",
			__func__, x, y, panel->width, panel->height, depth);
#endif
	/* panel */
	panel->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_move(GTK_WINDOW(panel->window), 0, panel->height
		- PANEL_ICON_SIZE - (PANEL_BORDER_WIDTH * 2));
	gtk_window_set_default_size(GTK_WINDOW(panel->window), panel->width,
			PANEL_ICON_SIZE + (PANEL_BORDER_WIDTH * 2));
	gtk_window_set_type_hint(GTK_WINDOW(panel->window),
			GDK_WINDOW_TYPE_HINT_DOCK);
	event = gtk_event_box_new();
	g_signal_connect(G_OBJECT(event), "button-press-event", G_CALLBACK(
				_on_button_press), panel);
	hbox = gtk_hbox_new(FALSE, 4);
	/* main menu */
	widget = _new_button("gnome-main-menu");
	g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(_on_menu),
			panel);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	/* quick launch */
	widget = _new_button("gnome-lockscreen");
	g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(_on_lock),
			panel);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	widget = _new_button("gnome-logout");
	g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(_on_logout),
			panel);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	/* clock */
	widget = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(widget), GTK_SHADOW_IN);
	panel->clock = gtk_label_new(" \n ");
	gtk_label_set_justify(GTK_LABEL(panel->clock), GTK_JUSTIFY_CENTER);
	g_timeout_add(1000, _on_timeout_clock, panel);
	_on_timeout_clock(panel);
	gtk_container_add(GTK_CONTAINER(widget), panel->clock);
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(event), hbox);
	gtk_container_add(GTK_CONTAINER(panel->window), event);
	gtk_container_set_border_width(GTK_CONTAINER(panel->window), 4);
	gtk_widget_show_all(panel->window);
	return panel;
}

static GtkWidget * _new_button(char const * stock)
{
	GtkWidget * button;
	GtkWidget * image;

	button = gtk_button_new();
	image = gtk_image_new_from_icon_name(stock,
			GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	return button;
}

static gboolean _on_button_press(GtkWidget * widget, GdkEventButton * event,
		gpointer data)
{
	GtkWidget * menu;
	GtkWidget * menuitem;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() right-click\n", __func__);
#endif
	menu = gtk_menu_new();
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PROPERTIES,
			NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, data, event->button,
			event->time);
	return FALSE;
}

static void _on_lock(GtkWidget * widget, gpointer data)
{
	Panel * panel = data;

	_panel_exec(panel, "xscreensaver-command -lock");
}

static void _on_logout(GtkWidget * widget, gpointer data)
{
	GtkWidget * dialog;
	const char message[] = "This will log you out of the current session,"
		" therefore closing any application currently opened and losing"
		" any unsaved data.\nDo you really want to proceed?";
	int res;

	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_WARNING,
			GTK_BUTTONS_NONE, "%s", message);
	gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, "Logout", GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if(res != GTK_RESPONSE_ACCEPT)
		return;
	gtk_main_quit();
}

static GtkWidget * _menu_applications(Panel * panel);
static void _applications_on_activate(GtkWidget * widget, gpointer data);
static void _applications_categories(GtkWidget * menu, GtkWidget ** menus);
static void _on_menu(GtkWidget * widget, gpointer data)
{
	Panel * panel = data;
	GtkWidget * menu;
	GtkWidget * menuitem;

	menu = gtk_menu_new();
	menuitem = _panel_menuitem("Applications", "gnome-applications");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem),
			_menu_applications(panel));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = _panel_menuitem("Run...", GTK_STOCK_EXECUTE);
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(_on_run),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES,
			NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = _panel_menuitem("Lock screen", "gnome-lockscreen");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(_on_lock),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = _panel_menuitem("Logout...", "gnome-logout");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(_on_logout),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, _on_menu_position, data, 0,
			gtk_get_current_event_time());
}

static GtkWidget * _menu_applications(Panel * panel)
{
	GtkWidget * menus[PANEL_MENUS_CNT];
	GSList * p;
	GtkWidget * menu;
	GtkWidget * menuitem;
	Config * config;
	const char section[] = "Desktop Entry";
	char const * q;
	size_t i;

	_panel_idle_apps(panel); /* just in case */
	memset(menus, 0, sizeof(menus));
	menu = gtk_menu_new();
	for(p = panel->apps; p != NULL; p = p->next)
	{
		config = p->data;
		q = config_get(config, section, "Name"); /* should not fail */
		menuitem = _panel_menuitem(q, config_get(config, section,
					"Icon"));
		q = config_get(config, section, "Exec"); /* should not fail */
		g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(
					_applications_on_activate),
				(gpointer)q);
		if((q = config_get(config, section, "Categories")) == NULL)
		{
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
			continue;
		}
		for(i = 0; _panel_menus[i].category != NULL && string_find(q,
					_panel_menus[i].category) == NULL; i++);
		if(_panel_menus[i].category == NULL)
		{
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
			continue;
		}
		if(menus[i] == NULL)
			menus[i] = gtk_menu_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menus[i]), menuitem);
	}
	_applications_categories(menu, menus);
	return menu;
}

static void _applications_on_activate(GtkWidget * widget, gpointer data)
{
	char const * program = data;

	if(program == NULL)
		return;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"", __func__, program);
#endif
	_panel_exec(NULL, program);
}

static void _applications_categories(GtkWidget * menu, GtkWidget ** menus)
{
	size_t i;
	const PanelMenu * m;
	GtkWidget * menuitem;
	size_t pos = 0;

	for(i = 0; _panel_menus[i].category != NULL; i++)
	{
		if(menus[i] == NULL)
			continue;
		m = &_panel_menus[i];
		menuitem = _panel_menuitem(m->label, m->stock);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menus[i]);
		gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, pos++);
	}
}

static void _on_menu_position(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data)
{
	Panel * panel = data;
	GtkRequisition req;

	gtk_widget_size_request(GTK_WIDGET(menu), &req);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() width=%d, height=%d\n", __func__,
			req.width, req.height);
#endif
	if(req.height <= 0)
		return;
	*x = PANEL_BORDER_WIDTH;
	*y = panel->height - PANEL_BORDER_WIDTH - PANEL_ICON_SIZE - req.height;
	*push_in = TRUE;
}

static void _on_run(GtkWidget * widget, gpointer data)
{
	Panel * panel = data;

	_panel_exec(panel, "run");
}

static gboolean _on_timeout_clock(gpointer data)
{
	Panel * panel = data;
	struct timeval tv;
	time_t t;
	struct tm tm;
	char buf[32];

	if(gettimeofday(&tv, NULL) != 0)
		return panel_error(panel, "gettimeofday", TRUE);
	t = tv.tv_sec;
	localtime_r(&t, &tm);
	strftime(buf, sizeof(buf), "%H:%M:%S\n%d/%m/%Y", &tm);
	gtk_label_set_text(GTK_LABEL(panel->clock), buf);
	return TRUE;
}


/* panel_delete */
void panel_delete(Panel * panel)
{
	/* FIXME delete panel->apps */
	free(panel);
}


/* useful */
static int _error_text(char const * message, int ret);

int panel_error(Panel * panel, char const * message, int ret)
{
	GtkWidget * dialog;

	if(panel == NULL)
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "%s: %s", message, strerror(errno));
	gtk_window_set_title(GTK_WINDOW(dialog), "Error");
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
				gtk_widget_destroy), NULL);
	gtk_widget_show(dialog);
	return ret;
}

static int _error_text(char const * message, int ret)
{
	fputs("panel: ", stderr);
	perror(message);
	return ret;
}


/* private */
/* functions */
/* panel_exec */
static int _panel_exec(Panel * panel, char const * command)
{
	pid_t pid;

	if((pid = fork()) == -1)
		return panel_error(panel, "fork", 1);
	else if(pid != 0) /* the parent returns */
		return 0;
	execlp("/bin/sh", "sh", "-c", command, NULL);
	exit(panel_error(NULL, command, 2));
	return 1;
}


/* callbacks */
/* panel_idle_apps */
static gint _apps_compare(gconstpointer a, gconstpointer b);

static gboolean _panel_idle_apps(gpointer data)
{
	Panel * panel = data;
	const char path[] = PREFIX "/share/applications";
	DIR * dir;
	struct dirent * de;
	size_t len;
	const char ext[] = ".desktop";
	const char section[] = "Desktop Entry";
	char * name = NULL;
	char * p;
	Config * config = NULL;
	String const * q;
	String const * r;

	if(panel->apps != NULL)
		return FALSE;
	if((dir = opendir(path)) == NULL)
		return panel_error(panel, path, FALSE);
	while((de = readdir(dir)) != NULL)
	{
		len = strlen(de->d_name);
		if(len < sizeof(ext) || strncmp(&de->d_name[len - sizeof(ext)
					+ 1], ext, sizeof(ext)) != 0)
			continue;
		if((p = realloc(name, sizeof(path) + len + 1)) == NULL)
		{
			panel_error(panel, "realloc", 1);
			continue;
		}
		name = p;
		snprintf(name, sizeof(path) + len + 1, "%s/%s", path,
				de->d_name);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, name);
#endif
		if(config == NULL && (config = config_new()) == NULL)
			continue; /* XXX report error */
		else
			config_reset(config);
		if(config_load(config, name) != 0)
		{
			error_print("panel");
			continue;
		}
		q = config_get(config, section, "Name");
		r = config_get(config, section, "Exec");
		if(q == NULL || r == NULL)
			continue;
		panel->apps = g_slist_insert_sorted(panel->apps, config,
				_apps_compare);
		config = NULL;
	}
	free(name);
	closedir(dir);
	if(config != NULL)
		config_delete(config);
	return FALSE;
}

static gint _apps_compare(gconstpointer a, gconstpointer b)
{
	Config * ca = (Config *)a;
	Config * cb = (Config *)b;
	char const * cap;
	char const * cbp;
	const char section[] = "Desktop Entry";
	const char variable[] = "Name";

	/* these should not fail */
	cap = config_get(ca, section, variable);
	cbp = config_get(cb, section, variable);
	return string_compare(cap, cbp);
}


/* panel_image */
static GtkWidget * _panel_image(char const * name)
{
	int width;
	int height;
	size_t len;
	String * buf;
	GError * error = NULL;
	GdkPixbuf * pixbuf = NULL;

	if(gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height) == TRUE
			&& (name != NULL && (len = strlen(name)) > 4)
			&& (strcmp(&name[len - 4], ".png") == 0
				|| strcmp(&name[len - 4], ".xpm") == 0)
			&& (buf = string_new_append(PREFIX, "/share/pixmaps/",
					name, NULL)) != NULL)
	{
		pixbuf = gdk_pixbuf_new_from_file_at_size(buf, width, height,
				&error);
		string_delete(buf);
	}
	if(pixbuf != NULL)
		return gtk_image_new_from_pixbuf(pixbuf);
	return gtk_image_new_from_icon_name(name, GTK_ICON_SIZE_MENU);
}


/* panel_menuitem */
static GtkWidget * _panel_menuitem(char const * label, char const * stock)
{
	GtkWidget * ret;
	GtkWidget * image;

	ret = gtk_image_menu_item_new_with_label(label);
	if(stock != NULL)
	{
		image = _panel_image(stock);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
	}
	return ret;
}
