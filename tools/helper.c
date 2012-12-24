/* $Id$ */
static char const _copyright[] =
"Copyright (c) 2009-2012 Pierre Pronchery <khorben@defora.org>";
/* This file is part of DeforaOS Desktop Panel */
static char const _license[] =
"This program is free software: you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation, version 3 of the License.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program.  If not, see <http://www.gnu.org/licenses/>.";
/* TODO:
 * - implement all remaining helpers */



#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>


/* types */
struct _Panel
{
	Config * config;
	GtkWidget * window;
	gint timeout;
};


/* constants */
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef LIBDIR
# define LIBDIR		PREFIX "/lib"
#endif


static char const * _authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};


/* private */
/* prototypes */
static int _applet_list(void);
static char * _config_get_filename(void);

static int _error(char const * message, int ret);


/* helper */
/* essential */
static void _helper_init(PanelAppletHelper * helper, Panel * panel,
		PanelAppletType type, GtkIconSize iconsize);

/* useful */
static char const * _helper_config_get(Panel * panel, char const * section,
		char const * variable);
static int _helper_error(Panel * panel, char const * message, int ret);
static void _helper_about_dialog(Panel * panel);
static void _helper_position_menu(Panel * panel, GtkMenu * menu, gint * x,
		gint * y, gboolean * push_in);
static void _helper_shutdown_dialog(Panel * panel);


/* functions */
/* applet_list */
static int _applet_list(void)
{
	char const path[] = LIBDIR "/Panel/applets";
	DIR * dir;
	struct dirent * de;
	size_t len;
	char const * sep = "";
#ifdef __APPLE__
	char const ext[] = ".dylib";
#else
	char const ext[] = ".so";
#endif

	puts("Applets available:");
	if((dir = opendir(path)) == NULL)
		return _error(path, 1);
	while((de = readdir(dir)) != NULL)
	{
		len = strlen(de->d_name);
		if(len < sizeof(ext) || strcmp(&de->d_name[
					len - sizeof(ext) + 1], ext) != 0)
			continue;
		de->d_name[len - 3] = '\0';
		printf("%s%s", sep, de->d_name);
		sep = ", ";
	}
	putchar('\n');
	closedir(dir);
	return 0;
}


/* config_get_filename */
static char * _config_get_filename(void)
{
	char const * homedir;
	size_t len;
	char * filename;

	if((homedir = getenv("HOME")) == NULL)
		homedir = g_get_home_dir();
	len = strlen(homedir) + 1 + sizeof(PANEL_CONFIG_FILE);
	if((filename = malloc(len)) == NULL)
		return NULL;
	snprintf(filename, len, "%s/%s", homedir, PANEL_CONFIG_FILE);
	return filename;
}


/* error */
static int _error(char const * message, int ret)
{
	return _helper_error(NULL, message, ret);
}


/* helpers */
/* essential */
/* helper_init */
static void _helper_init(PanelAppletHelper * helper, Panel * panel,
		PanelAppletType type, GtkIconSize iconsize)
{
	memset(helper, 0, sizeof(*helper));
	helper->panel = panel;
	helper->type = type;
	helper->icon_size = iconsize;
	helper->config_get = _helper_config_get;
	helper->error = _helper_error;
	helper->about_dialog = _helper_about_dialog;
	helper->position_menu = _helper_position_menu;
	helper->shutdown_dialog = _helper_shutdown_dialog;
}


/* useful */
/* helper_config_get */
static char const * _helper_config_get(Panel * panel, char const * section,
		char const * variable)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\")\n", __func__, section,
			variable);
#endif
	return config_get(panel->config, section, variable);
}


/* helper_error */
static int _helper_error(Panel * panel, char const * message, int ret)
{
	fputs(PACKAGE ": ", stderr);
	perror(message);
	return ret;
}


/* helper_about_dialog */
static void _helper_about_dialog(Panel * panel)
{
	GtkWidget * dialog;

	dialog = desktop_about_dialog_new();
	desktop_about_dialog_set_authors(dialog, _authors);
	desktop_about_dialog_set_comments(dialog,
			"Test application for the DeforaOS desktop panel");
	desktop_about_dialog_set_copyright(dialog, _copyright);
	desktop_about_dialog_set_logo_icon_name(dialog,
			"panel-settings"); /* XXX */
	desktop_about_dialog_set_license(dialog, _license);
	desktop_about_dialog_set_name(dialog, PACKAGE);
	desktop_about_dialog_set_program_name(dialog, "Panel test");
	desktop_about_dialog_set_version(dialog, VERSION);
	desktop_about_dialog_set_website(dialog,
			"http://www.defora.org/");
	gtk_window_set_position(GTK_WINDOW(dialog),
			GTK_WIN_POS_CENTER_ALWAYS);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/* helper_position_menu */
static void _helper_position_menu(Panel * panel, GtkMenu * menu, gint * x,
		gint * y, gboolean * push_in)
{
	GtkRequisition req;
	gint sx = 0;
	gint sy = 0;

	gtk_widget_size_request(GTK_WIDGET(menu), &req);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() width=%d, height=%d\n", __func__,
			req.width, req.height);
#endif
	if(req.height <= 0)
		return;
	gtk_window_get_position(GTK_WINDOW(panel->window), x, y);
	gtk_window_get_size(GTK_WINDOW(panel->window), &sx, &sy);
	*y += sy;
	*push_in = TRUE;
}


/* helper_shutdown_dialog */
enum { RES_CANCEL, RES_REBOOT, RES_SHUTDOWN };

static void _helper_shutdown_dialog(Panel * panel)
{
	GtkWidget * dialog;
	GtkWidget * widget;
#ifdef EMBEDDED
	const char message[] = "This will shutdown your device,"
		" therefore closing any application currently opened"
		" and losing any unsaved data.\n"
		"Do you really want to proceed?";
#else
	const char message[] = "This will shutdown your computer,"
		" therefore closing any application currently opened"
		" and losing any unsaved data.\n"
		"Do you really want to proceed?";
#endif

	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			"Shutdown");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s", message);
	gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, RES_CANCEL,
			"Restart", RES_REBOOT, NULL);
	widget = gtk_button_new_with_label("Shutdown");
	gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_icon_name(
				"gnome-shutdown", GTK_ICON_SIZE_BUTTON));
	gtk_widget_show_all(widget);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), widget, RES_SHUTDOWN);
	gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_title(GTK_WINDOW(dialog), "Shutdown");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}
