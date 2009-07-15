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



#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "panel.h"


/* Panel */
/* private */
/* types */
struct _Panel
{
	GdkWindow * root;
	GtkWidget * window;
	GtkWidget * clock;

	gint width;			/* width of the root window	*/
	gint height;			/* height of the root window	*/
};


/* constants */
#define PANEL_BORDER_WIDTH		4
#define PANEL_ICON_SIZE			48


/* prototypes */
static int _panel_exec(Panel * panel, char * command);


/* public */
/* panel_new */
static GtkWidget * _new_button(char const * stock);
static gboolean _on_button_press(GtkWidget * widget, GdkEventButton * event,
		gpointer data);
static void _on_lock(GtkWidget * widget, gpointer data);
static void _on_menu(GtkWidget * widget, gpointer data);
static void _on_menu_position(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data);
static void _on_run(GtkWidget * widget, gpointer data);
static gboolean _on_timeout_clock(gpointer data);

Panel * panel_new(void)
{
	Panel * panel;
	GtkWidget * event;
	GtkWidget * hbox;
	GtkWidget * widget;
	gint x;
	gint y;
	gint depth;

	if((panel = malloc(sizeof(*panel))) == NULL)
	{
		/* FIXME visually warn the user */
		panel_error(NULL, "malloc", 1);
		return NULL;
	}
	/* root window */
	panel->root = gdk_screen_get_root_window(
			gdk_display_get_default_screen(
				gdk_display_get_default()));
	gdk_window_get_geometry(panel->root, &x, &y, &panel->width,
			&panel->height, &depth);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() x=%d y=%d width=%d height=%d depth=%d\n",
			__func__, x, y, panel->width, panel->height, depth);
#endif
	/* panel */
	panel->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_move(GTK_WINDOW(panel->window), 0, panel->height - 56);
	gtk_window_set_default_size(GTK_WINDOW(panel->window), panel->width,
			56);
	gtk_window_set_type_hint(GTK_WINDOW(panel->window),
			GDK_WINDOW_TYPE_HINT_DOCK);
	event = gtk_event_box_new();
	g_signal_connect(G_OBJECT(event), "button-press-event", G_CALLBACK(
				_on_button_press), panel);
	hbox = gtk_hbox_new(FALSE, 4);
	/* main menu */
	widget = _new_button("gnome-main-menu");
	g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(_on_menu),
			panel);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	/* quick launch */
	widget = _new_button("gnome-lockscreen");
	g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(_on_lock),
			panel);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), _new_button("gnome-logout"), FALSE,
			TRUE, 0);
	/* clock */
	panel->clock = gtk_label_new(" \n ");
	gtk_label_set_justify(GTK_LABEL(panel->clock), GTK_JUSTIFY_CENTER);
	g_timeout_add(1000, _on_timeout_clock, panel);
	_on_timeout_clock(panel);
	gtk_box_pack_end(GTK_BOX(hbox), panel->clock, FALSE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(event), hbox);
	gtk_container_add(GTK_CONTAINER(panel->window), event);
	gtk_container_set_border_width(GTK_CONTAINER(panel->window), 4);
	gtk_widget_show_all(panel->window);
	return panel;
}

static GtkWidget * _new_button(char const * stock)
{
	GtkWidget * button;
	GtkWidget * image;

	button = gtk_button_new();
	image = gtk_image_new_from_icon_name(stock,
			GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	return button;
}

static gboolean _on_button_press(GtkWidget * widget, GdkEventButton * event,
		gpointer data)
{
	GtkWidget * menu;
	GtkWidget * menuitem;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() right-click\n", __func__);
#endif
	menu = gtk_menu_new();
	menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PROPERTIES,
			NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, data, event->button,
			event->time);
	return FALSE;
}

static void _on_lock(GtkWidget * widget, gpointer data)
{
	Panel * panel = data;

	_panel_exec(panel, "xscreensaver-command -lock");
}

static void _on_menu(GtkWidget * widget, gpointer data)
{
	GtkWidget * menu;
	GtkWidget * menuitem;
	GtkWidget * image;

	menu = gtk_menu_new();
	menuitem = gtk_image_menu_item_new_with_label("Applications");
	image = gtk_image_new_from_icon_name("gnome-applications",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_label("Run...");
	image = gtk_image_new_from_stock(GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
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
	menuitem = gtk_image_menu_item_new_with_label("Lock screen");
	image = gtk_image_new_from_icon_name("gnome-lockscreen",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(_on_lock),
			data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_image_menu_item_new_with_label("Logout...");
	image = gtk_image_new_from_icon_name("gnome-logout",
			GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, _on_menu_position, data, 0,
			gtk_get_current_event_time());
}

static void _on_menu_position(GtkMenu * menu, gint * x, gint * y,
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
	*y = panel->height - PANEL_BORDER_WIDTH - PANEL_ICON_SIZE - req.height;
	*push_in = TRUE;
}

static void _on_run(GtkWidget * widget, gpointer data)
{
	Panel * panel = data;

	_panel_exec(panel, "run");
}

static gboolean _on_timeout_clock(gpointer data)
{
	Panel * panel = data;
	struct timeval tv;
	time_t t;
	struct tm tm;
	char buf[32];

	if(gettimeofday(&tv, NULL) != 0)
		return panel_error(panel, "gettimeofday", TRUE);
	t = tv.tv_sec;
	localtime_r(&t, &tm);
	strftime(buf, sizeof(buf), "%H:%M:%S\n%d/%m/%Y", &tm);
	gtk_label_set_text(GTK_LABEL(panel->clock), buf);
	return TRUE;
}


/* panel_delete */
void panel_delete(Panel * panel)
{
	free(panel);
}


/* useful */
static int _error_text(char const * message, int ret);

int panel_error(Panel * panel, char const * message, int ret)
{
	GtkWidget * dialog;

	if(panel == NULL)
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "%s: %s", message, strerror(errno));
	gtk_window_set_title(GTK_WINDOW(dialog), "Error");
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
				gtk_widget_destroy), NULL);
	gtk_widget_show(dialog);
	return ret;
}

static int _error_text(char const * message, int ret)
{
	fputs("panel: ", stderr);
	perror(message);
	return ret;
}


/* private */
/* functions */
/* panel_exec */
static int _panel_exec(Panel * panel, char * command)
{
	pid_t pid;

	if((pid = fork()) == -1)
		return panel_error(panel, "fork", 1);
	else if(pid != 0) /* the parent returns */
		return 0;
	execlp("/bin/sh", "sh", "-c", command, NULL);
	exit(panel_error(NULL, command, 2));
	return 1;
}
