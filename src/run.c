/* $Id$ */
/* Copyright (c) 2006-2021 Pierre Pronchery <khorben@defora.org> */
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
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME_RUN
# define PROGNAME_RUN	"run"
#endif
#ifndef PROGNAME_XTERM
# define PROGNAME_XTERM	"xterm"
#endif

#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* run */
/* private */
/* types */
typedef struct _Run
{
	Config * config;
	gboolean terminal;
	GPid pid;		/* current child */
	guint source;

	/* widgets */
	GtkWidget * window;
	GtkWidget * entry;
} Run;


/* constants */
#define RUN_CONFIG_FILE	".runrc"


/* functions */
/* useful */
static int _run_error(Run * run, char const * message, int ret);
static char * _run_get_config_filename(void);

/* callbacks */
static gboolean _on_run_closex(gpointer data);
static void _on_run_cancel(gpointer data);
static void _on_run_choose_activate(GtkWidget * widget, gint arg1,
		gpointer data);
static void _on_run_execute(gpointer data);
static void _on_run_path_activate(gpointer data);
static void _on_run_terminal_toggle(GtkWidget * widget, gpointer data);

/* run_new */
static GtkWidget * _new_entry(Config * config);

static Run * _run_new(void)
{
	Run * run;
	GtkWindow * window;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * widget;
	GtkSizeGroup * group;
	GtkFileFilter * filter;

	if((run = object_new(sizeof(*run))) == NULL)
		return NULL;
	run->config = config_new();
	run->terminal = FALSE;
	run->pid = -1;
	run->source = 0;
	run->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	window = GTK_WINDOW(run->window);
	gtk_window_set_keep_above(window, TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(window, GTK_STOCK_EXECUTE);
#endif
	gtk_window_set_position(window, GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(window, FALSE);
	gtk_window_set_skip_pager_hint(window, TRUE);
	gtk_window_set_title(window, _("Run program..."));
	g_signal_connect_swapped(window, "delete-event", G_CALLBACK(
				_on_run_closex), run);
	group = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
#if GTK_CHECK_VERSION(3, 0, 0)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	vbox = gtk_vbox_new(FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 0);
#endif
	widget = gtk_label_new(_("Command:"));
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 4);
	run->entry = _new_entry(run->config);
	g_signal_connect_swapped(run->entry, "activate", G_CALLBACK(
				_on_run_path_activate), run);
	gtk_box_pack_start(GTK_BOX(hbox), run->entry, TRUE, TRUE, 4);
	/* file chooser */
	widget = gtk_file_chooser_dialog_new(_("Run program..."), window,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Executable files"));
	gtk_file_filter_add_mime_type(filter, "application/x-executable");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(widget), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Perl scripts"));
	gtk_file_filter_add_mime_type(filter, "application/x-perl");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Python scripts"));
	gtk_file_filter_add_mime_type(filter, "text/x-python");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Shell scripts"));
	gtk_file_filter_add_mime_type(filter, "application/x-shellscript");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), filter);
	g_signal_connect(widget, "response", G_CALLBACK(
				_on_run_choose_activate), run);
	widget = gtk_file_chooser_button_new_with_dialog(widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, FALSE, 4);
	widget = gtk_check_button_new_with_label(_("Run in a terminal"));
	g_signal_connect(widget, "toggled", G_CALLBACK(
				_on_run_terminal_toggle), run);
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
	hbox = gtk_hbox_new(FALSE, 0);
#endif
#if GTK_CHECK_VERSION(3, 10, 0)
	widget = gtk_button_new_from_icon_name(GTK_STOCK_EXECUTE,
			GTK_ICON_SIZE_BUTTON);
	gtk_button_set_label(GTK_BUTTON(widget), _("Run"));
#else
	widget = gtk_button_new_from_stock(GTK_STOCK_EXECUTE);
#endif
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(
				_on_run_execute), run);
	gtk_size_group_add_widget(group, widget);
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, TRUE, 4);
#if GTK_CHECK_VERSION(3, 10, 0)
	widget = gtk_button_new_from_icon_name(GTK_STOCK_CANCEL,
			GTK_ICON_SIZE_BUTTON);
	gtk_button_set_label(GTK_BUTTON(widget), _("Cancel"));
#else
	widget = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#endif
	gtk_size_group_add_widget(group, widget);
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(_on_run_cancel),
			run);
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);
	gtk_container_add(GTK_CONTAINER(run->window), vbox);
	gtk_widget_show_all(run->window);
	return run;
}

static GtkWidget * _new_entry(Config * config)
{
	GtkWidget * entry;
	char * p;
	char const * q;
	GtkEntryCompletion * completion;
	GtkListStore * store;
	int i;
	char buf[16];
	GtkTreeIter iter;

	entry = gtk_entry_new();
	if(config == NULL)
		return entry;
	if((p = _run_get_config_filename()) == NULL)
		return entry;
	if(config_load(config, p) != 0)
		error_print(PROGNAME_RUN);
	free(p);
	completion = gtk_entry_completion_new();
	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	g_object_unref(completion);
	store = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_entry_completion_set_text_column(completion, 0);
	for(i = 0; i < 100; i++)
	{
		snprintf(buf, sizeof(buf), "%s%d", "command", i);
		if((q = config_get(config, "commands", buf)) == NULL)
			continue;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, q, -1);
	}
	return entry;
}


/* run_delete */
static void _run_delete(Run * run)
{
	if(run->pid >= 0)
		g_spawn_close_pid(run->pid);
	if(run->config != NULL)
		config_delete(run->config);
	object_delete(run);
}


/* useful */
/* run_error */
static int _run_error(Run * run, char const * message, int ret)
{
	GtkWidget * dialog;
	GtkWindow * window = (run != NULL) ? GTK_WINDOW(run->window) : NULL;

	dialog = gtk_message_dialog_new(window, 0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s",
#endif
			message);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	gtk_widget_show(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return ret;
}


/* run_get_config_filename */
static char * _run_get_config_filename(void)
{
	char const * homedir;

	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	return string_new_append(homedir, "/", RUN_CONFIG_FILE, NULL);
}


/* callbacks */
/* on_run_closex */
static gboolean _on_run_closex(gpointer data)
{
	Run * run = data;

	gtk_widget_hide(run->window);
	gtk_main_quit();
	return FALSE;
}


/* on_run_cancel */
static void _on_run_cancel(gpointer data)
{
	Run * run = data;

	gtk_widget_hide(run->window);
	gtk_main_quit();
}


/* on_run_choose_activate */
static void _on_run_choose_activate(GtkWidget * widget, gint arg1,
		gpointer data)
{
	Run * run = data;

	if(arg1 != GTK_RESPONSE_ACCEPT)
		return;
	gtk_entry_set_text(GTK_ENTRY(run->entry), gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(widget)));
}


/* on_run_execute */
static void _execute_save_config(Run * run);
static gboolean _execute_timeout(gpointer data);
static void _execute_watch(GPid pid, gint status, gpointer data);
static void _execute_watch_cleanup(Run * run);

static void _on_run_execute(gpointer data)
{
	const unsigned int flags = G_SPAWN_FILE_AND_ARGV_ZERO
		| G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
	const unsigned int timeout = 30000;
	Run * run = data;
	char * argv_shell[] = { "/bin/sh", PROGNAME_RUN, "-c", NULL, NULL };
	char * argv_xterm[] = { PROGNAME_XTERM, PROGNAME_XTERM, "-e", "sh",
		"-c", NULL, NULL };
	char const * xterm;
	char ** argv = argv_shell;
	char * p = NULL;
	char const * q;
	GError * error = NULL;

	q = gtk_entry_get_text(GTK_ENTRY(run->entry));
	if((argv_shell[3] = strdup(q)) == NULL)
	{
		_run_error(run, strerror(errno), 1);
		return;
	}
	if(run->terminal)
	{
		if((xterm = config_get(run->config, NULL, "xterm")) != NULL)
		{
			if((p = strdup(xterm)) == NULL)
			{
				_run_error(run, strerror(errno), 1);
				return;
			}
			argv_xterm[0] = xterm;
			argv_xterm[1] = basename(p);
		}
		argv_xterm[5] = argv_shell[3];
		argv = argv_xterm;
	}
	if(g_spawn_async(NULL, argv, NULL, flags, NULL, NULL, &run->pid,
				&error) == FALSE)
	{
		_run_error(run, error->message, 1);
		g_error_free(error);
		free(argv_shell[3]);
		free(p);
		return;
	}
	free(argv_shell[3]);
	free(p);
	gtk_widget_hide(run->window);
	g_child_watch_add(run->pid, _execute_watch, run);
	run->source = g_timeout_add(timeout, _execute_timeout, run);
}

static void _execute_save_config(Run * run)
{
	char const * p;
	int i;
	char buf[11];
	char const * q;
	char * filename;

	if((filename = _run_get_config_filename()) == NULL)
		return;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	config_reset(run->config);
	if(config_load(run->config, filename) != 0)
		/* XXX this risks losing the configuration */
		error_print(PROGNAME_RUN);
	p = gtk_entry_get_text(GTK_ENTRY(run->entry));
	for(i = 0; i < 100; i++)
	{
		snprintf(buf, sizeof(buf), "%s%d", "command", i);
		q = config_get(run->config, "commands", buf);
		if(q == NULL || strcmp(p, q) != 0)
			continue;
		free(filename); /* the command is already known */
		return;
	}
	for(i = 0; i < 100; i++)
	{
		snprintf(buf, sizeof(buf), "%s%d", "command", i);
		q = config_get(run->config, "commands", buf);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() config_set(config, %s, %s, %s)\n",
				__func__, "\"\"", buf, p);
#endif
		/* FIXME this invalidates q and then p in the next iteration */
		config_set(run->config, "commands", buf, p);
		if((p = q) == NULL)
			break;
	}
	if(config_save(run->config, filename) != 0)
		error_print(PROGNAME_RUN);
	free(filename);
}

static gboolean _execute_timeout(gpointer data)
{
	Run * run = data;

	run->source = 0;
	_execute_save_config(run);
	gtk_main_quit();
	return FALSE;
}

static void _execute_watch(GPid pid, gint status, gpointer data)
{
	Run * run = data;
	char const * error;

	if(run->pid != pid)
		return; /* should not happen */
	if(WIFEXITED(status))
		switch(WEXITSTATUS(status))
		{
			case 127:
				error = _("Command not found");
				break;
			case 126:
				error = _("Permission denied");
				break;
			default:
				_execute_watch_cleanup(run);
				return;
		}
	else if(WIFSIGNALED(status))
	{
		_execute_watch_cleanup(run);
		return;
	}
	else
		return;
	if(run->source != 0)
		g_source_remove(run->source);
	run->source = 0;
	gtk_widget_show(run->window);
	_run_error(run, error, 0);
}

static void _execute_watch_cleanup(Run * run)
{
	g_spawn_close_pid(run->pid);
	run->pid = -1;
	_execute_save_config(run);
	gtk_main_quit();
}


/* on_run_path_activate */
static void _on_run_path_activate(gpointer data)
{
	_on_run_execute(data);
}


/* on_run_terminal_toggle */
static void _on_run_terminal_toggle(GtkWidget * widget, gpointer data)
{
	Run * run = data;

	run->terminal = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s\n"), PROGNAME_RUN);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	Run * run;

	if(setlocale(LC_ALL, "") == NULL)
		fprintf(stderr, PROGNAME_RUN ": %s: %s\n", "setlocale",
				strerror(errno));
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "")) != -1)
		switch(o)
		{
			default:
				return _usage();
		}
	if(optind != argc)
		return _usage();
	if((run = _run_new()) == NULL)
		return _run_error(NULL, error_get(NULL), 2);
	gtk_main();
	_run_delete(run);
	return 0;
}
