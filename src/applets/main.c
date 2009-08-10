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



#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "panel.h"
#include "../common.h"


/* Main */
/* private */
/* types */
typedef struct _Main
{
	PanelAppletHelper * helper;
	GSList * apps;
	guint idle;
} Main;

typedef struct _MainMenu
{
	char const * category;
	char const * label;
	char const * stock;
} MainMenu;


/* constants */
#ifndef PREFIX
# define PREFIX "/usr/local"
#endif

static const MainMenu _main_menus[] =
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
#define MAIN_MENUS_COUNT (sizeof(_main_menus) / sizeof(*_main_menus))


/* prototypes */
static GtkWidget * _main_init(PanelApplet * applet);
static void _main_destroy(PanelApplet * applet);

/* helpers */
static GtkWidget * _main_applications(Main * main);
static GtkWidget * _main_image(char const * name);
static GtkWidget * _main_menuitem(char const * label, char const * stock);

/* callbacks */
static void _on_clicked(GtkWidget * widget, gpointer data);
static gboolean _on_idle(gpointer data);
static void _on_lock(GtkWidget * widget, gpointer data);
static void _on_logout(GtkWidget * widget, gpointer data);
static void _on_run(GtkWidget * widget, gpointer data);


/* public */
/* variables */
PanelApplet applet =
{
	NULL,
	_main_init,
	_main_destroy,
	PANEL_APPLET_POSITION_FIRST,
	FALSE,
	TRUE,
	NULL
};


/* private */
/* functions */
/* main_init */
static GtkWidget * _main_init(PanelApplet * applet)
{
	GtkWidget * ret;
	Main * main;
	GtkWidget * image;

	if((main = malloc(sizeof(*main))) == NULL)
		return NULL;
	main->helper = applet->helper;
	main->apps = NULL;
	main->idle = g_idle_add(_on_idle, main);
	applet->priv = main;
	ret = gtk_button_new();
	image = gtk_image_new_from_icon_name("gnome-main-menu",
			GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(ret), image);
	gtk_button_set_relief(GTK_BUTTON(ret), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(ret, "Main menu");
	g_signal_connect(G_OBJECT(ret), "clicked", G_CALLBACK(_on_clicked),
			main);
	return ret;
}


/* main_destroy */
static void _main_destroy(PanelApplet * applet)
{
	Main * main = applet->priv;

	if(main->idle != 0)
		g_source_remove(main->idle);
	/* FIXME free main->apps */
	free(main);
}


/* helpers */
/* main_applications */
static void _applications_on_activate(GtkWidget * widget, gpointer data);
static void _applications_categories(GtkWidget * menu, GtkWidget ** menus);

static GtkWidget * _main_applications(Main * main)
{
	GtkWidget * menus[MAIN_MENUS_COUNT];
	GSList * p;
	GtkWidget * menu;
	GtkWidget * menuitem;
	Config * config;
	const char section[] = "Desktop Entry";
	char const * q;
	size_t i;

	_on_idle(main); /* just in case */
	memset(menus, 0, sizeof(menus));
	menu = gtk_menu_new();
	for(p = main->apps; p != NULL; p = p->next)
	{
		config = p->data;
		q = config_get(config, section, "Name"); /* should not fail */
		menuitem = _main_menuitem(q, config_get(config, section,
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
		for(i = 0; _main_menus[i].category != NULL && string_find(q,
					_main_menus[i].category) == NULL; i++);
		if(_main_menus[i].category == NULL)
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
	g_spawn_command_line_async(program, NULL);
}

static void _applications_categories(GtkWidget * menu, GtkWidget ** menus)
{
	size_t i;
	MainMenu const * m;
	GtkWidget * menuitem;
	size_t pos = 0;

	for(i = 0; _main_menus[i].category != NULL; i++)
	{
		if(menus[i] == NULL)
			continue;
		m = &_main_menus[i];
		menuitem = _main_menuitem(m->label, m->stock);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menus[i]);
		gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, pos++);
	}
}


/* main_image */
static GtkWidget * _main_image(char const * name)
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


/* main_menuitem */
static GtkWidget * _main_menuitem(char const * label, char const * stock)
{
	GtkWidget * ret;
	GtkWidget * image;

	ret = gtk_image_menu_item_new_with_label(label);
	if(stock != NULL)
	{
		image = _main_image(stock);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
	}
	return ret;
}


/* callbacks */
/* on_idle */
static gint _idle_apps_compare(gconstpointer a, gconstpointer b);

static gboolean _on_idle(gpointer data)
{
	Main * main = data;
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

	if(main->apps != NULL)
		return FALSE;
	if((dir = opendir(path)) == NULL)
		return error_set_print("panel", FALSE, "%s: %s", path, strerror(
					errno));
	while((de = readdir(dir)) != NULL)
	{
		len = strlen(de->d_name);
		if(len < sizeof(ext) || strncmp(&de->d_name[len - sizeof(ext)
					+ 1], ext, sizeof(ext)) != 0)
			continue;
		if((p = realloc(name, sizeof(path) + len + 1)) == NULL)
		{
			error_set_print("panel", 1, "%s: %s", "realloc",
					strerror(errno));
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
		main->apps = g_slist_insert_sorted(main->apps, config,
				_idle_apps_compare);
		config = NULL;
	}
	free(name);
	closedir(dir);
	if(config != NULL)
		config_delete(config);
	return FALSE;
}

static gint _idle_apps_compare(gconstpointer a, gconstpointer b)
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


/* on_clicked */
#if 0
static void _clicked_menu_position(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data);
#endif

static void _on_clicked(GtkWidget * widget, gpointer data)
{
	Main * main = data;
	GtkWidget * menu;
	GtkWidget * menuitem;

	menu = gtk_menu_new();
	menuitem = _main_menuitem("Applications", "gnome-applications");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), _main_applications(
				main));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = _main_menuitem("Run...", GTK_STOCK_EXECUTE);
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
	menuitem = _main_menuitem("Lock screen", "gnome-lockscreen");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(_on_lock),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = _main_menuitem("Logout...", "gnome-logout");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(_on_logout),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, main->helper->position_menu,
			main->helper->priv, 0, gtk_get_current_event_time());
}

#if 0
static void _clicked_menu_position(GtkMenu * menu, gint * x, gint * y,
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
#if 0 /* FIXME figure a way for this */
	*y = panel->height - PANEL_BORDER_WIDTH - PANEL_ICON_SIZE - req.height;
#else
	*y = 1024 - PANEL_BORDER_WIDTH - PANEL_ICON_SIZE - req.height;
#endif
	*push_in = TRUE;
}
#endif


/* on_lock */
static void _on_lock(GtkWidget * widget, gpointer data)
{
	char * argv[] = { "xscreensaver-command", "-lock", NULL };
	GSpawnFlags flags = G_SPAWN_SEARCH_PATH
		| G_SPAWN_STDOUT_TO_DEV_NULL
		| G_SPAWN_STDERR_TO_DEV_NULL;

	g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, NULL);
}


/* on_logout */
static void _on_logout(GtkWidget * widget, gpointer data)
{
	Main * main = data;

	main->helper->logout_dialog();
}


/* on_run */
static void _on_run(GtkWidget * widget, gpointer data)
{
	char * argv[] = { "run", NULL };
	GSpawnFlags flags = G_SPAWN_SEARCH_PATH
		| G_SPAWN_STDOUT_TO_DEV_NULL
		| G_SPAWN_STDERR_TO_DEV_NULL;

	g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, NULL);
}