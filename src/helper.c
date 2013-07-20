/* $Id$ */
static char const _copyright[] =
"Copyright (c) 2004-2013 DeforaOS Project <contact@defora.org>";
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



#ifdef __NetBSD__
# include <sys/param.h>
# include <sys/sysctl.h>
#else
# include <fcntl.h>
#endif
#include <unistd.h>
#ifdef DEBUG
# include <stdio.h>
#endif
#include <gtk/gtk.h>
#include <System.h>
#include <Desktop.h>


/* helper */
/* private */
/* prototypes */
static char const * _panel_helper_config_get(Panel * panel,
		char const * section, char const * variable);
static int _panel_helper_config_set(Panel * panel, char const * section,
		char const * variable, char const * value);
static int _panel_helper_error(Panel * panel, char const * message, int ret);
static void _panel_helper_about_dialog(Panel * panel);
static int _panel_helper_lock(Panel * panel);
#ifndef EMBEDDED
static void _panel_helper_logout_dialog(Panel * panel);
#endif
#ifndef HELPER_POSITION_MENU_WIDGET
static void _panel_helper_position_menu(Panel * panel, GtkMenu * menu, gint * x,
		gint * y, gboolean * push_in, PanelPosition position);
static void _panel_helper_position_menu_bottom(Panel * panel, GtkMenu * menu,
		gint * x, gint * y, gboolean * push_in);
static void _panel_helper_position_menu_top(Panel * panel, GtkMenu * menu,
		gint * x, gint * y, gboolean * push_in);
#else
static void _panel_helper_position_menu_widget(Panel * panel, GtkMenu * menu,
		gint * x, gint * y, gboolean * push_in);
#endif
static void _panel_helper_preferences_dialog(Panel * panel);
static void _panel_helper_rotate_screen(Panel * panel);
static void _panel_helper_shutdown_dialog(Panel * panel);
static int _panel_helper_suspend(Panel * panel);


/* functions */
/* panel_helper_config_get */
static char const * _panel_helper_config_get(Panel * panel,
		char const * section, char const * variable)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\")\n", __func__, section,
			variable);
#endif
	return config_get(panel->config, section, variable);
}


/* panel_helper_config_set */
static int _panel_helper_config_set(Panel * panel, char const * section,
		char const * variable, char const * value)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", \"%s\")\n", __func__,
			section, variable, value);
#endif
	/* FIXME save the configuration (if not in test mode) */
	return config_set(panel->config, section, variable, value);
}


/* panel_helper_error */
static int _panel_helper_error(Panel * panel, char const * message, int ret)
{
	return panel_error(panel, message, ret);
}


/* panel_helper_about_dialog */
static gboolean _about_on_closex(gpointer data);

static void _panel_helper_about_dialog(Panel * panel)
{
	if(panel->ab_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(panel->ab_window));
		return;
	}
	panel->ab_window = desktop_about_dialog_new();
	desktop_about_dialog_set_authors(panel->ab_window, _authors);
	desktop_about_dialog_set_comments(panel->ab_window,
			_("Panel for the DeforaOS desktop"));
	desktop_about_dialog_set_copyright(panel->ab_window, _copyright);
	desktop_about_dialog_set_logo_icon_name(panel->ab_window,
			"panel-settings"); /* XXX */
	desktop_about_dialog_set_license(panel->ab_window, _license);
	desktop_about_dialog_set_name(panel->ab_window, PACKAGE);
	desktop_about_dialog_set_program_name(panel->ab_window, PACKAGE);
	desktop_about_dialog_set_translator_credits(panel->ab_window,
			_("translator-credits"));
	desktop_about_dialog_set_version(panel->ab_window, VERSION);
	desktop_about_dialog_set_website(panel->ab_window,
			"http://www.defora.org/");
	gtk_window_set_position(GTK_WINDOW(panel->ab_window),
			GTK_WIN_POS_CENTER_ALWAYS);
	g_signal_connect_swapped(panel->ab_window, "delete-event", G_CALLBACK(
				_about_on_closex), panel);
	gtk_widget_show(panel->ab_window);
}

static gboolean _about_on_closex(gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(panel->ab_window);
	return TRUE;
}


/* panel_helper_lock */
static gboolean _lock_on_idle(gpointer data);

static int _panel_helper_lock(Panel * panel)
{
	panel->source = g_idle_add(_lock_on_idle, panel);
	return 0;
}

static gboolean _lock_on_idle(gpointer data)
{
	/* FIXME default to calling XActivateScreenSaver() */
	Panel * panel = data;
	char const * command = "xset s activate";
	char const * p;
	GError * error = NULL;

	panel->source = 0;
	if((p = config_get(panel->config, "lock", "command")) != NULL)
		command = p;
	if(g_spawn_command_line_async(command, &error) != TRUE)
	{
		_panel_helper_error(panel, error->message, 1);
		g_error_free(error);
	}
	return FALSE;
}


#ifndef EMBEDDED
/* panel_helper_logout_dialog */
static gboolean _logout_dialog_on_closex(gpointer data);
static void _logout_dialog_on_response(GtkWidget * widget, gint response);

static void _panel_helper_logout_dialog(Panel * panel)
{
	const char * message = _("This will log you out of the current session,"
			" therefore closing any application currently opened"
			" and losing any unsaved data.\n"
			"Do you really want to proceed?");
	GtkWidget * widget;

	if(panel->lo_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(panel->lo_window));
		return;
	}
	panel->lo_window = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Logout"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
				panel->lo_window),
#endif
			"%s", message);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(panel->lo_window),
			gtk_image_new_from_icon_name("gnome-logout",
				GTK_ICON_SIZE_DIALOG));
#endif
	gtk_dialog_add_buttons(GTK_DIALOG(panel->lo_window), GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, NULL);
	widget = gtk_button_new_with_label(_("Logout"));
	gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_icon_name(
				"gnome-logout", GTK_ICON_SIZE_BUTTON));
	gtk_widget_show_all(widget);
	gtk_dialog_add_action_widget(GTK_DIALOG(panel->lo_window), widget,
			GTK_RESPONSE_ACCEPT);
	gtk_window_set_keep_above(GTK_WINDOW(panel->lo_window), TRUE);
	gtk_window_set_position(GTK_WINDOW(panel->lo_window),
			GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_title(GTK_WINDOW(panel->lo_window), _("Logout"));
	g_signal_connect(panel->lo_window, "delete-event", G_CALLBACK(
				_logout_dialog_on_closex), panel);
	g_signal_connect(panel->lo_window, "response", G_CALLBACK(
				_logout_dialog_on_response), panel);
	gtk_widget_show_all(panel->lo_window);
}

static gboolean _logout_dialog_on_closex(gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(panel->lo_window);
	return TRUE;
}

static void _logout_dialog_on_response(GtkWidget * widget, gint response)
{
	gtk_widget_hide(widget);
	if(response == GTK_RESPONSE_ACCEPT)
	{
		gtk_main_quit();
#ifndef DEBUG
		/* XXX assumes the parent process is the session manager */
		kill(getppid(), SIGHUP);
#endif
	}
}
#endif


#ifndef HELPER_POSITION_MENU_WIDGET
/* panel_helper_position_menu */
static void _panel_helper_position_menu(Panel * panel, GtkMenu * menu, gint * x,
		gint * y, gboolean * push_in, PanelPosition position)
{
	GtkRequisition req;

	gtk_widget_size_request(GTK_WIDGET(menu), &req);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() width=%d, height=%d\n", __func__,
			req.width, req.height);
#endif
	if(req.height <= 0)
		return;
	*x = (req.width < panel->root_width - PANEL_BORDER_WIDTH)
		? PANEL_BORDER_WIDTH : 0;
	if(position == PANEL_POSITION_TOP)
		*y = panel_window_get_height(panel->top);
	else if(position == PANEL_POSITION_BOTTOM)
		*y = panel->root_height
			- panel_window_get_height(panel->bottom) - req.height;
	else /* XXX generic */
		*y = panel->root_height - req.height;
	*push_in = TRUE;
}


/* panel_helper_position_menu_bottom */
static void _panel_helper_position_menu_bottom(Panel * panel, GtkMenu * menu,
		gint * x, gint * y, gboolean * push_in)
{
	_panel_helper_position_menu(panel, menu, x, y, push_in,
			PANEL_POSITION_BOTTOM);
}


/* panel_helper_position_menu_top */
static void _panel_helper_position_menu_top(Panel * panel, GtkMenu * menu,
		gint * x, gint * y, gboolean * push_in)
{
	_panel_helper_position_menu(panel, menu, x, y, push_in,
			PANEL_POSITION_TOP);
}
#else
/* panel_helper_position_menu_widget */
static void _panel_helper_position_menu_widget(Panel * panel, GtkMenu * menu,
		gint * x, gint * y, gboolean * push_in)
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
#endif


/* panel_helper_preferences_dialog */
static void _panel_helper_preferences_dialog(Panel * panel)
{
	panel_show_preferences(panel, TRUE);
}


/* panel_helper_rotate_screen */
static void _panel_helper_rotate_screen(Panel * panel)
{
	desktop_message_send(DESKTOP_CLIENT_MESSAGE, DESKTOP_MESSAGE_SET_LAYOUT,
			DESKTOP_LAYOUT_TOGGLE, 0);
}


/* panel_helper_shutdown_dialog */
static gboolean _shutdown_dialog_on_closex(gpointer data);
static void _shutdown_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data);
enum { RES_CANCEL, RES_REBOOT, RES_SHUTDOWN };

static void _panel_helper_shutdown_dialog(Panel * panel)
{
#ifdef EMBEDDED
	const char * message = _("This will shutdown your device,"
			" therefore closing any application currently opened"
			" and losing any unsaved data.\n"
			"Do you really want to proceed?");
#else
	const char * message = _("This will shutdown your computer,"
			" therefore closing any application currently opened"
			" and losing any unsaved data.\n"
			"Do you really want to proceed?");
#endif
	GtkWidget * widget;

	if(panel->sh_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(panel->sh_window));
		return;
	}
	panel->sh_window = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Shutdown"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
				panel->sh_window),
#endif
			"%s", message);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(panel->sh_window),
			gtk_image_new_from_icon_name("gnome-shutdown",
				GTK_ICON_SIZE_DIALOG));
#endif
	gtk_dialog_add_buttons(GTK_DIALOG(panel->sh_window), GTK_STOCK_CANCEL,
			RES_CANCEL, _("Restart"), RES_REBOOT, NULL);
	widget = gtk_button_new_with_label(_("Shutdown"));
	gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_icon_name(
				"gnome-shutdown", GTK_ICON_SIZE_BUTTON));
	gtk_widget_show_all(widget);
	gtk_dialog_add_action_widget(GTK_DIALOG(panel->sh_window), widget,
			RES_SHUTDOWN);
	gtk_window_set_keep_above(GTK_WINDOW(panel->sh_window), TRUE);
	gtk_window_set_position(GTK_WINDOW(panel->sh_window),
			GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_title(GTK_WINDOW(panel->sh_window), _("Shutdown"));
	g_signal_connect(panel->sh_window, "delete-event", G_CALLBACK(
				_shutdown_dialog_on_closex), panel);
	g_signal_connect(panel->sh_window, "response", G_CALLBACK(
				_shutdown_dialog_on_response), panel);
	gtk_widget_show_all(panel->sh_window);
}

static gboolean _shutdown_dialog_on_closex(gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(panel->sh_window);
	return TRUE;
}

static void _shutdown_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data)
{
	Panel * panel = data;
	char * reboot[] = { "/sbin/shutdown", "shutdown", "-r", "now", NULL };
	char * shutdown[] = { "/sbin/shutdown", "shutdown",
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
		"-p",
#else
		"-h",
#endif
		"now", NULL };
	char ** argv;
	GError * error = NULL;

	gtk_widget_hide(widget);
	if(response == RES_SHUTDOWN)
		argv = shutdown;
	else if(response == RES_REBOOT)
		argv = reboot;
	else
		return;
	if(g_spawn_async(NULL, argv, NULL, G_SPAWN_FILE_AND_ARGV_ZERO, NULL,
				NULL, NULL, &error) != TRUE)
	{
		_panel_helper_error(panel, error->message, 1);
		g_error_free(error);
	}
}


/* panel_helper_suspend */
static int _panel_helper_suspend(Panel * panel)
{
#ifdef __NetBSD__
	int sleep_state = 3;
#else
	int fd;
	char * suspend[] = { "/usr/bin/sudo", "sudo", "/usr/bin/apm", "-s",
		NULL };
	GSpawnFlags flags = G_SPAWN_FILE_AND_ARGV_ZERO;
	GError * error = NULL;
#endif

#ifdef __NetBSD__
	if(sysctlbyname("machdep.sleep_state", NULL, NULL, &sleep_state,
				sizeof(sleep_state)) != 0)
		return -_panel_helper_error(panel, "sysctl", 1);
#else
	if((fd = open("/sys/power/state", O_WRONLY)) >= 0)
	{
		write(fd, "mem\n", 4);
		close(fd);
	}
	else if(g_spawn_async(NULL, suspend, NULL, flags, NULL, NULL, NULL,
				&error) != TRUE)
	{
		_panel_helper_error(panel, error->message, 1);
		g_error_free(error);
		return -1;
	}
#endif
	_panel_helper_lock(panel); /* XXX may already be suspended */
	return 0;
}