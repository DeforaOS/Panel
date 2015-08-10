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



#include <sys/stat.h>
#if defined(__sun)
# include <fcntl.h>
#endif
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <System.h>
#include "Panel/applet.h"
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


/* Menu */
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
} Menu;

typedef struct _MenuMenu
{
	char const * category;
	char const * label;
	char const * stock;
} MenuMenu;


/* constants */
static const MenuMenu _menu_menus[] =
{
	{ "Audio",	"Audio",	"gnome-mime-audio",		},
	{ "Development","Development",	"applications-development",	},
	{ "Education",	"Education",	"applications-science",		},
	{ "Game",	"Games",	"applications-games",		},
	{ "Graphics",	"Graphics",	"applications-graphics",	},
	{ "AudioVideo",	"Multimedia",	"applications-multimedia",	},
	{ "Network",	"Network",	"applications-internet",	},
	{ "Office",	"Office",	"applications-office",		},
	{ "Settings",	"Settings",	"gnome-settings",		},
	{ "System",	"System",	"applications-system",		},
	{ "Utility",	"Utilities",	"applications-utilities",	},
	{ "Video",	"Video",	"video",			}
};
#define MENU_MENUS_COUNT (sizeof(_menu_menus) / sizeof(*_menu_menus))


/* prototypes */
static Menu * _menu_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _menu_destroy(Menu * menu);

/* helpers */
static GtkWidget * _menu_applications(Menu * menu);
static GtkWidget * _menu_icon(Menu * menu, char const * path,
		char const * icon);
static GtkWidget * _menu_menuitem(Menu * menu, char const * path,
		char const * label, char const * icon);
static GtkWidget * _menu_menuitem_stock(char const * icon, char const * label,
		gboolean mnemonic);

static void _menu_xdg_dirs(Menu * menu, void (*callback)(Menu * menu,
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
	"start-here",
	NULL,
	_menu_init,
	_menu_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* menu_init */
static Menu * _menu_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Menu * menu;
	GtkWidget * hbox;
	GtkWidget * image;
	char const * p;
	PangoFontDescription * bold;
	GtkWidget * label;

	if((menu = malloc(sizeof(*menu))) == NULL)
		return NULL;
	menu->helper = helper;
	menu->apps = NULL;
	menu->idle = g_idle_add(_on_idle, menu);
	menu->refresh_mti = 0;
	menu->widget = gtk_button_new();
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
	hbox = gtk_hbox_new(FALSE, 4);
#endif
	image = gtk_image_new_from_icon_name("start-here",
			panel_window_get_icon_size(helper->window));
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);
	/* add some text if configured so */
	if((p = helper->config_get(helper->panel, "menu", "text")) != NULL
			&& strlen(p) > 0)
	{
		bold = pango_font_description_new();
		pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
		label = gtk_label_new(p);
		gtk_widget_modify_font(label, bold);
		pango_font_description_free(bold);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	}
	gtk_button_set_relief(GTK_BUTTON(menu->widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(menu->widget, _("Main menu"));
#endif
	g_signal_connect_swapped(menu->widget, "clicked", G_CALLBACK(
				_on_clicked), menu);
	gtk_container_add(GTK_CONTAINER(menu->widget), hbox);
	gtk_widget_show_all(menu->widget);
	*widget = menu->widget;
	return menu;
}


/* menu_destroy */
static void _menu_destroy(Menu * menu)
{
	if(menu->idle != 0)
		g_source_remove(menu->idle);
	g_slist_foreach(menu->apps, (GFunc)config_delete, NULL);
	g_slist_free(menu->apps);
	gtk_widget_destroy(menu->widget);
	free(menu);
}


/* helpers */
/* menu_applications */
static void _applications_on_activate(gpointer data);
static void _applications_on_activate_application(Config * config);
static void _applications_on_activate_directory(Config * config);
static void _applications_on_activate_url(Config * config);
static void _applications_categories(GtkWidget * menu, GtkWidget ** menus);

static GtkWidget * _menu_applications(Menu * menu)
{
	GtkWidget * menus[MENU_MENUS_COUNT];
	GSList * p;
	GtkWidget * menushell;
	GtkWidget * menuitem;
	Config * config;
	const char section[] = "Desktop Entry";
	char const * name;
#if GTK_CHECK_VERSION(2, 12, 0)
	char const * comment;
#endif
	char const * q;
	char const * r;
	char const * path;
	size_t i;

	if(menu->apps == NULL)
		_on_idle(menu);
	memset(&menus, 0, sizeof(menus));
	menushell = gtk_menu_new();
	for(p = menu->apps; p != NULL; p = p->next)
	{
		config = p->data;
		/* should not fail */
		name = config_get(config, section, "Name");
#if GTK_CHECK_VERSION(2, 12, 0)
		comment = config_get(config, section, "Comment");
#endif
		if((q = config_get(config, section, "GenericName")) != NULL)
		{
#if GTK_CHECK_VERSION(2, 12, 0)
			if(comment == NULL)
				comment = name;
#endif
			name = q;
		}
		path = config_get(config, NULL, "path");
		menuitem = _menu_menuitem(menu, path, name,
				config_get(config, section, "Icon"));
#if GTK_CHECK_VERSION(2, 12, 0)
		if(comment != NULL)
			gtk_widget_set_tooltip_text(menuitem, comment);
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
			gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
			continue;
		}
		for(i = 0; i < MENU_MENUS_COUNT; i++)
		{
			if((r = string_find(q, _menu_menus[i].category)) == NULL)
				continue;
			r += string_length(_menu_menus[i].category);
			if(*r == '\0' || *r == ';')
				break;
		}
		if(i == MENU_MENUS_COUNT)
		{
			gtk_menu_shell_append(GTK_MENU_SHELL(menushell),
					menuitem);
			continue;
		}
		if(menus[i] == NULL)
			menus[i] = gtk_menu_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menus[i]), menuitem);
	}
	_applications_categories(menushell, menus);
	return menushell;
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
	char * argv[] = { BINDIR "/htmlapp", "--", NULL, NULL };
	unsigned int flags = 0;
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
	MenuMenu const * m;
	GtkWidget * menuitem;
	size_t pos = 0;

	for(i = 0; i < MENU_MENUS_COUNT; i++)
	{
		if(menus[i] == NULL)
			continue;
		m = &_menu_menus[i];
		menuitem = _menu_menuitem_stock(m->stock, m->label, FALSE);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menus[i]);
		gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, pos++);
	}
}


/* menu_icon */
static GtkWidget * _menu_icon(Menu * menu, char const * path, char const * icon)
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
		menu->helper->error(NULL, error->message, 1);
		g_error_free(error);
	}
	if(pixbuf == NULL)
		return gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
	return gtk_image_new_from_pixbuf(pixbuf);
}


/* menu_menuitem */
static GtkWidget * _menu_menuitem(Menu * menu, char const * path,
		char const * label, char const * icon)
{
	GtkWidget * ret;
	GtkWidget * image;

	ret = gtk_image_menu_item_new_with_label(label);
	if(icon != NULL)
	{
		image = _menu_icon(menu, path, icon);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
	}
	return ret;
}


/* menu_menuitem_stock */
static GtkWidget * _menu_menuitem_stock(char const * icon, char const * label,
		gboolean mnemonic)
{
	GtkWidget * ret;
	GtkWidget * image;

	ret = (mnemonic) ? gtk_image_menu_item_new_with_mnemonic(label)
		: gtk_image_menu_item_new_with_label(label);
	if(icon != NULL)
	{
		image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
	}
	return ret;
}


/* menu_xdg_dirs */
static void _xdg_dirs_home(Menu * menu, void (*callback)(Menu * menu,
			char const * path, char const * apppath));
static void _xdg_dirs_path(Menu * menu, void (*callback)(Menu * menu,
			char const * path, char const * apppath),
		char const * path);

static void _menu_xdg_dirs(Menu * menu, void (*callback)(Menu * menu,
			char const * path, char const * apppath))
{
	char const * path;
	char * p;
	size_t i;
	size_t j;
	int datadir = 1;

	/* read through every XDG application folder */
	if((path = getenv("XDG_DATA_DIRS")) == NULL || strlen(path) == 0)
	{
		path = "/usr/local/share:/usr/share";
		datadir = 0;
	}
	if((p = strdup(path)) == NULL)
		menu->helper->error(NULL, "strdup", 1);
	else
		for(i = 0, j = 0;; i++)
			if(p[i] == '\0')
			{
				string_rtrim(&p[j], "/");
				_xdg_dirs_path(menu, callback, &p[j]);
				datadir |= (strcmp(&p[j], DATADIR) == 0);
				break;
			}
			else if(p[i] == ':')
			{
				p[i] = '\0';
				string_rtrim(&p[j], "/");
				_xdg_dirs_path(menu, callback, &p[j]);
				datadir |= (strcmp(&p[j], DATADIR) == 0);
				j = i + 1;
			}
	free(p);
	if(datadir == 0)
		_xdg_dirs_path(menu, callback, DATADIR);
	_xdg_dirs_home(menu, callback);
}

static void _xdg_dirs_home(Menu * menu, void (*callback)(Menu * menu,
			char const * path, char const * apppath))
{
	char const fallback[] = ".local/share";
	char const * path;
	char const * homedir;
	String * p;

	/* use $XDG_DATA_HOME if set and not empty */
	if((path = getenv("XDG_DATA_HOME")) != NULL && strlen(path) > 0)
	{
		_xdg_dirs_path(menu, callback, path);
		return;
	}
	/* fallback to "$HOME/.local/share" */
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	if((p = string_new_append(homedir, "/", fallback, NULL)) == NULL)
	{
		menu->helper->error(NULL, homedir, 1);
		return;
	}
	_xdg_dirs_path(menu, callback, p);
	string_delete(p);
}

static void _xdg_dirs_path(Menu * menu, void (*callback)(Menu * menu,
			char const * path, char const * apppath),
		char const * path)
{
	const char applications[] = "/applications";
	char * apppath;

	if((apppath = string_new_append(path, applications, NULL)) == NULL)
		menu->helper->error(NULL, path, 1);
	callback(menu, path, apppath);
	string_delete(apppath);
}


/* callbacks */
/* on_about */
static void _on_about(gpointer data)
{
	Menu * menu = data;

	menu->helper->about_dialog(menu->helper->panel);
}


/* on_clicked */
static void _clicked_position_menu(GtkMenu * widget, gint * x, gint * y,
		gboolean * push_in, gpointer data);

static void _on_clicked(gpointer data)
{
	Menu * menu = data;
	PanelAppletHelper * helper = menu->helper;
	GtkWidget * menushell;
	GtkWidget * menuitem;
	GtkWidget * widget;
	char const * p;

	menushell = gtk_menu_new();
	if((p = helper->config_get(helper->panel, "menu", "applications"))
			== NULL || strtol(p, NULL, 0) != 0)
	{
		menuitem = _menu_menuitem_stock("gnome-applications",
				_("A_pplications"), TRUE);
		widget = _menu_applications(menu);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), widget);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	if((p = helper->config_get(helper->panel, "menu", "run")) == NULL
			|| strtol(p, NULL, 0) != 0)
	{
		menuitem = _menu_menuitem_stock(GTK_STOCK_EXECUTE, _("_Run..."),
				TRUE);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_on_run), menu);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	if((p = helper->config_get(helper->panel, "menu", "about")) == NULL
			|| strtol(p, NULL, 0) != 0)
	{
		menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT,
				NULL);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_on_about), menu);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	/* lock screen */
	menuitem = _menu_menuitem_stock("gnome-lockscreen", _("_Lock screen"),
			TRUE);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(_on_lock),
			menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
#ifdef EMBEDDED
	/* rotate screen */
	/* XXX find a more appropriate icon */
	menuitem = _menu_menuitem_stock(GTK_STOCK_REFRESH, _("R_otate"), TRUE);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(_on_rotate),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
#endif
	/* logout */
	if(menu->helper->logout_dialog != NULL)
	{
		menuitem = _menu_menuitem_stock("gnome-logout", _("Lo_gout..."),
				TRUE);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_on_logout), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	/* suspend */
	if(menu->helper->suspend != NULL)
	{
		menuitem = _menu_menuitem_stock("gtk-media-pause",
				_("S_uspend"), TRUE);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_on_suspend), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	/* shutdown */
	menuitem = _menu_menuitem_stock("gnome-shutdown", _("_Shutdown..."),
			TRUE);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_on_shutdown), data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	gtk_widget_show_all(menushell);
	gtk_menu_popup(GTK_MENU(menushell), NULL, NULL, _clicked_position_menu,
			menu, 0, gtk_get_current_event_time());
}

static void _clicked_position_menu(GtkMenu * widget, gint * x, gint * y,
		gboolean * push_in, gpointer data)
{
	Menu * menu = data;
	GtkAllocation a;

#if GTK_CHECK_VERSION(2, 18, 0)
	gtk_widget_get_allocation(menu->widget, &a);
#else
	a = menu->widget->allocation;
#endif
	*x = a.x;
	*y = a.y;
	menu->helper->position_menu(menu->helper->panel, widget, x, y, push_in);
}


/* on_idle */
static int _idle_access(Menu * menu, char const * path, int mode);
static int _idle_access_path(Menu * menu, char const * path,
		char const * filename, int mode);
static gint _idle_apps_compare(gconstpointer a, gconstpointer b);
static void _idle_path(Menu * menu, char const * path, char const * apppath);

static gboolean _on_idle(gpointer data)
{
	Menu * menu = data;

	if(menu->apps != NULL)
	{
		menu->idle = 0;
		return FALSE;
	}
	_menu_xdg_dirs(menu, _idle_path);
	menu->idle = g_timeout_add(10000, _on_timeout, menu);
	return FALSE;
}

static int _idle_access(Menu * menu, char const * path, int mode)
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
		menu->helper->error(NULL, path, 1);
		return 0;
	}
	errno = ENOENT;
	for(i = 0, j = 0;; i++)
		if(q[i] == '\0')
		{
			ret = _idle_access_path(menu, &q[j], path, mode);
			break;
		}
		else if(q[i] == ':')
		{
			q[i] = '\0';
			if((ret = _idle_access_path(menu, &q[j], path, mode))
					== 0)
				break;
			j = i + 1;
		}
	free(q);
	return ret;
}

static int _idle_access_path(Menu * menu, char const * path,
		char const * filename, int mode)
{
	int ret;
	char * p;
	size_t len;

	len = strlen(path) + 1 + strlen(filename) + 1;
	if((p = malloc(len)) == NULL)
		return -menu->helper->error(NULL, path, 1);
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

static void _idle_path(Menu * menu, char const * path, char const * apppath)
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

#if defined(__sun)
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
			menu->helper->error(NULL, apppath, 1);
		return;
	}
	if(st.st_mtime > menu->refresh_mti)
		menu->refresh_mti = st.st_mtime;
	while((de = readdir(dir)) != NULL)
	{
		if(de->d_name[0] == '.')
			if(de->d_name[1] == '\0' || (de->d_name[1] == '.'
						&& de->d_name[2] == '\0'))
				continue;
		len = strlen(de->d_name);
		if(len < sizeof(ext))
			continue;
		if(strncmp(&de->d_name[len - sizeof(ext) + 1], ext,
					sizeof(ext)) != 0)
			continue;
		if((p = realloc(name, strlen(apppath) + len + 2)) == NULL)
		{
			menu->helper->error(NULL, apppath, 1);
			continue;
		}
		name = p;
		snprintf(name, strlen(apppath) + len + 2, "%s/%s", apppath,
				de->d_name);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, name);
#endif
		if(config == NULL)
			config = config_new();
		else
			config_reset(config);
		if(config == NULL || config_load(config, name) != 0)
		{
			menu->helper->error(NULL, NULL, 0); /* XXX */
			continue;
		}
		/* skip this entry if it is deleted */
		if((q = config_get(config, section, "Hidden")) != NULL
				&& strcmp(q, "true") == 0)
			continue;
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
				&& _idle_access(menu, q, X_OK) != 0
				&& errno == ENOENT)
			continue;
		/* remember the path */
		config_set(config, NULL, "path", path);
		menu->apps = g_slist_insert_sorted(menu->apps, config,
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
	Menu * menu = data;

	menu->helper->lock(menu->helper->panel);
}


/* on_logout */
static void _on_logout(gpointer data)
{
	Menu * menu = data;

	menu->helper->logout_dialog(menu->helper->panel);
}


#ifdef EMBEDDED
/* on_rotate */
static void _on_rotate(gpointer data)
{
	Menu * menu = data;

	menu->helper->rotate_screen(menu->helper->panel);
}
#endif


/* on_run */
static void _on_run(gpointer data)
{
	Menu * menu = data;
	char * argv[] = { BINDIR "/run", NULL };
	GSpawnFlags flags = G_SPAWN_STDOUT_TO_DEV_NULL
		| G_SPAWN_STDERR_TO_DEV_NULL;
	GError * error = NULL;

	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		menu->helper->error(menu->helper->panel, error->message, 1);
		g_error_free(error);
	}
}


/* on_shutdown */
static void _on_shutdown(gpointer data)
{
	Menu * menu = data;

	menu->helper->shutdown_dialog(menu->helper->panel);
}


/* on_suspend */
static void _on_suspend(gpointer data)
{
	Menu * menu = data;

	menu->helper->suspend(menu->helper->panel);
}


/* on_timeout */
static void _timeout_path(Menu * menu, char const * path, char const * apppath);

static gboolean _on_timeout(gpointer data)
{
	Menu * menu = data;

	menu->refresh = FALSE;
	_menu_xdg_dirs(menu, _timeout_path);
	if(menu->refresh == FALSE)
		return TRUE;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() resetting the menu\n", __func__);
#endif
	g_slist_foreach(menu->apps, (GFunc)config_delete, NULL);
	g_slist_free(menu->apps);
	menu->apps = NULL;
	menu->idle = g_idle_add(_on_idle, menu);
	return FALSE;
}

static void _timeout_path(Menu * menu, char const * path, char const * apppath)
{
	struct stat st;

	if(menu->refresh != TRUE
			&& stat(apppath, &st) == 0
			&& st.st_mtime > menu->refresh_mti)
		menu->refresh = TRUE;
}
