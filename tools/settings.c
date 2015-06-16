/* $Id$ */
/* Copyright (c) 2015 Pierre Pronchery <khorben@defora.org> */
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
#include <gtk/gtk.h>
#include <System.h>
#include "../config.h"


/* constants */
#ifndef PROGNAME
# define PROGNAME	"settings"
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


/* private */
/* types */
typedef struct _Settings
{
	GtkWidget * window;
	GtkWidget * view;
} Settings;

/* prototypes */
static int _settings(void);

/* useful */
static int _settings_browse(Settings * settings);

static int _settings_error(char const * message, int ret);
static int _settings_usage(void);


/* functions */
/* settings */
/* callbacks */
static gboolean _settings_on_closex(gpointer data);
static gboolean _settings_on_idle(gpointer data);
#if GTK_CHECK_VERSION(2, 10, 0)
static void _settings_on_item_activated(GtkWidget * widget, GtkTreePath * path,
		gpointer data);
#endif

static int _settings(void)
{
	Settings settings;
	GtkWidget * widget;
	GtkListStore * store;

	settings.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(settings.window), 400, 300);
	gtk_window_set_icon_name(GTK_WINDOW(settings.window),
			GTK_STOCK_PREFERENCES);
	gtk_window_set_title(GTK_WINDOW(settings.window), "System preferences");
	g_signal_connect_swapped(settings.window, "delete-event", G_CALLBACK(
				_settings_on_closex), NULL);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	store = gtk_list_store_new(3,
			GDK_TYPE_PIXBUF,	/* icon */
			G_TYPE_STRING,		/* name */
			G_TYPE_STRING);		/* exec */
	settings.view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(settings.view), 96);
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(settings.view), 0);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(settings.view), 1);
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(settings.view),
			GTK_SELECTION_SINGLE);
#if GTK_CHECK_VERSION(2, 10, 0)
	g_signal_connect(settings.view, "item-activated", G_CALLBACK(
				_settings_on_item_activated), &settings);
#endif
	gtk_container_add(GTK_CONTAINER(widget), settings.view);
	gtk_container_add(GTK_CONTAINER(settings.window), widget);
	gtk_widget_show_all(settings.window);
	g_idle_add(_settings_on_idle, &settings);
	gtk_main();
	return 0;
}

static gboolean _settings_on_closex(gpointer data)
{
	gtk_main_quit();
	return FALSE;
}

static gboolean _settings_on_idle(gpointer data)
{
	Settings * settings = data;

	_settings_browse(settings);
	return FALSE;
}

#if GTK_CHECK_VERSION(2, 10, 0)
static void _settings_on_item_activated(GtkWidget * widget, GtkTreePath * path,
		gpointer data)
{
	Settings * settings = data;
	const GSpawnFlags flags = G_SPAWN_FILE_AND_ARGV_ZERO
		| G_SPAWN_SEARCH_PATH;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gchar * exec;
	GError * error = NULL;
	char * argv[] = { "/bin/sh", "sh", "-c", NULL, NULL };

	model = gtk_icon_view_get_model(GTK_ICON_VIEW(widget));
	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path) == FALSE)
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 2, &exec, -1);
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, exec);
# endif
	argv[3] = exec;
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, NULL, &error)
			== FALSE)
		_settings_error(error->message, 1);
	g_free(exec);
}
#endif


/* useful */
/* settings_browse */
static int _settings_browse_folder(Settings * settings, Config * config,
		char const * folder);
static int _settings_browse_folder_access(char const * filename, int mode);
static int _settings_browse_folder_access_path(char const * path,
		char const * filename, int mode);
static int _settings_browse_home(Settings * settings, Config * config);
static void _settings_rtrim(String * string, char c);

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
		return -_settings_error(error_get(), 1);
	model = gtk_icon_view_get_model(GTK_ICON_VIEW(settings->view));
	gtk_list_store_clear(GTK_LIST_STORE(model));
	/* read through every XDG application folder */
	if((path = getenv("XDG_DATA_DIRS")) == NULL || strlen(path) == 0)
	{
		path = "/usr/local/share:/usr/share";
		datadir = 0;
	}
	if((p = strdup(path)) == NULL)
		_settings_error(error_get(), 1);
	else
		for(i = 0, j = 0;; i++)
			if(p[i] == '\0')
			{
				_settings_rtrim(&p[j], '/');
				_settings_browse_folder(settings, config,
						&p[j]);
				datadir |= (strcmp(&p[j], DATADIR) == 0);
				break;
			}
			else if(p[i] == ':')
			{
				p[i] = '\0';
				_settings_rtrim(&p[j], '/');
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
	const char application[] = "Application";
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
		return -_settings_error(error_get(), 1);
	dir = opendir(path);
	string_delete(path);
	if(dir == NULL)
		return -_settings_error(strerror(errno), 1);
	theme = gtk_icon_theme_get_default();
	model = gtk_icon_view_get_model(GTK_ICON_VIEW(settings->view));
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
			_settings_error(error_get(), 1);
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
			_settings_error(error_get(), 1);
			continue;
		}
		p = config_get(config, section, "Type");
		name = config_get(config, section, "Name");
		exec = config_get(config, section, "Exec");
		if(p == NULL || name == NULL || exec == NULL)
			continue;
		if(strcmp(p, application) != 0)
			continue;
		if((p = config_get(config, section, "Categories")) == NULL
				|| string_find(p, "Settings") == NULL)
			continue;
		if((p = config_get(config, section, "TryExec")) != NULL
				&& _settings_browse_folder_access(path, X_OK)
				!= 0 && errno == ENOENT)
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
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pixbuf, 1, name, 2, exec,
				-1);
	}
	closedir(dir);
	return FALSE;
}

static int _settings_browse_folder_access(char const * path, int mode)
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
		return access(path, mode);
	if((p = getenv("PATH")) == NULL)
		return 0;
	if((q = string_new(p)) == NULL)
		return -1;
	errno = ENOENT;
	for(i = 0, j = 0;; i++)
		if(q[i] == '\0')
		{
			ret = _settings_browse_folder_access_path(&q[j], path,
					mode);
			break;
		}
		else if(q[i] == ':')
		{
			q[i] = '\0';
			if((ret = _settings_browse_folder_access_path(&q[j],
							path, mode)) == 0)
				break;
			j = i + 1;
		}
	string_delete(q);
	return ret;
}

static int _settings_browse_folder_access_path(char const * path,
		char const * filename, int mode)
{
	int ret;
	String * p;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", %d)\n", __func__, path,
			filename, mode);
#endif
	if((p = string_new_append(path, "/", filename, NULL)) == NULL)
		return -1;
	ret = access(p, mode);
	string_delete(p);
	return ret;
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
		return -_settings_error(error_get(), 1);
	ret = _settings_browse_folder(settings, config, p);
	free(p);
	return ret;
}

static void _settings_rtrim(String * string, char c)
{
	size_t len;

	for(len = string_length(string); len > 0; len--)
		if(string[len - 1] == c)
			string[len - 1] = '\0';
		else
			break;
}


/* settings_error */
static int _settings_error(char const * message, int ret)
{
	fprintf(stderr, "%s: %s\n", PROGNAME, message);
	return ret;
}


/* settings_usage */
static int _settings_usage(void)
{
	fputs("Usage: " PROGNAME "\n", stderr);
	return 1;
}


/* public */
/* main */
int main(int argc, char * argv[])
{
	int o;

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
