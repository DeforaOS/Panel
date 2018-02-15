/* $Id$ */
static char const _copyright[] =
"Copyright © 2004-2018 DeforaOS Project <contact@defora.org>";
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
#include <Desktop/Browser.h>


/* helper */
/* private */
/* prototypes */
static char const * _panel_helper_config_get(Panel * panel,
		char const * section, char const * variable);
static int _panel_helper_config_set(Panel * panel, char const * section,
		char const * variable, char const * value);
static int _panel_helper_error(Panel * panel, char const * message, int ret);
static void _panel_helper_about_dialog(Panel * panel);
static void _panel_helper_lock(Panel * panel);
static void _panel_helper_lock_dialog(Panel * panel);
static void _panel_helper_logout(Panel * panel);
static void _panel_helper_logout_dialog(Panel * panel);
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
static void _panel_helper_shutdown(Panel * panel, gboolean reboot);
static void _panel_helper_shutdown_dialog(Panel * panel);
static void _panel_helper_suspend(Panel * panel);
static void _panel_helper_suspend_dialog(Panel * panel);


/* functions */
/* panel_helper_config_get */
static char const * _panel_helper_config_get(Panel * panel,
		char const * section, char const * variable)
{
	char const * ret;
	String * s = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\")\n", __func__, section,
			variable);
#endif
	if(section != NULL)
	{
		if((s = string_new_append("applet::", section, NULL)) == NULL)
			return NULL;
		section = s;
	}
	ret = panel_get_config(panel, section, variable);
	string_delete(s);
	return ret;
}


/* panel_helper_config_set */
static int _panel_helper_config_set(Panel * panel, char const * section,
		char const * variable, char const * value)
{
	int ret;
	String * s = NULL;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", \"%s\")\n", __func__,
			section, variable, value);
#endif
	if(section != NULL)
	{
		if((s = string_new_append("applet::", section, NULL)) == NULL)
			return -1;
		section = s;
	}
	/* FIXME save the configuration (if not in test mode) */
	ret = config_set(panel->config, section, variable, value);
	string_delete(s);
	return ret;
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
	char const * p;
	char const ** q;
	char const * authors[] = { NULL, NULL };

	if(panel->ab_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(panel->ab_window));
		return;
	}
	panel->ab_window = desktop_about_dialog_new();
	if((authors[0] = panel_get_config(panel, "about", "authors")) != NULL)
		q = authors;
	else
		q = _authors;
	desktop_about_dialog_set_authors(panel->ab_window, q);
	if((p = panel_get_config(panel, "about", "comment")) == NULL)
		p = _("Panel for the DeforaOS desktop");
	desktop_about_dialog_set_comments(panel->ab_window, p);
	if((p = panel_get_config(panel, "about", "copyright")) == NULL)
		p = _copyright;
	desktop_about_dialog_set_copyright(panel->ab_window, p);
	if((p = panel_get_config(panel, "about", "icon")) == NULL)
		p = "panel-settings"; /* XXX */
	desktop_about_dialog_set_logo_icon_name(panel->ab_window, p);
	if((p = panel_get_config(panel, "about", "license")) == NULL)
		p = _license;
	desktop_about_dialog_set_license(panel->ab_window, p);
	if((p = panel_get_config(panel, "about", "name")) == NULL)
		p = PACKAGE;
	desktop_about_dialog_set_program_name(panel->ab_window, p);
	if((p = panel_get_config(panel, "about", "translator")) == NULL)
		p = _("translator-credits");
	desktop_about_dialog_set_translator_credits(panel->ab_window, p);
	if((p = panel_get_config(panel, "about", "version")) == NULL)
		p = VERSION;
	desktop_about_dialog_set_version(panel->ab_window, p);
	if((p = panel_get_config(panel, "about", "website")) == NULL)
		p = "https://www.defora.org/";
	desktop_about_dialog_set_website(panel->ab_window, p);
	gtk_window_set_position(GTK_WINDOW(panel->ab_window),
			GTK_WIN_POS_CENTER);
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

static void _panel_helper_lock(Panel * panel)
{
	panel->source = g_idle_add(_lock_on_idle, panel);
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


/* panel_helper_lock_dialog */
static gboolean _lock_dialog_on_closex(gpointer data);
static void _lock_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data);

static void _panel_helper_lock_dialog(Panel * panel)
{
#ifdef EMBEDDED
	const char message[] = N_("This will lock your device.\n"
			"Do you really want to proceed?");
#else
	const char message[] = N_("This will lock your session.\n"
			"Do you really want to proceed?");
#endif
	GtkWidget * widget;

	if(panel->lk_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(panel->lk_window));
		return;
	}
	panel->lk_window = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Shutdown"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
				panel->lk_window),
#endif
			"%s", _(message));
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(panel->lk_window),
			gtk_image_new_from_icon_name("gnome-lockscreen",
				GTK_ICON_SIZE_DIALOG));
#endif
	gtk_dialog_add_buttons(GTK_DIALOG(panel->lk_window), GTK_STOCK_CANCEL,
			FALSE, NULL);
	widget = gtk_button_new_with_label(_("Lock"));
	gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_icon_name(
				"gnome-lockscreen", GTK_ICON_SIZE_BUTTON));
	gtk_widget_show_all(widget);
	gtk_dialog_add_action_widget(GTK_DIALOG(panel->lk_window), widget,
			TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(panel->lk_window), TRUE);
	gtk_window_set_position(GTK_WINDOW(panel->lk_window),
			GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(panel->lk_window), _("Lock"));
	g_signal_connect(panel->lk_window, "delete-event", G_CALLBACK(
				_lock_dialog_on_closex), panel);
	g_signal_connect(panel->lk_window, "response", G_CALLBACK(
				_lock_dialog_on_response), panel);
	gtk_widget_show_all(panel->lk_window);
}

static gboolean _lock_dialog_on_closex(gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(panel->lk_window);
	return TRUE;
}

static void _lock_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(widget);
	if(response == TRUE)
		_panel_helper_lock(panel);
}


/* panel_helper_logout */
static void _panel_helper_logout(Panel * panel)
{
	gtk_main_quit();
#ifndef DEBUG
	/* XXX assumes the parent process is the session manager */
	kill(getppid(), SIGHUP);
#endif
}


/* panel_helper_logout_dialog */
static gboolean _logout_dialog_on_closex(gpointer data);
static void _logout_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data);

static void _panel_helper_logout_dialog(Panel * panel)
{
	const char message[] = N_("This will log you out of the current session"
			", therefore closing any application currently opened"
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
			"%s", _(message));
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
			GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(panel->lo_window), _("Logout"));
	g_signal_connect_swapped(panel->lo_window, "delete-event", G_CALLBACK(
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

static void _logout_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(widget);
	if(response == GTK_RESPONSE_ACCEPT)
		_panel_helper_logout(panel);
}


#ifndef HELPER_POSITION_MENU_WIDGET
/* panel_helper_position_menu */
static void _panel_helper_position_menu(Panel * panel, GtkMenu * menu, gint * x,
		gint * y, gboolean * push_in, PanelPosition position)
{
	GtkRequisition req;
	PanelWindow * window = panel->windows[position];

	if(window == NULL)
		return;
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_get_preferred_size(GTK_WIDGET(menu), NULL, &req);
#else
	gtk_widget_size_request(GTK_WIDGET(menu), &req);
#endif
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() width=%d, height=%d\n", __func__,
			req.width, req.height);
#endif
	if(req.height <= 0)
		return;
	switch(position)
	{
		case PANEL_POSITION_TOP:
			*y = panel_window_get_height(window);
			break;
		case PANEL_POSITION_BOTTOM:
			*y = panel->root_height
				- panel_window_get_height(window) - req.height;
			break;
		case PANEL_POSITION_LEFT:
			*x = panel_window_get_width(window);
			break;
		case PANEL_POSITION_RIGHT:
			*x = panel->root_width
				- panel_window_get_width(window) - req.width;
			break;
	}
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

#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_get_preferred_size(GTK_WIDGET(menu), NULL, &req);
#else
	gtk_widget_size_request(GTK_WIDGET(menu), &req);
#endif
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() width=%d, height=%d\n", __func__,
			req.width, req.height);
#endif
	if(req.height <= 0)
		return;
	panel_window_get_position(panel->windows[PANEL_POSITION_TOP], x, y);
	panel_window_get_size(panel->windows[PANEL_POSITION_TOP], &sx, &sy);
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
	(void) panel;

	desktop_message_send(DESKTOP_CLIENT_MESSAGE, DESKTOP_MESSAGE_SET_LAYOUT,
			DESKTOP_LAYOUT_TOGGLE, 0);
}


/* panel_helper_shutdown */
static void _panel_helper_shutdown(Panel * panel, gboolean reboot)
{
	char * reset[] = { "/sbin/shutdown", "shutdown", "-r", "now", NULL };
	char * halt[] = { "/sbin/shutdown", "shutdown",
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
		"-p",
#else
		"-h",
#endif
		"now", NULL };
	GError * error = NULL;

	if(g_spawn_async(NULL, reboot ? reset : halt, NULL,
				G_SPAWN_FILE_AND_ARGV_ZERO, NULL, NULL, NULL,
				&error) != TRUE)
	{
		_panel_helper_error(panel, error->message, 1);
		g_error_free(error);
	}
}


/* panel_helper_shutdown_dialog */
static gboolean _shutdown_dialog_on_closex(gpointer data);
static void _shutdown_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data);
enum { RES_CANCEL, RES_REBOOT, RES_SHUTDOWN };

static void _panel_helper_shutdown_dialog(Panel * panel)
{
#ifdef EMBEDDED
	const char message[] = N_("This will shutdown your device,"
			" therefore closing any application currently opened"
			" and losing any unsaved data.\n"
			"Do you really want to proceed?");
#else
	const char message[] = N_("This will shutdown your computer,"
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
			"%s", _(message));
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
			GTK_WIN_POS_CENTER);
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

	gtk_widget_hide(widget);
	if(response == RES_SHUTDOWN)
		_panel_helper_shutdown(panel, FALSE);
	else if(response == RES_REBOOT)
		_panel_helper_shutdown(panel, TRUE);
}


/* panel_helper_suspend */
static void _panel_helper_suspend(Panel * panel)
{
#ifdef __NetBSD__
	int sleep_state = 3;
#else
	int fd;
	char * suspend[] = { "/usr/bin/sudo", "sudo", "/usr/bin/apm", "-s",
		NULL };
	const unsigned int flags = G_SPAWN_FILE_AND_ARGV_ZERO;
	GError * error = NULL;
#endif

#ifdef __NetBSD__
	if(sysctlbyname("machdep.sleep_state", NULL, NULL, &sleep_state,
				sizeof(sleep_state)) != 0)
	{
		_panel_helper_error(panel, "sysctl", 1);
		return;
	}
#else
	if((fd = open("/sys/power/state", O_WRONLY)) >= 0)
	{
		write(fd, "mem\n", 4);
		close(fd);
	}
	else if(g_spawn_async(NULL, geteuid() == 0 ? &suspend[2] : suspend,
				NULL, geteuid() == 0 ? 0 : flags, NULL, NULL,
				NULL, &error) != TRUE)
	{
		_panel_helper_error(panel, error->message, 1);
		g_error_free(error);
		return;
	}
#endif
	_panel_helper_lock(panel); /* XXX may already be suspended */
}


/* panel_helper_suspend_dialog */
static gboolean _suspend_dialog_on_closex(gpointer data);
static void _suspend_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data);

static void _panel_helper_suspend_dialog(Panel * panel)
{
#ifdef EMBEDDED
	const char message[] = N_("This will suspend your device.\n"
			"Do you really want to proceed?");
#else
	const char message[] = N_("This will suspend your computer.\n"
			"Do you really want to proceed?");
#endif
	GtkWidget * widget;

	if(panel->su_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(panel->su_window));
		return;
	}
	panel->su_window = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Shutdown"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
				panel->su_window),
#endif
			"%s", _(message));
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(panel->su_window),
			gtk_image_new_from_icon_name("gnome-shutdown",
				GTK_ICON_SIZE_DIALOG));
#endif
	gtk_dialog_add_buttons(GTK_DIALOG(panel->su_window), GTK_STOCK_CANCEL,
			FALSE, NULL);
	widget = gtk_button_new_with_label(_("Suspend"));
	gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_icon_name(
				"gtk-media-pause", GTK_ICON_SIZE_BUTTON));
	gtk_widget_show_all(widget);
	gtk_dialog_add_action_widget(GTK_DIALOG(panel->su_window), widget,
			TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(panel->su_window), TRUE);
	gtk_window_set_position(GTK_WINDOW(panel->su_window),
			GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(panel->su_window), _("Suspend"));
	g_signal_connect(panel->su_window, "delete-event", G_CALLBACK(
				_suspend_dialog_on_closex), panel);
	g_signal_connect(panel->su_window, "response", G_CALLBACK(
				_suspend_dialog_on_response), panel);
	gtk_widget_show_all(panel->su_window);
}

static gboolean _suspend_dialog_on_closex(gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(panel->su_window);
	return TRUE;
}

static void _suspend_dialog_on_response(GtkWidget * widget, gint response,
		gpointer data)
{
	Panel * panel = data;

	gtk_widget_hide(widget);
	if(response == TRUE)
		_panel_helper_suspend(panel);
}
