/* $Id$ */
/* Copyright (c) 2013 Pierre Pronchery <khorben@defora.org> */
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



#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include "../src/applets/wpa_supplicant.c"

/* constants */
#ifndef PROGNAME
# define PROGNAME	"wifibrowser"
#endif


/* WifiBrowser */
/* private */
/* prototypes */
static int _error(Panel * panel, char const * message, int ret);
static int _usage(void);

/* callbacks */
static gboolean _wifibrowser_on_closex(gpointer data);


/* functions */
/* error */
static int _error(Panel * panel, char const * message, int ret)
{
	fputs(PROGNAME ": ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, "Usage: %s\n", PROGNAME);
	return 1;
}


/* callbacks */
static gboolean _wifibrowser_on_closex(gpointer data)
{
	WPA * wpa = data;

	gtk_widget_hide(wpa->widget);
	gtk_main_quit();
	return FALSE;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	PanelAppletHelper helper;
	WPA * wpa;
	GtkWidget * window;
	GtkWidget * view;
	GtkWidget * widget;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	memset(&helper, 0, sizeof(helper));
	helper.type = PANEL_APPLET_TYPE_NORMAL;
	helper.icon_size = GTK_ICON_SIZE_MENU;
	helper.error = _error;
	gtk_init(&argc, &argv);
	if(optind != argc)
		return _usage();
	if((wpa = _wpa_init(&helper, &widget)) == NULL)
		return 2;
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window), 200, 300);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(GTK_WINDOW(window), "network-wireless");
#endif
	gtk_window_set_title(GTK_WINDOW(window), "Wireless browser");
	g_signal_connect_swapped(window, "delete-event", G_CALLBACK(
				_wifibrowser_on_closex), wpa);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(wpa->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("SSID", renderer,
			"text", 3, NULL);
	gtk_tree_view_column_set_sort_column_id(column, 3);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("BSSID", renderer,
			"text", 0, NULL);
	gtk_tree_view_column_set_sort_column_id(column, 1);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	gtk_container_add(GTK_CONTAINER(widget), view);
	gtk_container_add(GTK_CONTAINER(window), widget);
	gtk_widget_show_all(window);
	gtk_main();
	_wpa_destroy(wpa);
	return 0;
}
