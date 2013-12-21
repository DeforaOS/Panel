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
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include "../src/applets/wpa_supplicant.c"
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME
# define PROGNAME	"wifibrowser"
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


/* WifiBrowser */
/* private */
/* types */
typedef enum _WifiBrowserResponse
{
	WBR_REASSOCIATE = 0,
	WBR_RESCAN,
	WBR_SAVE_CONFIGURATION
} WifiBrowserResponse;

struct _Panel
{
	Config * config;
};


/* prototypes */
static int _wifibrowser(char const * configfile, char const * interface);

static int _error(Panel * panel, char const * message, int ret);
static int _usage(void);

/* helpers */
static char const * _helper_config_get(Panel * panel, char const * section,
		char const * variable);

/* callbacks */
static gboolean _wifibrowser_on_closex(gpointer data);
static void _wifibrowser_on_response(GtkWidget * widget, gint arg1,
		gpointer data);


/* functions */
/* wifibrowser */
static int _wifibrowser(char const * configfile, char const * interface)
{
	Panel panel;
	PanelAppletHelper helper;
	WPA * wpa;
	GtkWidget * window;
	GtkWidget * vbox;
	GtkWidget * view;
	GtkWidget * widget;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	/* XXX report errors to the user instead */
	if((panel.config = config_new()) != NULL)
	{
		if(configfile != NULL
				&& config_load(panel.config, configfile) != 0)
			error_print(PROGNAME);
		if(interface != NULL
				&& config_set(panel.config, "wpa_supplicant",
					"interface", interface) != 0)
			error_print(PROGNAME);
	}
	else
		error_print(PROGNAME);
	/* FIXME load the configuration */
	memset(&helper, 0, sizeof(helper));
	helper.panel = &panel;
	helper.type = PANEL_APPLET_TYPE_NORMAL;
	helper.icon_size = GTK_ICON_SIZE_MENU;
	helper.error = _error;
	helper.config_get = _helper_config_get;
	if((wpa = _wpa_init(&helper, &widget)) == NULL)
		return 2;
	window = gtk_dialog_new_with_buttons(_("Wireless browser"), NULL, 0,
			_("Reassociate"), WBR_REASSOCIATE,
			_("Rescan"), WBR_RESCAN,
			GTK_STOCK_SAVE, WBR_SAVE_CONFIGURATION,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(GTK_WINDOW(window), "network-wireless");
#endif
	g_signal_connect_swapped(window, "delete-event", G_CALLBACK(
				_wifibrowser_on_closex), wpa);
	g_signal_connect(window, "response", G_CALLBACK(
				_wifibrowser_on_response), wpa);
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(window));
#else
	vbox = GTK_DIALOG(window)->vbox;
#endif
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(wpa->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
	/* signal level */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer,
			"pixbuf", WSR_ICON, NULL);
	gtk_tree_view_column_set_sort_column_id(column, WSR_LEVEL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	/* SSID */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("SSID"), renderer,
			"text", WSR_SSID, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, WSR_SSID);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	/* BSSID */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("BSSID"), renderer,
			"text", WSR_BSSID, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, WSR_BSSID);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	gtk_container_add(GTK_CONTAINER(widget), view);
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
	gtk_widget_show_all(window);
	gtk_main();
	_wpa_destroy(wpa);
	if(panel.config != NULL)
		config_delete(panel.config);
	return 0;
}


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
	fprintf(stderr, _("Usage: %s [-c filename][-i interface]\n"
"  -c	Path to a configuration file\n"
"  -i	Network interface to connect to\n"), PROGNAME);
	return 1;
}


/* helpers */
/* helper_config_get */
static char const * _helper_config_get(Panel * panel, char const * section,
		char const * variable)
{
	if(panel->config == NULL)
		return NULL;
	return config_get(panel->config, section, variable);
}


/* callbacks */
/* wifibrowser_on_closex */
static gboolean _wifibrowser_on_closex(gpointer data)
{
	WPA * wpa = data;

	gtk_widget_hide(wpa->widget);
	gtk_main_quit();
	return FALSE;
}


/* wifibrowser_on_response */
static void _wifibrowser_on_response(GtkWidget * widget, gint arg1,
		gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel = &wpa->channel[0];

	switch(arg1)
	{
		case GTK_RESPONSE_CLOSE:
			gtk_widget_hide(widget);
			gtk_main_quit();
			break;
		case WBR_REASSOCIATE:
			_wpa_queue(wpa, channel, WC_REASSOCIATE);
			break;
		case WBR_RESCAN:
			_wpa_queue(wpa, channel, WC_SCAN);
			break;
		case WBR_SAVE_CONFIGURATION:
			_wpa_queue(wpa, channel, WC_SAVE_CONFIGURATION);
			break;
	}
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	char const * configfile = NULL;
	char const * interface = NULL;
	int o;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "c:i:")) != -1)
		switch(o)
		{
			case 'c':
				configfile = optarg;
				break;
			case 'i':
				interface = optarg;
				break;
			default:
				return _usage();
		}
	if(optind != argc)
		return _usage();
	return (_wifibrowser(configfile, interface) == 0) ? 0 : 2;
}
