/* $Id$ */
/* Copyright (c) 2015-2018 Pierre Pronchery <khorben@defora.org> */
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <gdk/gdkkeysyms.h>
#include <System.h>
#include <Desktop.h>
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME_SETTINGS
# define PROGNAME_SETTINGS	"settings"
#endif

#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef BINDIR
# define BINDIR			PREFIX "/bin"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR		DATADIR "/locale"
#endif


/* settings */
/* private */
/* types */
typedef struct _Settings
{
	GtkWidget * window;
	GtkWidget * view;
} Settings;

/* constants */
typedef enum _SettingsColumn
{
	SC_ICON = 0,
	SC_NAME,
	SC_EXEC,
	SC_PRIVILEGED
} SettingsColumn;
#define SC_LAST SC_PRIVILEGED
#define SC_COUNT (SC_LAST + 1)

/* prototypes */
static int _settings(void);

/* accessors */
static int _mimehandler_can_display(Config * config);
static int _mimehandler_can_execute(Config * config);
static int _mimehandler_is_deleted(Config * config);
static gboolean _settings_get_iter(Settings * settings, GtkTreeIter * iter,
		GtkTreePath * path);
static GtkTreeModel * _settings_get_model(Settings * settings);

/* useful */
static int _settings_browse(Settings * settings);

static int _settings_error(char const * message, int ret);
static int _settings_usage(void);

/* callbacks */
static void _settings_on_close(gpointer data);


/* constants */
static const DesktopAccel _settings_accel[] =
{
	{ G_CALLBACK(_settings_on_close), GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, 0, 0 }
};


/* functions */
/* settings */
/* callbacks */
static gboolean _settings_on_closex(gpointer data);
static gboolean _settings_on_filter_view(GtkTreeModel * model,
		GtkTreeIter * iter, gpointer data);
static gboolean _settings_on_idle(gpointer data);
static void _settings_on_item_activated(GtkWidget * widget, GtkTreePath * path,
		gpointer data);

static int _settings(void)
{
	Settings settings;
	GtkAccelGroup * accel;
	GtkWidget * widget;
	GtkListStore * store;
	GtkTreeModel * model;

	accel = gtk_accel_group_new();
	settings.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_add_accel_group(GTK_WINDOW(settings.window), accel);
	desktop_accel_create(_settings_accel, &settings, accel);
	g_object_unref(accel);
	gtk_window_set_default_size(GTK_WINDOW(settings.window), 400, 300);
	gtk_window_set_icon_name(GTK_WINDOW(settings.window),
			GTK_STOCK_PREFERENCES);
	gtk_window_set_title(GTK_WINDOW(settings.window),
			_("System preferences"));
	g_signal_connect_swapped(settings.window, "delete-event", G_CALLBACK(
				_settings_on_closex), &settings);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	store = gtk_list_store_new(SC_COUNT,
			GDK_TYPE_PIXBUF,	/* SC_ICON */
			G_TYPE_STRING,		/* SC_NAME */
			G_TYPE_STRING,		/* SC_EXEC */
			G_TYPE_BOOLEAN);	/* SC_PRIVILEGED */
	model = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model),
			_settings_on_filter_view, &settings, NULL);
	model = gtk_tree_model_sort_new_with_model(model);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), SC_NAME,
			GTK_SORT_ASCENDING);
	settings.view = gtk_icon_view_new_with_model(model);
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(settings.view), 96);
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(settings.view), SC_ICON);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(settings.view), SC_NAME);
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(settings.view),
			GTK_SELECTION_SINGLE);
	g_signal_connect(settings.view, "item-activated", G_CALLBACK(
				_settings_on_item_activated), &settings);
	gtk_container_add(GTK_CONTAINER(widget), settings.view);
	gtk_container_add(GTK_CONTAINER(settings.window), widget);
	gtk_widget_show_all(settings.window);
	g_idle_add(_settings_on_idle, &settings);
	gtk_main();
	return 0;
}

static gboolean _settings_on_closex(gpointer data)
{
	Settings * settings = data;

	_settings_on_close(settings);
	return FALSE;
}

static gboolean _settings_on_filter_view(GtkTreeModel * model,
		GtkTreeIter * iter, gpointer data)
{
	gboolean privileged;
	(void) data;

	if(geteuid() == 0)
		return TRUE;
	gtk_tree_model_get(model, iter, SC_PRIVILEGED, &privileged, -1);
	return (SC_PRIVILEGED == TRUE) ? FALSE : TRUE;
}

static gboolean _settings_on_idle(gpointer data)
{
	Settings * settings = data;

	_settings_browse(settings);
	return FALSE;
}

static void _settings_on_item_activated(GtkWidget * widget, GtkTreePath * path,
		gpointer data)
{
	Settings * settings = data;
	const unsigned int flags = G_SPAWN_FILE_AND_ARGV_ZERO;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * exec;
	GError * error = NULL;
	char * argv[] = { "/bin/sh", "sh", "-c", NULL, NULL };
	(void) widget;

	if(_settings_get_iter(settings, &iter, path) == FALSE)
		return;
	model = _settings_get_model(settings);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, SC_EXEC, &exec, -1);
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, exec);
# endif
	argv[3] = exec;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			== FALSE)
		_settings_error(error->message, 1);
	g_free(exec);
}


/* accessors */
/* mimehandler_can_display */
static gboolean _mimehandler_can_display(Config * config)
{
	const char section[] = "Desktop Entry";
	String const * p;

	/* FIXME re-implement with libDesktop's MimeHandler class */
	if(_mimehandler_is_deleted(config))
		return 0;
	if(config_get(config, section, "OnlyShowIn") != NULL)
		return 0;
	return ((p = config_get(config, section, "NoDisplay")) == NULL
			|| string_compare(p, "true") != 0) ? 1 : 0;
}


/* mimehandler_can_execute */
static int _can_execute_access(char const * path, int mode);
static int _can_execute_access_path(char const * path, char const * filename,
		int mode);

static int _mimehandler_can_execute(Config * config)
{
	const char section[] = "Desktop Entry";
	String const * p;

	/* FIXME re-implement with libDesktop's MimeHandler class */
	if((p = config_get(config, section, "Type")) == NULL)
		return 0;
	if(string_compare(p, "Application") != 0)
		return 0;
	if((p = config_get(config, section, "TryExec")) != NULL
			&& _can_execute_access(p, X_OK) <= 0)
		return 0;
	return (config_get(config, section, "Exec") != NULL) ? 1 : 0;
}

static int _can_execute_access(char const * path, int mode)
{
	int ret = -1;
	char const * p;
	char * q;
	size_t i;
	size_t j;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %d)\n", __func__, path, mode);
#endif
	if(path[0] == '/')
		return (access(path, mode) == 0) ? 1 : 0;
	if((p = getenv("PATH")) == NULL)
		return 0;
	if((q = string_new(p)) == NULL)
		return -1;
	errno = ENOENT;
	for(i = 0, j = 0;; i++)
		if(q[i] == '\0')
		{
			ret = _can_execute_access_path(&q[j], path, mode);
			break;
		}
		else if(q[i] == ':')
		{
			q[i] = '\0';
			if((ret = _can_execute_access_path(&q[j], path, mode))
					> 0)
				break;
			j = i + 1;
		}
	string_delete(q);
	return ret;
}

static int _can_execute_access_path(char const * path, char const * filename,
		int mode)
{
	int ret;
	String * p;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", %d)\n", __func__, path,
			filename, mode);
#endif
	if((p = string_new_append(path, "/", filename, NULL)) == NULL)
		return -1;
	ret = (access(p, mode) == 0) ? 1 : 0;
	string_delete(p);
	return ret;
}


/* mimehandler_is_deleted */
static int _mimehandler_is_deleted(Config * config)
{
	const char section[] = "Desktop Entry";
	String const * p;

	/* FIXME re-implement with libDesktop's MimeHandler class */
	if((p = config_get(config, section, "Hidden")) != NULL
			&& string_compare(p, "true") == 0)
		return 1;
	if((p = config_get(config, section, "Type")) == NULL)
		return 1;
	if(string_compare(p, "Application") == 0)
		if(_mimehandler_can_execute(config) <= 0)
			return 1;
	return 0;
}


/* settings_get_iter */
static gboolean _settings_get_iter(Settings * settings, GtkTreeIter * iter,
		GtkTreePath * path)
{
	GtkTreeModel * model;
	GtkTreeIter p;

	model = gtk_icon_view_get_model(GTK_ICON_VIEW(settings->view));
	if(gtk_tree_model_get_iter(model, iter, path) == FALSE)
		return FALSE;
	gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT(
				model), &p, iter);
	model = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(model));
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(
				model), iter, &p);
	return TRUE;
}


/* settings_get_model */
static GtkTreeModel * _settings_get_model(Settings * settings)
{
	GtkTreeModel * model;

	model = gtk_icon_view_get_model(GTK_ICON_VIEW(settings->view));
	model = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(model));
	return gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));
}


/* useful */
/* settings_browse */
static int _settings_browse_folder(Settings * settings, Config * config,
		char const * folder);
static int _settings_browse_home(Settings * settings, Config * config);

static int _settings_browse(Settings * settings)
{
	int ret = 0;
	Config * config;
	GtkTreeModel * model;
	char const * path;
	char * p;
	size_t i;
	size_t j;
	int datadir = 1;

	if((config = config_new()) == NULL)
		return -_settings_error(error_get(NULL), 1);
	model = _settings_get_model(settings);
	gtk_list_store_clear(GTK_LIST_STORE(model));
	/* read through every XDG application folder */
	if((path = getenv("XDG_DATA_DIRS")) == NULL || strlen(path) == 0)
	{
		path = "/usr/local/share:/usr/share";
		datadir = 0;
	}
	if((p = strdup(path)) == NULL)
		_settings_error(error_get(NULL), 1);
	else
		for(i = 0, j = 0;; i++)
			if(p[i] == '\0')
			{
				string_rtrim(&p[j], "/");
				_settings_browse_folder(settings, config,
						&p[j]);
				datadir |= (strcmp(&p[j], DATADIR) == 0);
				break;
			}
			else if(p[i] == ':')
			{
				p[i] = '\0';
				string_rtrim(&p[j], "/");
				_settings_browse_folder(settings, config,
						&p[j]);
				datadir |= (strcmp(&p[j], DATADIR) == 0);
				j = i + 1;
			}
	free(p);
	if(datadir == 0)
		ret = _settings_browse_folder(settings, config, DATADIR);
	ret |= _settings_browse_home(settings, config);
	config_delete(config);
	return ret;
}

static int _settings_browse_folder(Settings * settings, Config * config,
		char const * folder)
{
	const char ext[8] = ".desktop";
	const char section[] = "Desktop Entry";
	const int flags = GTK_ICON_LOOKUP_FORCE_SIZE;
	const gint iconsize = 48;
	GtkIconTheme * theme;
	GtkTreeModel * model;
	GtkListStore * store;
	DIR * dir;
	struct dirent * de;
	size_t len;
	String * path;
	int res;
	String const * name;
	String const * icon;
	String const * exec;
	String const * p;
	GdkPixbuf * pixbuf;
	GtkTreeIter iter;
	GError * error = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, folder);
#endif
	if((path = string_new_append(folder, "/applications", NULL)) == NULL)
		return -_settings_error(error_get(NULL), 1);
	dir = opendir(path);
	string_delete(path);
	if(dir == NULL)
		return -_settings_error(strerror(errno), 1);
	theme = gtk_icon_theme_get_default();
	model = _settings_get_model(settings);
	store = GTK_LIST_STORE(model);
	while((de = readdir(dir)) != NULL)
	{
		if((len = strlen(de->d_name)) <= sizeof(ext))
			continue;
		if(strncmp(&de->d_name[len - sizeof(ext)], ext, sizeof(ext))
				!= 0)
			continue;
		if((path = string_new_append(folder, "/applications/",
						de->d_name, NULL)) == NULL)
		{
			_settings_error(error_get(NULL), 1);
			continue;
		}
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, path);
#endif
		config_reset(config);
		res = config_load(config, path);
		string_delete(path);
		if(res != 0)
		{
			_settings_error(error_get(NULL), 1);
			continue;
		}
		if(_mimehandler_can_display(config) == 0)
			continue;
		name = config_get(config, section, "Name");
		exec = config_get(config, section, "Exec");
		if(name == NULL || exec == NULL
				|| strcmp(exec, PROGNAME_SETTINGS) == 0)
			continue;
		if((p = config_get(config, section, "Categories")) == NULL
				|| string_find(p, "Settings") == NULL)
			continue;
		if(_mimehandler_can_execute(config) <= 0)
			continue;
		if((icon = config_get(config, section, "Icon")) == NULL)
			icon = GTK_STOCK_PREFERENCES;
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\" %s\n", __func__, name,
				icon);
#endif
		if((pixbuf = gtk_icon_theme_load_icon(theme, icon, iconsize,
						flags, &error)) == NULL)
		{
			_settings_error(error->message, 0);
			g_error_free(error);
			error = NULL;
		}
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_list_store_insert_with_values(store, &iter, -1,
#else
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
#endif
				SC_ICON, pixbuf, SC_NAME, name, SC_EXEC, exec,
				/* FIXME detect privileged settings */
				SC_PRIVILEGED, FALSE, -1);
	}
	closedir(dir);
	return FALSE;
}

static int _settings_browse_home(Settings * settings, Config * config)
{
	int ret;
	char const fallback[] = ".local/share";
	char const * path;
	char const * homedir;
	String * p;

	/* use $XDG_DATA_HOME if set and not empty */
	if((path = getenv("XDG_DATA_HOME")) != NULL && strlen(path) > 0)
		return _settings_browse_folder(settings, config, path);
	/* fallback to "$HOME/.local/share" */
	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	if((p = string_new_append(homedir, "/", fallback, NULL)) == NULL)
		return -_settings_error(error_get(NULL), 1);
	ret = _settings_browse_folder(settings, config, p);
	free(p);
	return ret;
}


/* settings_error */
static int _settings_error(char const * message, int ret)
{
	fprintf(stderr, "%s: %s\n", PROGNAME_SETTINGS, message);
	return ret;
}


/* settings_usage */
static int _settings_usage(void)
{
	fprintf(stderr, _("Usage: %s\n"), PROGNAME_SETTINGS);
	return 1;
}


/* callbacks */
static void _settings_on_close(gpointer data)
{
	Settings * settings = data;

	gtk_widget_hide(settings->window);
	gtk_main_quit();
}


/* public */
/* main */
int main(int argc, char * argv[])
{
	int o;

	if(setlocale(LC_ALL, "") == NULL)
		_settings_error(strerror(errno), 1); /* XXX mention setlocale */
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "")) != -1)
		switch(o)
		{
			default:
				return _settings_usage();
		}
	if(optind != argc)
		return _settings_usage();
	return (_settings() == 0) ? 0 : 2;
}
