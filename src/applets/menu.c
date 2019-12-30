/* $Id$ */
/* Copyright (c) 2009-2018 Pierre Pronchery <khorben@defora.org> */
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
#include <Desktop.h>
#include "Panel/applet.h"
#include "../../config.h"
#define _(string) gettext(string)
#define N_(string) string

/* constants */
#ifndef PROGNAME_RUN
# define PROGNAME_RUN	"run"
#endif

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

typedef struct _MenuApp
{
	MimeHandler * handler;
	char * path;
} MenuApp;

typedef struct _MenuCategory
{
	char const * category;
	char const * label;
	char const * stock;
} MenuCategory;


/* constants */
static const MenuCategory _menu_categories[] =
{
	{ "Audio",	N_("Audio"),	"gnome-mime-audio",		},
	{ "Development",N_("Development"),"applications-development",	},
	{ "Education",	N_("Education"),"applications-science",		},
	{ "Game",	N_("Games"),	"applications-games",		},
	{ "Graphics",	N_("Graphics"),	"applications-graphics",	},
	{ "AudioVideo",	N_("Multimedia"),"applications-multimedia",	},
	{ "Network",	N_("Network"),	"applications-internet",	},
	{ "Office",	N_("Office"),	"applications-office",		},
	{ "Settings",	N_("Settings"),	"gnome-settings",		},
	{ "System",	N_("System"),	"applications-system",		},
	{ "Utility",	N_("Utilities"),"applications-utilities",	},
	{ "Video",	N_("Video"),	"video",			}
};
#define MENU_MENUS_COUNT (sizeof(_menu_categories) / sizeof(*_menu_categories))


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
static void _menu_on_about(gpointer data);
static void _menu_on_clicked(gpointer data);
static gboolean _menu_on_idle(gpointer data);
static void _menu_on_lock(gpointer data);
static void _menu_on_logout(gpointer data);
#ifdef EMBEDDED
static void _menu_on_rotate(gpointer data);
#endif
static void _menu_on_run(gpointer data);
static void _menu_on_shutdown(gpointer data);
static void _menu_on_suspend(gpointer data);
static gboolean _menu_on_timeout(gpointer data);

/* MenuApp */
static MenuApp * _menuapp_new(MimeHandler * handler, String const * path);
static void _menuapp_delete(MenuApp * menuapp);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Main menu"),
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
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	menu->helper = helper;
	menu->apps = NULL;
	menu->idle = g_idle_add(_menu_on_idle, menu);
	menu->refresh_mti = 0;
	menu->widget = gtk_button_new();
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	if((p = helper->config_get(helper->panel, "menu", "icon")) == NULL)
		p = applet.icon;
	image = gtk_image_new_from_icon_name(p,
			panel_window_get_icon_size(helper->window));
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);
	/* add some text if configured so */
	if((p = helper->config_get(helper->panel, "menu", "text")) != NULL
			&& strlen(p) > 0)
	{
		bold = pango_font_description_new();
		pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
		label = gtk_label_new(p);
#if GTK_CHECK_VERSION(3, 0, 0)
		gtk_widget_override_font(label, bold);
#else
		gtk_widget_modify_font(label, bold);
#endif
		pango_font_description_free(bold);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	}
	gtk_button_set_relief(GTK_BUTTON(menu->widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(menu->widget, _("Main menu"));
	g_signal_connect_swapped(menu->widget, "clicked", G_CALLBACK(
				_menu_on_clicked), menu);
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
	g_slist_foreach(menu->apps, (GFunc)_menuapp_delete, NULL);
	g_slist_free(menu->apps);
	gtk_widget_destroy(menu->widget);
	free(menu);
}


/* helpers */
/* menu_applications */
static void _applications_on_activate(gpointer data);
static void _applications_categories(GtkWidget * menu, GtkWidget ** menus);

static GtkWidget * _menu_applications(Menu * menu)
{
	GtkWidget * menus[MENU_MENUS_COUNT];
	GSList * p;
	GtkWidget * menushell;
	GtkWidget * menuitem;
	MenuApp * menuapp;
	MimeHandler * handler;
	char const * name;
#if GTK_CHECK_VERSION(2, 12, 0)
	char const * comment;
#endif
	char const * q;
	String const ** categories;
	size_t i;
	size_t j;

	if(menu->apps == NULL)
		_menu_on_idle(menu);
	memset(&menus, 0, sizeof(menus));
	menushell = gtk_menu_new();
	for(p = menu->apps; p != NULL; p = p->next)
	{
		menuapp = p->data;
		handler = menuapp->handler;
		if((name = mimehandler_get_name(handler, 1)) == NULL)
		{
			menu->helper->error(NULL, error_get(NULL), 0);
			continue;
		}
#if GTK_CHECK_VERSION(2, 12, 0)
		comment = mimehandler_get_comment(handler, 1);
#endif
		if((q = mimehandler_get_generic_name(handler, 1)) != NULL)
		{
#if GTK_CHECK_VERSION(2, 12, 0)
			if(comment == NULL)
				comment = name;
#endif
			name = q;
		}
		menuitem = _menu_menuitem(menu, menuapp->path, name,
				mimehandler_get_icon(handler, 1));
#if GTK_CHECK_VERSION(2, 12, 0)
		if(comment != NULL)
			gtk_widget_set_tooltip_text(menuitem, comment);
#endif
		if(mimehandler_get_type(handler)
				== MIMEHANDLER_TYPE_APPLICATION
				&& mimehandler_can_execute(handler) == 0)
			gtk_widget_set_sensitive(menuitem, FALSE);
		else
			g_signal_connect_swapped(menuitem, "activate",
					G_CALLBACK(_applications_on_activate),
					handler);
		if((categories = mimehandler_get_categories(handler)) == NULL
				|| categories[0] == NULL)
		{
			gtk_menu_shell_append(GTK_MENU_SHELL(menushell),
					menuitem);
			continue;
		}
		for(i = 0; i < MENU_MENUS_COUNT; i++)
		{
			for(j = 0; categories[j] != NULL; j++)
				if(string_compare(_menu_categories[i].category,
							categories[j]) == 0)
					break;
			if(categories[j] != NULL)
				break;
		}
		if(i == MENU_MENUS_COUNT)
			gtk_menu_shell_append(GTK_MENU_SHELL(menushell),
					menuitem);
		else
		{
			if(menus[i] == NULL)
				menus[i] = gtk_menu_new();
			gtk_menu_shell_append(GTK_MENU_SHELL(menus[i]),
					menuitem);
		}
	}
	_applications_categories(menushell, menus);
	return menushell;
}

static void _applications_on_activate(gpointer data)
{
	MimeHandler * handler = data;

	if(mimehandler_open(handler, NULL) != 0)
		/* XXX really report error */
		error_print(NULL);
}

static void _applications_categories(GtkWidget * menu, GtkWidget ** menus)
{
	size_t i;
	MenuCategory const * m;
	GtkWidget * menuitem;
	size_t pos = 0;

	for(i = 0; i < MENU_MENUS_COUNT; i++)
	{
		if(menus[i] == NULL)
			continue;
		m = &_menu_categories[i];
		menuitem = _menu_menuitem_stock(m->stock, _(m->label), FALSE);
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
		path = "/usr/local/share:" DATADIR ":/usr/share";
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
/* menu_on_about */
static void _menu_on_about(gpointer data)
{
	Menu * menu = data;

	menu->helper->about_dialog(menu->helper->panel);
}


/* menu_on_clicked */
static void _clicked_position_menu(GtkMenu * widget, gint * x, gint * y,
		gboolean * push_in, gpointer data);

static void _menu_on_clicked(gpointer data)
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
					_menu_on_run), menu);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	if((p = helper->config_get(helper->panel, "menu", "about")) == NULL
			|| strtol(p, NULL, 0) != 0)
	{
#if GTK_CHECK_VERSION(3, 10, 0)
		menuitem = gtk_image_menu_item_new_with_mnemonic(_("_About"));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
				gtk_image_new_from_icon_name(GTK_STOCK_ABOUT,
					GTK_ICON_SIZE_MENU));
#else
		menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT,
				NULL);
#endif
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_menu_on_about), menu);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	/* lock screen */
	menuitem = _menu_menuitem_stock("gnome-lockscreen", _("_Lock screen"),
			TRUE);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_menu_on_lock), menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
#ifdef EMBEDDED
	/* rotate screen */
	/* XXX find a more appropriate icon */
	menuitem = _menu_menuitem_stock(GTK_STOCK_REFRESH, _("R_otate"), TRUE);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_menu_on_rotate), data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
#endif
	/* logout */
	if(menu->helper->logout_dialog != NULL)
	{
		menuitem = _menu_menuitem_stock("gnome-logout", _("Lo_gout..."),
				TRUE);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_menu_on_logout), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	/* suspend */
	if(menu->helper->suspend != NULL)
	{
		menuitem = _menu_menuitem_stock("gtk-media-pause",
				_("S_uspend"), TRUE);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_menu_on_suspend), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	/* shutdown */
	if(menu->helper->shutdown_dialog != NULL)
	{
		menuitem = _menu_menuitem_stock("gnome-shutdown",
				_("_Shutdown..."), TRUE);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_menu_on_shutdown), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menushell), menuitem);
	}
	gtk_widget_show_all(menushell);
	gtk_menu_popup(GTK_MENU(menushell), NULL, NULL, _clicked_position_menu,
			menu, 0, gtk_get_current_event_time());
}

static void _clicked_position_menu(GtkMenu * widget, gint * x, gint * y,
		gboolean * push_in, gpointer data)
{
	Menu * menu = data;
	GtkAllocation a;

	gtk_widget_get_allocation(menu->widget, &a);
	*x = a.x;
	*y = a.y;
	menu->helper->position_menu(menu->helper->panel, widget, x, y, push_in);
}


/* menu_on_idle */
static gint _idle_apps_compare(gconstpointer a, gconstpointer b);
static void _idle_path(Menu * menu, char const * path, char const * apppath);

static gboolean _menu_on_idle(gpointer data)
{
	const int timeout = 10000;
	Menu * menu = data;

	if(menu->apps != NULL)
	{
		menu->idle = 0;
		return FALSE;
	}
	_menu_xdg_dirs(menu, _idle_path);
	menu->idle = g_timeout_add(timeout, _menu_on_timeout, menu);
	return FALSE;
}

static gint _idle_apps_compare(gconstpointer a, gconstpointer b)
{
	MenuApp * maa = (MenuApp *)a;
	MenuApp * mab = (MenuApp *)b;
	MimeHandler * mha = maa->handler;
	MimeHandler * mhb = mab->handler;
	String const * mhas;
	String const * mhbs;

	if((mhas = mimehandler_get_generic_name(mha, 1)) == NULL)
		mhas = mimehandler_get_name(mha, 1);
	if((mhbs = mimehandler_get_generic_name(mhb, 1)) == NULL)
		mhbs = mimehandler_get_name(mhb, 1);
	return string_compare(mhas, mhbs);
}

static void _idle_path(Menu * menu, char const * path, char const * apppath)
{
	DIR * dir;
	int fd;
	struct stat st;
	struct dirent * de;
	size_t len;
	const char ext[] = ".desktop";
	char * name = NULL;
	char * p;
	MimeHandler * handler;
	MenuApp * menuapp;
	(void) path;

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
		if((handler = mimehandler_new_load(name)) == NULL)
		{
			menu->helper->error(NULL, error_get(NULL), 1);
			continue;
		}
		/* skip this entry if cannot be displayed or opened */
		if(mimehandler_can_display(handler) == 0
				|| mimehandler_can_execute(handler) == 0
				|| (menuapp = _menuapp_new(handler, path))
				== NULL)
		{
			mimehandler_delete(handler);
			continue;
		}
		menu->apps = g_slist_insert_sorted(menu->apps, menuapp,
				_idle_apps_compare);
	}
	free(name);
	closedir(dir);
}


/* menu_on_lock */
static void _menu_on_lock(gpointer data)
{
	Menu * menu = data;

	menu->helper->lock(menu->helper->panel);
}


/* menu_on_logout */
static void _menu_on_logout(gpointer data)
{
	Menu * menu = data;

	menu->helper->logout_dialog(menu->helper->panel);
}


#ifdef EMBEDDED
/* menu_on_rotate */
static void _menu_on_rotate(gpointer data)
{
	Menu * menu = data;

	menu->helper->rotate_screen(menu->helper->panel);
}
#endif


/* menu_on_run */
static void _menu_on_run(gpointer data)
{
	Menu * menu = data;
	char * argv[] = { BINDIR "/" PROGNAME_RUN, NULL };
	const unsigned int flags = G_SPAWN_STDOUT_TO_DEV_NULL
		| G_SPAWN_STDERR_TO_DEV_NULL;
	GError * error = NULL;

	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			!= TRUE)
	{
		menu->helper->error(menu->helper->panel, error->message, 1);
		g_error_free(error);
	}
}


/* menu_on_shutdown */
static void _menu_on_shutdown(gpointer data)
{
	Menu * menu = data;

	menu->helper->shutdown_dialog(menu->helper->panel);
}


/* menu_on_suspend */
static void _menu_on_suspend(gpointer data)
{
	Menu * menu = data;

	menu->helper->suspend(menu->helper->panel);
}


/* menu_on_timeout */
static void _timeout_path(Menu * menu, char const * path, char const * apppath);

static gboolean _menu_on_timeout(gpointer data)
{
	Menu * menu = data;

	menu->refresh = FALSE;
	_menu_xdg_dirs(menu, _timeout_path);
	if(menu->refresh == FALSE)
		return TRUE;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() resetting the menu\n", __func__);
#endif
	g_slist_foreach(menu->apps, (GFunc)_menuapp_delete, NULL);
	g_slist_free(menu->apps);
	menu->apps = NULL;
	menu->idle = g_idle_add(_menu_on_idle, menu);
	return FALSE;
}

static void _timeout_path(Menu * menu, char const * path, char const * apppath)
{
	struct stat st;
	(void) path;

	if(menu->refresh != TRUE
			&& stat(apppath, &st) == 0
			&& st.st_mtime > menu->refresh_mti)
		menu->refresh = TRUE;
}


/* MenuApp */
/* menuapp_new */
static MenuApp * _menuapp_new(MimeHandler * handler, String const * path)
{
	MenuApp * menuapp;

	if((menuapp = object_new(sizeof(*menuapp))) == NULL)
		return NULL;
	menuapp->handler = NULL;
	if(path == NULL)
		menuapp->path = NULL;
	else if((menuapp->path = string_new(path)) == NULL)
	{
		_menuapp_delete(menuapp);
		return NULL;
	}
	menuapp->handler = handler;
	return menuapp;
}


/* menuapp_delete */
static void _menuapp_delete(MenuApp * menuapp)
{
	if(menuapp->handler != NULL)
		mimehandler_delete(menuapp->handler);
	string_delete(menuapp->path);
	object_delete(menuapp);
}
