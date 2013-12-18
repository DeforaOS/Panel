/* $Id$ */
/* Copyright (c) 2009-2013 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <System.h>
#include "Panel.h"
#include "../../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef BINDIR
# define BINDIR		PREFIX "/bin"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif

/* XXX to avoid pointless warnings with GCC */
#define main _main


/* Main */
/* private */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	GSList * apps;
	guint idle;
	gboolean refresh;
	time_t refresh_mti;
	GtkWidget * widget;
} Main;

typedef struct _MainMenu
{
	char const * category;
	char const * label;
	char const * stock;
} MainMenu;


/* constants */
static const MainMenu _main_menus[] =
{
	{ "Audio;",	"Audio",	"gnome-mime-audio",		},
	{ "Development;","Development",	"applications-development",	},
	{ "Education;",	"Education",	"applications-science",		},
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
static Main * _main_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _main_destroy(Main * main);

/* helpers */
static GtkWidget * _main_applications(Main * main);
static GtkWidget * _main_icon(Main * main, char const * path,
		char const * icon);
static GtkWidget * _main_menuitem(Main * main, char const * path,
		char const * label, char const * icon);
static GtkWidget * _main_menuitem_stock(char const * label, char const * stock);

static void _main_xdg_dirs(Main * main, void (*callback)(Main * main,
			char const * path, char const * apppath));

/* callbacks */
static void _on_about(gpointer data);
static void _on_clicked(gpointer data);
static gboolean _on_idle(gpointer data);
static void _on_lock(gpointer data);
static void _on_logout(gpointer data);
#ifdef EMBEDDED
static void _on_rotate(gpointer data);
#endif
static void _on_run(gpointer data);
static void _on_shutdown(gpointer data);
static void _on_suspend(gpointer data);
static gboolean _on_timeout(gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Main menu",
	"gnome-main-menu",
	NULL,
	_main_init,
	_main_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* main_init */
static Main * _main_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Main * main;
	GtkWidget * ret;
	GtkWidget * hbox;
	GtkWidget * image;
	char const * p;
	PangoFontDescription * bold;
	GtkWidget * label;

	if((main = malloc(sizeof(*main))) == NULL)
		return NULL;
	main->helper = helper;
	main->apps = NULL;
	main->idle = g_idle_add(_on_idle, main);
	main->refresh_mti = 0;
	ret = gtk_button_new();
	hbox = gtk_hbox_new(FALSE, 4);
	image = gtk_image_new_from_icon_name("gnome-main-menu",
			helper->icon_size);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);
	/* add some text if configured so */
	if((p = helper->config_get(helper->panel, "main", "text")) != NULL
			&& strlen(p) > 0)
	{
		bold = pango_font_description_new();
		pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
		label = gtk_label_new(p);
		gtk_widget_modify_font(label, bold);
		pango_font_description_free(bold);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	}
	gtk_button_set_relief(GTK_BUTTON(ret), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(ret, _("Main menu"));
#endif
	g_signal_connect_swapped(ret, "clicked", G_CALLBACK(_on_clicked),
			main);
	gtk_container_add(GTK_CONTAINER(ret), hbox);
	gtk_widget_show_all(ret);
	main->widget = ret;
	*widget = main->widget;
	return main;
}


/* main_destroy */
static void _main_destroy(Main * main)
{
	if(main->idle != 0)
		g_source_remove(main->idle);
	g_slist_foreach(main->apps, (GFunc)config_delete, NULL);
	g_slist_free(main->apps);
	gtk_widget_destroy(main->widget);
	free(main);
}


/* helpers */
/* main_applications */
static void _applications_on_activate(gpointer data);
static void _applications_on_activate_application(Config * config);
static void _applications_on_activate_directory(Config * config);
static void _applications_on_activate_url(Config * config);
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
	char const * path;
	size_t i;

	if(main->apps == NULL)
		_on_idle(main);
	memset(&menus, 0, sizeof(menus));
	menu = gtk_menu_new();
	for(p = main->apps; p != NULL; p = p->next)
	{
		config = p->data;
		q = config_get(config, section, "Name"); /* should not fail */
		path = config_get(config, NULL, "path");
		menuitem = _main_menuitem(main, path, q,
				config_get(config, section, "Icon"));
#if GTK_CHECK_VERSION(2, 12, 0)
		if((q = config_get(config, section, "Comment")) != NULL)
			gtk_widget_set_tooltip_text(menuitem, q);
#endif
		if((q = config_get(config, section, "Type")) != NULL
				&& strcmp(q, "Application") == 0
				&& config_get(config, section, "Exec") == NULL)
			gtk_widget_set_sensitive(menuitem, FALSE);
		else
			g_signal_connect_swapped(menuitem, "activate",
					G_CALLBACK(_applications_on_activate),
					config);
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

static void _applications_on_activate(gpointer data)
{
	Config * config = data;
	const char section[] = "Desktop Entry";
	char const * q;

	if((q = config_get(config, section, "Type")) == NULL)
		return;
	else if(strcmp(q, "Application") == 0)
		_applications_on_activate_application(config);
	else if(strcmp(q, "Directory") == 0)
		_applications_on_activate_directory(config);
	else if(strcmp(q, "URL") == 0)
		_applications_on_activate_url(config);
}

static void _applications_on_activate_application(Config * config)
{
	const char section[] = "Desktop Entry";
	char * program;
	char * p;
	char const * q;
	pid_t pid;
	GError * error = NULL;

	if((q = config_get(config, section, "Exec")) == NULL)
		return;
	if((program = strdup(q)) == NULL)
		return; /* XXX report error */
	/* XXX crude way to ignore %f, %F, %u and %U */
	if((p = strchr(program, '%')) != NULL)
		*p = '\0';
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"", __func__, program);
#endif
	if((q = config_get(config, section, "Path")) == NULL)
	{
		/* execute the program directly */
		if(g_spawn_command_line_async(program, &error) != TRUE)
		{
			fprintf(stderr, "%s: %s\n", program, error->message);
			g_error_free(error);
		}
	}
	else if((pid = fork()) == 0)
	{
		/* change the current working directory */
		if(chdir(q) != 0)
			fprintf(stderr, "%s: %s: %s\n", program, q,
					strerror(errno));
		else if(g_spawn_command_line_async(program, &error) != TRUE)
		{
			fprintf(stderr, "%s: %s\n", program, error->message);
			g_error_free(error);
		}
		exit(0);
	}
	else if(pid < 0)
		fprintf(stderr, "%s: %s\n", program, strerror(errno));
	free(program);
}

static void _applications_on_activate_directory(Config * config)
{
	const char section[] = "Desktop Entry";
	char const * directory;
	/* XXX open with the default file manager instead */
	char * argv[] = { "browser", "--", NULL, NULL };
	GSpawnFlags flags = G_SPAWN_SEARCH_PATH;
	GError * error = NULL;

	/* XXX this may not might the correct key */
	if((directory = config_get(config, section, "Path")) == NULL)
		return;
	if((argv[2] = strdup(directory)) == NULL)
		fprintf(stderr, "%s: %s\n", directory, strerror(errno));
	else if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		fprintf(stderr, "%s: %s\n", directory, error->message);
		g_error_free(error);
	}
	free(argv[2]);
}

static void _applications_on_activate_url(Config * config)
{
	const char section[] = "Desktop Entry";
	char const * url;
	/* XXX open with the default web browser instead */
	char * argv[] = { "surfer", "--", NULL, NULL };
	GSpawnFlags flags = G_SPAWN_SEARCH_PATH;
	GError * error = NULL;

	if((url = config_get(config, section, "URL")) == NULL)
		return;
	if((argv[2] = strdup(url)) == NULL)
		fprintf(stderr, "%s: %s\n", url, strerror(errno));
	else if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		fprintf(stderr, "%s: %s\n", url, error->message);
		g_error_free(error);
	}
	free(argv[2]);
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
		menuitem = _main_menuitem_stock(m->label, m->stock);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menus[i]);
		gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, pos++);
	}
}


/* main_icon */
static GtkWidget * _main_icon(Main * main, char const * path, char const * icon)
{
	const char pixmaps[] = "/pixmaps/";
	int width = 16;
	int height = 16;
	String * buf;
	GdkPixbuf * pixbuf = NULL;
	GError * error = NULL;

	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
	if(icon[0] == '/')
		pixbuf = gdk_pixbuf_new_from_file_at_size(icon, width, height,
				&error);
	else if(strchr(icon, '.') != NULL)
	{
		if(path == NULL)
			path = DATADIR;
		if((buf = string_new_append(path, pixmaps, icon, NULL)) != NULL)
		{
			pixbuf = gdk_pixbuf_new_from_file_at_size(buf, width,
					height, &error);
			string_delete(buf);
		}
	}
	if(error != NULL)
	{
		main->helper->error(NULL, error->message, 1);
		g_error_free(error);
	}
	if(pixbuf == NULL)
		return gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
	return gtk_image_new_from_pixbuf(pixbuf);
}


/* main_menuitem */
static GtkWidget * _main_menuitem(Main * main, char const * path,
		char const * label, char const * icon)
{
	GtkWidget * ret;
	GtkWidget * image;

	ret = gtk_image_menu_item_new_with_label(label);
	if(icon != NULL)
	{
		image = _main_icon(main, path, icon);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
	}
	return ret;
}


/* main_menuitem_stock */
static GtkWidget * _main_menuitem_stock(char const * label, char const * stock)
{
	GtkWidget * ret;
	GtkWidget * image;

	ret = gtk_image_menu_item_new_with_label(label);
	if(stock != NULL)
	{
		image = gtk_image_new_from_icon_name(stock, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
	}
	return ret;
}


/* main_xdg_dirs */
static void _xdg_dirs_home(Main * main, void (*callback)(Main * main,
			char const * path, char const * apppath));
static void _xdg_dirs_path(Main * main, void (*callback)(Main * main,
			char const * path, char const * apppath),
		char const * path);

static void _main_xdg_dirs(Main * main, void (*callback)(Main * main,
			char const * path, char const * apppath))
{
	char const * path;
	char * p;
	size_t i;
	size_t j;

	if((path = getenv("XDG_DATA_DIRS")) != NULL
			&& strlen(path) > 0
			&& (p = strdup(path)) != NULL)
	{
		for(i = 0, j = 0;; i++)
			if(p[i] == '\0')
			{
				_xdg_dirs_path(main, callback, &p[j]);
				break;
			}
			else if(p[i] == ':')
			{
				p[i] = '\0';
				_xdg_dirs_path(main, callback, &p[j]);
				j = i + 1;
			}
		free(p);
	}
	else
		_xdg_dirs_path(main, callback, DATADIR);
	_xdg_dirs_home(main, callback);
}

static void _xdg_dirs_home(Main * main, void (*callback)(Main * main,
			char const * path, char const * apppath))
{
	char const fallback[] = ".local/share";
	char const * path;
	char const * homedir;
	size_t len;
	char * p;

	/* FIXME fallback to "$HOME/.local/share" if not set */
	if((path = getenv("XDG_DATA_HOME")) != NULL && strlen(path) > 0)
	{
		_xdg_dirs_path(main, callback, path);
		return;
	}
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(fallback);
	if((p = malloc(len)) == NULL)
	{
		main->helper->error(NULL, homedir, 1);
		return;
	}
	snprintf(p, len, "%s/%s", homedir, fallback);
	_xdg_dirs_path(main, callback, p);
	free(p);
}

static void _xdg_dirs_path(Main * main, void (*callback)(Main * main,
			char const * path, char const * apppath),
		char const * path)
{
	const char applications[] = "/applications";
	char * apppath;

	if((apppath = string_new_append(path, applications, NULL)) == NULL)
		main->helper->error(NULL, path, 1);
	callback(main, path, apppath);
	string_delete(apppath);
}


/* callbacks */
/* on_about */
static void _on_about(gpointer data)
{
	Main * main = data;

	main->helper->about_dialog(main->helper->panel);
}


/* on_clicked */
static void _clicked_position_menu(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data);

static void _on_clicked(gpointer data)
{
	Main * main = data;
	GtkWidget * menu;
	GtkWidget * menuitem;

	menu = gtk_menu_new();
	menuitem = _main_menuitem_stock(_("Applications"),
			"gnome-applications");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), _main_applications(
				main));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = _main_menuitem_stock(_("Run..."), GTK_STOCK_EXECUTE);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(_on_run),
			main);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(_on_about),
			main);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	/* lock screen */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = _main_menuitem_stock(_("Lock screen"), "gnome-lockscreen");
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(_on_lock),
			main);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
#ifdef EMBEDDED
	/* rotate screen */
	menuitem = _main_menuitem_stock(_("Rotate"),
			GTK_STOCK_REFRESH); /* XXX */
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(_on_rotate),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
#endif
	/* logout */
	if(main->helper->logout_dialog != NULL)
	{
		menuitem = _main_menuitem_stock(_("Logout..."), "gnome-logout");
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_on_logout), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	/* suspend */
	if(main->helper->suspend != NULL)
	{
		menuitem = _main_menuitem_stock(_("Suspend"),
				"gtk-media-pause");
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_on_suspend), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	/* shutdown */
	menuitem = _main_menuitem_stock(_("Shutdown..."), "gnome-shutdown");
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_shutdown), data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, _clicked_position_menu,
			main, 0, gtk_get_current_event_time());
}

static void _clicked_position_menu(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data)
{
	Main * main = data;
	GtkAllocation a;

#if GTK_CHECK_VERSION(2, 18, 0)
	gtk_widget_get_allocation(main->widget, &a);
#else
	a = main->widget->allocation;
#endif
	*x = a.x;
	*y = a.y;
	main->helper->position_menu(main->helper->panel, menu, x, y, push_in);
}


/* on_idle */
static int _idle_access(Main * main, char const * path, int mode);
static int _idle_access_path(Main * main, char const * path,
		char const * filename, int mode);
static gint _idle_apps_compare(gconstpointer a, gconstpointer b);
static void _idle_path(Main * main, char const * path, char const * apppath);

static gboolean _on_idle(gpointer data)
{
	Main * main = data;

	if(main->apps != NULL)
	{
		main->idle = 0;
		return FALSE;
	}
	_main_xdg_dirs(main, _idle_path);
	main->idle = g_timeout_add(10000, _on_timeout, main);
	return FALSE;
}

static int _idle_access(Main * main, char const * path, int mode)
{
	int ret = -1;
	char const * p;
	char * q;
	size_t i;
	size_t j;

	if(path[0] == '/')
		return access(path, mode);
	if((p = getenv("PATH")) == NULL)
		return 0;
	if((q = strdup(p)) == NULL)
	{
		main->helper->error(NULL, path, 1);
		return 0;
	}
	errno = ENOENT;
	for(i = 0, j = 0;; i++)
		if(q[i] == '\0')
		{
			ret = _idle_access_path(main, &q[j], path, mode);
			break;
		}
		else if(q[i] == ':')
		{
			q[i] = '\0';
			if((ret = _idle_access_path(main, &q[j], path, mode))
					== 0)
				break;
			j = i + 1;
		}
	free(q);
	return ret;
}

static int _idle_access_path(Main * main, char const * path,
		char const * filename, int mode)
{
	int ret;
	char * p;
	size_t len;

	len = strlen(path) + 1 + strlen(filename) + 1;
	if((p = malloc(len)) == NULL)
		return -main->helper->error(NULL, path, 1);
	snprintf(p, len, "%s/%s", path, filename);
	ret = access(p, mode);
	free(p);
	return ret;
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

static void _idle_path(Main * main, char const * path, char const * apppath)
{
	DIR * dir;
	int fd;
	struct stat st;
	struct dirent * de;
	size_t len;
	const char ext[] = ".desktop";
	const char section[] = "Desktop Entry";
	char * name = NULL;
	char * p;
	Config * config = NULL;
	String const * q;

#if defined(__sun__)
	if((fd = open(apppath, O_RDONLY)) < 0
			|| fstat(fd, &st) != 0
			|| (dir = fdopendir(fd)) == NULL)
#else
	if((dir = opendir(apppath)) == NULL
			|| (fd = dirfd(dir)) < 0
			|| fstat(fd, &st) != 0)
#endif
	{
		if(errno != ENOENT)
			main->helper->error(NULL, apppath, 1);
		return;
	}
	if(st.st_mtime > main->refresh_mti)
		main->refresh_mti = st.st_mtime;
	while((de = readdir(dir)) != NULL)
	{
		if(de->d_name[0] == '.')
			if(de->d_name[1] == '\0' || (de->d_name[1] == '.'
						&& de->d_name[2] == '\0'))
				continue;
		len = strlen(de->d_name);
		if(len < sizeof(ext) || strncmp(&de->d_name[len - sizeof(ext)
					+ 1], ext, sizeof(ext)) != 0)
			continue;
		if((p = realloc(name, strlen(apppath) + len + 2)) == NULL)
		{
			main->helper->error(NULL, apppath, 1);
			continue;
		}
		name = p;
		snprintf(name, strlen(apppath) + len + 2, "%s/%s", apppath,
				de->d_name);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, name);
#endif
		if(config != NULL)
			config_reset(config);
		else
			config = config_new();
		if(config == NULL || config_load(config, name) != 0)
		{
			main->helper->error(NULL, NULL, 0); /* XXX */
			continue;
		}
		/* skip this entry if it has an unknown type */
		if((q = config_get(config, section, "Type")) == NULL)
			continue;
		if(strcmp(q, "Application") != 0
				&& strcmp(q, "Directory") != 0
				&& strcmp(q, "URL") != 0)
			continue;
		/* skip this entry if there is no name defined */
		if((q = config_get(config, section, "Name")) == NULL)
			continue;
		/* skip this entry if should not be displayed at all */
		if((q = config_get(config, section, "NoDisplay")) != NULL
				&& strcmp(q, "true") == 0)
			continue;
		/* skip this entry if the binary cannot be executed */
		if((q = config_get(config, section, "TryExec")) != NULL
				&& _idle_access(main, q, X_OK) != 0
				&& errno == ENOENT)
			continue;
		/* remember the path */
		config_set(config, NULL, "path", path);
		main->apps = g_slist_insert_sorted(main->apps, config,
				_idle_apps_compare);
		config = NULL;
	}
	free(name);
	closedir(dir);
	if(config != NULL)
		config_delete(config);
}


/* on_lock */
static void _on_lock(gpointer data)
{
	Main * main = data;

	main->helper->lock(main->helper->panel);
}


/* on_logout */
static void _on_logout(gpointer data)
{
	Main * main = data;

	main->helper->logout_dialog(main->helper->panel);
}


#ifdef EMBEDDED
/* on_rotate */
static void _on_rotate(gpointer data)
{
	Main * main = data;

	main->helper->rotate_screen(main->helper->panel);
}
#endif


/* on_run */
static void _on_run(gpointer data)
{
	Main * main = data;
	char * argv[] = { BINDIR "/run", NULL };
	GSpawnFlags flags = G_SPAWN_STDOUT_TO_DEV_NULL
		| G_SPAWN_STDERR_TO_DEV_NULL;
	GError * error = NULL;

	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		main->helper->error(main->helper->panel, error->message, 1);
		g_error_free(error);
	}
}


/* on_shutdown */
static void _on_shutdown(gpointer data)
{
	Main * main = data;

	main->helper->shutdown_dialog(main->helper->panel);
}


/* on_suspend */
static void _on_suspend(gpointer data)
{
	Main * main = data;

	main->helper->suspend(main->helper->panel);
}


/* on_timeout */
static void _timeout_path(Main * main, char const * path, char const * apppath);

static gboolean _on_timeout(gpointer data)
{
	Main * main = data;

	main->refresh = FALSE;
	_main_xdg_dirs(main, _timeout_path);
	if(main->refresh == FALSE)
		return TRUE;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() resetting the menu\n", __func__);
#endif
	g_slist_foreach(main->apps, (GFunc)config_delete, NULL);
	g_slist_free(main->apps);
	main->apps = NULL;
	main->idle = g_idle_add(_on_idle, main);
	return FALSE;
}

static void _timeout_path(Main * main, char const * path, char const * apppath)
{
	struct stat st;

	if(main->refresh != TRUE
			&& stat(apppath, &st) == 0
			&& st.st_mtime > main->refresh_mti)
		main->refresh = TRUE;
}
