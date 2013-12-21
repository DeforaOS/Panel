/* $Id$ */
/* Copyright (c) 2011-2013 Pierre Pronchery <khorben@defora.org> */
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
/* TODO:
 * - track password requests
 * - tooltip with details on the button (interface tracked...)
 * - more tooltip details on the menu entries (channel, security settings...)
 * - icons mentioning the security level (emblem-nowrite?) */



#include <System.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include "Panel.h"
#define _(string) gettext(string)

/* constants */
#ifndef TMPDIR
# define TMPDIR			"/tmp"
#endif
#ifndef WPA_SUPPLICANT_PATH
# define WPA_SUPPLICANT_PATH	"/var/run/wpa_supplicant"
#endif

/* macros */
#define min(a, b)		((a) < (b) ? (a) : (b))

/* portability */
#ifndef SUN_LEN
# define SUN_LEN(su)		sizeof(struct sockaddr_un)
#endif


/* wpa_supplicant */
/* private */
/* types */
typedef enum _WPACommand
{
	WC_ADD_NETWORK = 0,	/* char const * ssid */
	WC_ATTACH,
	WC_DETACH,
	WC_ENABLE_NETWORK,	/* unsigned int id */
	WC_LIST_NETWORKS,
	WC_REASSOCIATE,
	WC_RECONFIGURE,
	WC_SAVE_CONFIGURATION,
	WC_SCAN,
	WC_SCAN_RESULTS,
	WC_SELECT_NETWORK,	/* unsigned int id */
	WC_SET_NETWORK,		/* unsigned int id, key, value */
	WC_SET_PASSWORD,	/* unsigned int id, char const * password */
	WC_STATUS,
	WC_TERMINATE
} WPACommand;

typedef struct _WPAEntry
{
	WPACommand command;
	char * buf;
	size_t buf_cnt;
	/* for WC_ADD_NETWORK */
	char * ssid;
} WPAEntry;

typedef struct _WPAChannel
{
	/* FIXME dynamically allocate instead */
	char path[256];
	int fd;
	GIOChannel * channel;
	guint rd_source;
	guint wr_source;

	WPAEntry * queue;
	size_t queue_cnt;
} WPAChannel;

typedef struct _WPANetwork
{
	unsigned int id;
	char * name;
	int enabled;
} WPANetwork;

typedef enum _WPAScanResult
{
	WSR_UPDATED = 0,
	WSR_ICON,
	WSR_BSSID,
	WSR_FREQUENCY,
	WSR_LEVEL,
	WSR_SSID
} WPAScanResult;
#define WSR_LAST WSR_SSID
#define WSR_COUNT (WSR_LAST + 1)

typedef struct _PanelApplet
{
	PanelAppletHelper * helper;

	guint source;
	WPAChannel channel[2];

	WPANetwork * networks;
	size_t networks_cnt;
	ssize_t networks_cur;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * image;
#ifndef EMBEDDED
	GtkWidget * label;
#endif
	GtkListStore * store;
} WPA;


/* prototypes */
static WPA * _wpa_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _wpa_destroy(WPA * wpa);

static int _wpa_error(WPA * wpa, char const * message, int ret);

static int _wpa_queue(WPA * wpa, WPAChannel * channel, WPACommand command, ...);

static int _wpa_reset(WPA * wpa);
static int _wpa_start(WPA * wpa);
static void _wpa_stop(WPA * wpa);

/* callbacks */
static void _on_clicked(gpointer data);
static gboolean _on_timeout(gpointer data);
static gboolean _on_watch_can_read(GIOChannel * source, GIOCondition condition,
		gpointer data);
static gboolean _on_watch_can_write(GIOChannel * source, GIOCondition condition,
		gpointer data);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Wifi",
	"network-wireless",
	NULL,
	_wpa_init,
	_wpa_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* wpa_init */
static void _init_channel(WPAChannel * channel);

static WPA * _wpa_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	WPA * wpa;
	GtkWidget * ret;
	GtkWidget * hbox;
	PangoFontDescription * bold;

	if((wpa = object_new(sizeof(*wpa))) == NULL)
		return NULL;
	wpa->helper = helper;
	wpa->source = 0;
	_init_channel(&wpa->channel[0]);
	_init_channel(&wpa->channel[1]);
	wpa->networks = NULL;
	wpa->networks_cnt = 0;
	wpa->networks_cur = -1;
	/* widgets */
	bold = pango_font_description_new();
	pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
	hbox = gtk_hbox_new(FALSE, 4);
	wpa->image = gtk_image_new_from_stock(GTK_STOCK_DISCONNECT,
			helper->icon_size);
	gtk_box_pack_start(GTK_BOX(hbox), wpa->image, FALSE, TRUE, 0);
#ifndef EMBEDDED
	wpa->label = gtk_label_new(" ");
	gtk_widget_modify_font(wpa->label, bold);
	gtk_box_pack_start(GTK_BOX(hbox), wpa->label, FALSE, TRUE, 0);
#endif
	wpa->store = gtk_list_store_new(WSR_COUNT, G_TYPE_BOOLEAN,
			GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_UINT,
			G_TYPE_UINT, G_TYPE_STRING);
	_wpa_start(wpa);
	gtk_widget_show_all(hbox);
	pango_font_description_free(bold);
	if(helper->type == PANEL_APPLET_TYPE_NOTIFICATION)
		wpa->widget = hbox;
	else
	{
		ret = gtk_button_new();
		gtk_button_set_relief(GTK_BUTTON(ret), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_widget_set_tooltip_text(ret, _("Wireless networking"));
#endif
		g_signal_connect_swapped(ret, "clicked", G_CALLBACK(
					_on_clicked), wpa);
		gtk_container_add(GTK_CONTAINER(ret), hbox);
		wpa->widget = ret;
	}
	*widget = wpa->widget;
	return wpa;
}

static void _init_channel(WPAChannel * channel)
{
	channel->fd = -1;
	channel->channel = NULL;
	channel->rd_source = 0;
	channel->wr_source = 0;
	channel->queue = NULL;
	channel->queue_cnt = 0;
}


/* wpa_destroy */
static void _wpa_destroy(WPA * wpa)
{
	_wpa_stop(wpa);
	gtk_widget_destroy(wpa->widget);
	object_delete(wpa);
}


/* wpa_error */
static int _wpa_error(WPA * wpa, char const * message, int ret)
{
	gtk_image_set_from_icon_name(GTK_IMAGE(wpa->image), "error",
			wpa->helper->icon_size);
#ifndef EMBEDDED
	gtk_label_set_text(GTK_LABEL(wpa->label), _("Error"));
#endif
	return wpa->helper->error(NULL, message, ret);
}


/* wpa_queue */
static int _wpa_queue(WPA * wpa, WPAChannel * channel, WPACommand command, ...)
{
	va_list ap;
	unsigned int u;
	char * cmd = NULL;
	WPAEntry * p;
	char const * ssid = NULL;
	char const * s;
	char const * t;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%u, ...)\n", __func__, command);
#endif
	if(channel->channel == NULL)
		return -1;
	va_start(ap, command);
	switch(command)
	{
		case WC_ADD_NETWORK:
			cmd = g_strdup_printf("ADD_NETWORK");
			ssid = va_arg(ap, char const *);
			break;
		case WC_ATTACH:
			cmd = strdup("ATTACH");
			break;
		case WC_DETACH:
			cmd = strdup("DETACH");
			break;
		case WC_ENABLE_NETWORK:
			u = va_arg(ap, unsigned int);
			cmd = g_strdup_printf("ENABLE_NETWORK %u", u);
			break;
		case WC_LIST_NETWORKS:
			cmd = strdup("LIST_NETWORKS");
			break;
		case WC_REASSOCIATE:
			cmd = strdup("REASSOCIATE");
			break;
		case WC_RECONFIGURE:
			cmd = strdup("RECONFIGURE");
			break;
		case WC_SAVE_CONFIGURATION:
			cmd = strdup("SAVE_CONFIG");
			break;
		case WC_SCAN:
			cmd = strdup("SCAN");
			break;
		case WC_SCAN_RESULTS:
			cmd = strdup("SCAN_RESULTS");
			break;
		case WC_SELECT_NETWORK:
			u = va_arg(ap, unsigned int);
			cmd = g_strdup_printf("SELECT_NETWORK %u", u);
			break;
		case WC_SET_NETWORK:
			u = va_arg(ap, unsigned int);
			s = va_arg(ap, char const *);
			t = va_arg(ap, char const *);
			cmd = g_strdup_printf("SET_NETWORK %u %s \"%s\"", u, s,
					t);
			break;
		case WC_SET_PASSWORD:
			u = va_arg(ap, unsigned int);
			s = va_arg(ap, char const *);
			cmd = g_strdup_printf("PASSWORD %u \"%s\"", u, s);
			break;
		case WC_STATUS:
			cmd = strdup("STATUS-VERBOSE");
			break;
		case WC_TERMINATE:
			cmd = strdup("TERMINATE");
			break;
	}
	va_end(ap);
	if(cmd == NULL)
		return -1;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, cmd);
#endif
	if((p = realloc(channel->queue, sizeof(*p) * (channel->queue_cnt + 1)))
			== NULL)
	{
		free(cmd);
		return -1;
	}
	channel->queue = p;
	p = &channel->queue[channel->queue_cnt];
	p->command = command;
	p->buf = cmd;
	p->buf_cnt = strlen(cmd);
	/* XXX may fail */
	p->ssid = (ssid != NULL) ? strdup(ssid) : NULL;
	if(channel->queue_cnt++ == 0)
		channel->wr_source = g_io_add_watch(channel->channel, G_IO_OUT,
				_on_watch_can_write, wpa);
	return 0;
}


/* wpa_reset */
static int _wpa_reset(WPA * wpa)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	_wpa_stop(wpa);
	return _wpa_start(wpa);
}


/* wpa_start */
static gboolean _start_timeout(gpointer data);
static int _timeout_channel(WPA * wpa, WPAChannel * channel);
static int _timeout_channel_interface(WPA * wpa, WPAChannel * channel,
		char const * path, char const * interface);

static int _wpa_start(WPA * wpa)
{
	if(wpa->source != 0)
		g_source_remove(wpa->source);
	wpa->source = 0;
	/* reconnect to the daemon */
	_start_timeout(wpa);
	return 0;
}

static gboolean _start_timeout(gpointer data)
{
	WPA * wpa = data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(_timeout_channel(wpa, &wpa->channel[0]) != 0
			|| _timeout_channel(wpa, &wpa->channel[1]) != 0)
	{
		_wpa_stop(wpa);
		wpa->source = g_timeout_add(5000, _start_timeout, wpa);
		return FALSE;
	}
	_on_timeout(wpa);
	wpa->source = g_timeout_add(5000, _on_timeout, wpa);
	_wpa_queue(wpa, &wpa->channel[1], WC_ATTACH);
	return FALSE;
}

static int _timeout_channel(WPA * wpa, WPAChannel * channel)
{
	int ret;
	char const path[] = WPA_SUPPLICANT_PATH;
	char const * interface;
	char const * p;
	DIR * dir;
	struct dirent * de;
	struct sockaddr_un lu;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if((p = getenv("TMPDIR")) == NULL)
		p = TMPDIR;
	if(snprintf(channel->path, sizeof(channel->path), "%s%s", p,
				"/panel_wpa_supplicant.XXXXXX")
			>= (int)sizeof(channel->path))
		return -wpa->helper->error(NULL, "snprintf", 1);
	if(mktemp(channel->path) == NULL)
		return -wpa->helper->error(NULL, "mktemp", 1);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, channel->path);
#endif
	/* create the local socket */
	memset(&lu, 0, sizeof(lu));
	if(snprintf(lu.sun_path, sizeof(lu.sun_path), "%s", channel->path)
			>= (int)sizeof(lu.sun_path))
	{
		unlink(channel->path);
		/* XXX make sure this error is explicit enough */
		return -_wpa_error(wpa, channel->path, 1);
	}
	lu.sun_family = AF_LOCAL;
	if((channel->fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1)
		return -_wpa_error(wpa, "socket", 1);
	if(bind(channel->fd, (struct sockaddr *)&lu, SUN_LEN(&lu)) != 0)
	{
		close(channel->fd);
		unlink(channel->path);
		return -_wpa_error(wpa, channel->path, 1);
	}
	if((interface = wpa->helper->config_get(wpa->helper->panel,
					"wpa_supplicant", "interface")) != NULL)
	{
		ret = _timeout_channel_interface(wpa, channel, path, interface);
		if(ret != 0)
			wpa->helper->error(NULL, interface, 1);
	}
	/* look for the first available interface */
	else if((dir = opendir(path)) != NULL)
	{
		for(ret = -1; ret != 0 && (de = readdir(dir)) != NULL;)
			ret = _timeout_channel_interface(wpa, channel, path,
					de->d_name);
		closedir(dir);
	}
	else
		ret = -wpa->helper->error(NULL, path, 1);
	if(ret != 0)
	{
		close(channel->fd);
		unlink(channel->path);
		channel->fd = -1;
	}
	return ret;
}

static int _timeout_channel_interface(WPA * wpa, WPAChannel * channel,
		char const * path, char const * interface)
{
	struct stat st;
	struct sockaddr_un ru;

	memset(&ru, 0, sizeof(ru));
	ru.sun_family = AF_UNIX;
	if(snprintf(ru.sun_path, sizeof(ru.sun_path), "%s/%s", path, interface)
			>= (int)sizeof(ru.sun_path)
			|| lstat(ru.sun_path, &st) != 0
			|| (st.st_mode & S_IFSOCK) != S_IFSOCK)
		return -1;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, interface);
#endif
	/* connect to the wpa_supplicant daemon */
	if(connect(channel->fd, (struct sockaddr *)&ru, SUN_LEN(&ru)) != 0)
		return -wpa->helper->error(NULL, "connect", 1);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() connected to %s\n", __func__, interface);
#endif
#ifndef EMBEDDED
	/* XXX will be done for every channel */
	gtk_label_set_text(GTK_LABEL(wpa->label), interface);
#endif
	channel->channel = g_io_channel_unix_new(channel->fd);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %p\n", __func__, (void *)channel->channel);
#endif
	g_io_channel_set_encoding(channel->channel, NULL, NULL);
	g_io_channel_set_buffered(channel->channel, FALSE);
	channel->rd_source = g_io_add_watch(channel->channel, G_IO_IN,
			_on_watch_can_read, wpa);
	return 0;
}


/* wpa_stop */
static void _stop_channel(WPA * wpa, WPAChannel * channel);

static void _wpa_stop(WPA * wpa)
{
	size_t i;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* de-register the event source */
	if(wpa->source != 0)
		g_source_remove(wpa->source);
	wpa->source = 0;
	_stop_channel(wpa, &wpa->channel[0]);
	_stop_channel(wpa, &wpa->channel[1]);
	/* free the network list */
	gtk_list_store_clear(wpa->store);
	for(i = 0; i < wpa->networks_cnt; i++)
		free(wpa->networks[i].name);
	free(wpa->networks);
	wpa->networks = NULL;
	wpa->networks_cnt = 0;
	wpa->networks_cur = -1;
	/* report the status */
	gtk_image_set_from_stock(GTK_IMAGE(wpa->image), GTK_STOCK_DISCONNECT,
			wpa->helper->icon_size);
#ifndef EMBEDDED
	gtk_label_set_text(GTK_LABEL(wpa->label), _("Not running"));
#endif
}

static void _stop_channel(WPA * wpa, WPAChannel * channel)
{
	size_t i;

	if(channel->rd_source != 0)
		g_source_remove(channel->rd_source);
	channel->rd_source = 0;
	if(channel->wr_source != 0)
		g_source_remove(channel->wr_source);
	channel->wr_source = 0;
	/* free the command queue */
	for(i = 0; i < channel->queue_cnt; i++)
	{
		free(channel->queue[i].buf);
		free(channel->queue[i].ssid);
	}
	free(channel->queue);
	channel->queue = NULL;
	channel->queue_cnt = 0;
	/* close and remove the socket */
	if(channel->channel != NULL)
	{
		g_io_channel_shutdown(channel->channel, TRUE, NULL);
		g_io_channel_unref(channel->channel);
		channel->channel = NULL;
		channel->fd = -1;
	}
	unlink(channel->path);
	if(channel->fd != -1 && close(channel->fd) != 0)
		wpa->helper->error(NULL, channel->path, 1);
	channel->fd = -1;
}


/* callbacks */
/* on_clicked */
static void _clicked_network_list(WPA * wpa, GtkWidget * menu);
static void _clicked_network_view(WPA * wpa, GtkWidget * menu);
static void _clicked_position_menu(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data);
/* callbacks */
static void _clicked_on_network_activated(GtkWidget * widget, gpointer data);
static void _clicked_on_network_toggled(GtkWidget * widget, gpointer data);
static void _clicked_on_reassociate(gpointer data);
static void _clicked_on_rescan(gpointer data);
static void _clicked_on_save_configuration(gpointer data);

static void _on_clicked(gpointer data)
{
	WPA * wpa = data;
	GtkWidget * menu;
	GtkWidget * menuitem;
	GtkWidget * image;

	menu = gtk_menu_new();
	/* FIXME summarize the status instead */
	_clicked_network_list(wpa, menu);
	/* reassociate */
	menuitem = gtk_image_menu_item_new_with_label(_("Reassociate"));
#if GTK_CHECK_VERSION(2, 12, 0)
	image = gtk_image_new_from_stock(GTK_STOCK_DISCARD, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
#endif
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_clicked_on_reassociate), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	/* rescan */
	menuitem = gtk_image_menu_item_new_with_label(_("Rescan"));
	image = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_clicked_on_rescan), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	/* save configuration */
	menuitem = gtk_image_menu_item_new_with_label(_("Save configuration"));
	image = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_clicked_on_save_configuration), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	/* view */
	_clicked_network_view(wpa, menu);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, _clicked_position_menu,
			wpa, 0, gtk_get_current_event_time());
}

static void _clicked_network_list(WPA * wpa, GtkWidget * menu)
{
	GtkWidget * menuitem;
	GtkWidget * submenu;
	GSList * group;
	size_t i;

	if(wpa->networks_cnt == 0)
		return;
	/* network list */
	menuitem = gtk_image_menu_item_new_with_label(_("Network list"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
	/* network list: any network */
	menuitem = gtk_radio_menu_item_new_with_label(NULL, _("Any network"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
	g_signal_connect(menuitem, "toggled", G_CALLBACK(
				_clicked_on_network_toggled), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	/* network list: every known network */
	for(i = 0; i < wpa->networks_cnt; i++)
	{
		menuitem = gtk_radio_menu_item_new_with_label(group,
				wpa->networks[i].name);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(
					menuitem));
		g_object_set_data(G_OBJECT(menuitem), "network",
				&wpa->networks[i]);
		if(wpa->networks_cur >= 0 && i == (size_t)wpa->networks_cur)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
						menuitem), TRUE);
		g_signal_connect(menuitem, "toggled", G_CALLBACK(
					_clicked_on_network_toggled), wpa);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	}
	/* separator */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static void _clicked_network_view(WPA * wpa, GtkWidget * menu)
{
	GtkTreeModel * model = GTK_TREE_MODEL(wpa->store);
	GtkWidget * menuitem;
	GtkWidget * image;
	GtkTreeIter iter;
	gboolean valid;
	GdkPixbuf * pixbuf;
	gchar * bssid;
	guint frequency;
	guint level;
	gchar * ssid;
#if GTK_CHECK_VERSION(2, 12, 0)
	char buf[80];
#endif

	if((valid = gtk_tree_model_get_iter_first(model, &iter)) == FALSE)
		return;
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	for(; valid == TRUE; valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, WSR_ICON, &pixbuf,
				WSR_BSSID, &bssid, WSR_FREQUENCY, &frequency,
				WSR_LEVEL, &level, WSR_SSID, &ssid, -1);
		menuitem = gtk_image_menu_item_new_with_label((ssid != NULL)
				? ssid : bssid);
#if GTK_CHECK_VERSION(2, 12, 0)
		snprintf(buf, sizeof(buf), _("Frequency: %u\nLevel: %u"),
				frequency, level);
		gtk_widget_set_tooltip_text(menuitem, buf);
#endif
		if(ssid != NULL)
			g_object_set_data(G_OBJECT(menuitem), "ssid", ssid);
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
				image);
		g_signal_connect(menuitem, "activate", G_CALLBACK(
					_clicked_on_network_activated), wpa);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		g_free(bssid);
#if 0 /* XXX memory leak (for g_object_set_data() above) */
		g_free(ssid);
#endif
	}
}

static void _clicked_on_network_activated(GtkWidget * widget, gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel = &wpa->channel[0];
	char const * ssid;

	if((ssid = g_object_get_data(G_OBJECT(widget), "ssid")) == NULL)
		/* FIXME implement */
		return;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, ssid);
#endif
	_wpa_queue(wpa, channel, WC_ADD_NETWORK, ssid);
}

static void _clicked_on_network_toggled(GtkWidget * widget, gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel = &wpa->channel[0];
	WPANetwork * network;
	size_t i;

	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == FALSE)
		return;
	if((network = g_object_get_data(G_OBJECT(widget), "network")) == NULL)
	{
		/* enable every network again */
		for(i = 0; i < wpa->networks_cnt; i++)
			_wpa_queue(wpa, channel, WC_ENABLE_NETWORK, i);
		_wpa_queue(wpa, channel, WC_LIST_NETWORKS);
		return;
	}
	/* select this network */
	for(i = 0; i < wpa->networks_cnt; i++)
		if(network == &wpa->networks[i])
		{
			network->enabled = 1;
			wpa->networks_cur = i;
		}
		else
			wpa->networks[i].enabled = 0;
	_wpa_queue(wpa, channel, WC_SELECT_NETWORK, network->id);
}

static void _clicked_on_reassociate(gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel = &wpa->channel[0];

	_wpa_queue(wpa, channel, WC_REASSOCIATE);
}

static void _clicked_on_rescan(gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel = &wpa->channel[0];

	_wpa_queue(wpa, channel, WC_SCAN);
}

static void _clicked_on_save_configuration(gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel = &wpa->channel[0];

	_wpa_queue(wpa, channel, WC_SAVE_CONFIGURATION);
}

static void _clicked_position_menu(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data)
{
	WPA * wpa = data;
	GtkAllocation a;

#if GTK_CHECK_VERSION(2, 18, 0)
	gtk_widget_get_allocation(wpa->widget, &a);
#else
	a = wpa->widget->allocation;
#endif
	*x = a.x;
	*y = a.y;
	wpa->helper->position_menu(wpa->helper->panel, menu, x, y, push_in);
}


/* on_timeout */
static gboolean _on_timeout(gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel = &wpa->channel[0];

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(wpa->networks == NULL)
	{
		_wpa_queue(wpa, channel, WC_LIST_NETWORKS);
		_wpa_queue(wpa, channel, WC_SCAN_RESULTS);
	}
	_wpa_queue(wpa, channel, WC_STATUS);
	return TRUE;
}


/* on_watch_can_read */
static void _read_add_network(WPA * wpa, WPAChannel * channel, char const * buf,
		size_t cnt, char const * ssid);
static void _read_list_networks(WPA * wpa, char const * buf, size_t cnt);
static void _read_scan_results(WPA * wpa, char const * buf, size_t cnt);
static gboolean _read_scan_results_flags(WPA * wpa, char const * flags);
static void _read_scan_results_iter(WPA * wpa, GtkTreeIter * iter,
		char const * bssid);
static GdkPixbuf * _read_scan_results_pixbuf(GtkIconTheme * icontheme,
		gint size, guint level, gboolean encrypted);
static void _read_status(WPA * wpa, char const * buf, size_t cnt);
static void _read_unsolicited(WPA * wpa, char const * buf, size_t cnt);

static gboolean _on_watch_can_read(GIOChannel * source, GIOCondition condition,
		gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel;
	WPAEntry * entry;
	char buf[4096]; /* XXX in wpa */
	gsize cnt;
	GError * error = NULL;
	GIOStatus status;

	if(condition != G_IO_IN)
		return FALSE; /* should not happen */
	if(source == wpa->channel[0].channel
			&& wpa->channel[0].queue_cnt > 0
			&& wpa->channel[0].queue[0].buf_cnt == 0)
		channel = &wpa->channel[0];
	else if(source == wpa->channel[1].channel)
		channel = &wpa->channel[1];
	else
		return FALSE; /* should not happen */
	entry = (channel->queue_cnt > 0) ? &channel->queue[0] : NULL;
	status = g_io_channel_read_chars(source, buf, sizeof(buf), &cnt,
			&error);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"", __func__);
	fwrite(buf, sizeof(*buf), cnt, stderr);
	fprintf(stderr, "\"\n");
#endif
	switch(status)
	{
		case G_IO_STATUS_NORMAL:
			if(entry == NULL)
			{
				_read_unsolicited(wpa, buf, cnt);
				break;
			}
			if(cnt == 3 && strncmp(buf, "OK\n", cnt) == 0)
				break;
			if(cnt == 5 && strncmp(buf, "FAIL\n", cnt) == 0)
			{
				/* FIXME improve the error message */
				wpa->helper->error(NULL, _("Unknown error"), 0);
				break;
			}
			if(entry->command == WC_ADD_NETWORK)
				_read_add_network(wpa, channel, buf, cnt,
						entry->ssid);
			else if(entry->command == WC_LIST_NETWORKS)
				_read_list_networks(wpa, buf, cnt);
			else if(entry->command == WC_SCAN_RESULTS)
				_read_scan_results(wpa, buf, cnt);
			else if(entry->command == WC_STATUS)
				_read_status(wpa, buf, cnt);
			break;
		case G_IO_STATUS_ERROR:
			_wpa_error(wpa, error->message, 1);
		case G_IO_STATUS_EOF:
		default: /* should not happen */
			_wpa_reset(wpa);
			return FALSE;
	}
	if(entry != NULL)
	{
		free(channel->queue[0].ssid);
		memmove(&channel->queue[0], &channel->queue[1],
				sizeof(channel->queue[0])
				* (--channel->queue_cnt));
	}
	/* schedule commands again */
	if(channel->queue_cnt > 0 && channel->wr_source == 0)
		channel->wr_source = g_io_add_watch(channel->channel, G_IO_OUT,
				_on_watch_can_write, wpa);
	return TRUE;
}

static void _read_add_network(WPA * wpa, WPAChannel * channel, char const * buf,
		size_t cnt, char const * ssid)
{
	unsigned int id;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, ssid);
#endif
	if(cnt < 2 || buf[cnt - 1] != '\n')
		return;
	errno = 0;
	id = strtoul(buf, NULL, 10);
	if(buf[0] == '\0' || errno != 0)
		return;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u \"%s\"\n", __func__, id, ssid);
#endif
	_wpa_queue(wpa, channel, WC_SET_NETWORK, id, "ssid", ssid);
	_wpa_queue(wpa, channel, WC_ENABLE_NETWORK, id);
	_wpa_queue(wpa, channel, WC_LIST_NETWORKS);
}

static void _read_list_networks(WPA * wpa, char const * buf, size_t cnt)
{
	WPANetwork * n;
	size_t i;
	size_t j;
	char * p = NULL;
	char * q;
	unsigned int u;
	char ssid[80];
	char bssid[80];
	char flags[80];
	int res;

	for(i = 0; i < wpa->networks_cnt; i++)
		free(wpa->networks[i].name);
	free(wpa->networks);
	wpa->networks = NULL;
	wpa->networks_cnt = 0;
	wpa->networks_cur = -1;
	for(i = 0; i < cnt; i = j)
	{
		for(j = i; j < cnt; j++)
			if(buf[j] == '\n')
				break;
		if((q = realloc(p, ++j - i)) == NULL)
			continue;
		p = q;
		snprintf(p, j - i, "%s", &buf[i]);
		p[j - i - 1] = '\0';
#ifdef DEBUG
		fprintf(stderr, "DEBUG: line \"%s\"\n", p);
#endif
		if((res = sscanf(p, "%u %79[^\t] %79[^\t] [%79[^]]]", &u, ssid,
						bssid, flags)) >= 3)
		{
			ssid[sizeof(ssid) - 1] = '\0';
#ifdef DEBUG
			fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, ssid);
#endif
			/* FIXME store the scan results instead */
			if((n = realloc(wpa->networks, sizeof(*n)
							* (wpa->networks_cnt
								+ 1))) != NULL)
			{
				wpa->networks = n;
				n = &wpa->networks[wpa->networks_cnt];
				/* XXX ignore errors */
				n->id = u;
				n->name = strdup(ssid);
				n->enabled = 1;
				if(n->name != NULL)
					wpa->networks_cnt++;
			}
			if(res > 3 && strcmp(flags, "DISABLED") == 0)
				n->enabled = 0;
			if(res > 3 && strcmp(flags, "CURRENT") == 0)
			{
				wpa->networks_cur = wpa->networks_cnt - 1;
				gtk_image_set_from_stock(GTK_IMAGE(wpa->image),
						GTK_STOCK_CONNECT,
						wpa->helper->icon_size);
#ifndef EMBEDDED
				gtk_label_set_text(GTK_LABEL(wpa->label), ssid);
#endif
			}
		}
	}
	free(p);
	if(wpa->networks_cur < 0)
	{
		/* determine if only one network is enabled */
		for(i = 0, j = 0; i < wpa->networks_cnt; i++)
			if(wpa->networks[i].enabled)
				j++;
		if(wpa->networks_cnt > 1 && j == 1)
			for(i = 0, j = 0; i < wpa->networks_cnt; i++)
				if(wpa->networks[i].enabled)
				{
					/* set as the current network */
					wpa->networks_cur = i;
					break;
				}
	}
}

static void _read_scan_results(WPA * wpa, char const * buf, size_t cnt)
{
	GtkTreeModel * model = GTK_TREE_MODEL(wpa->store);
	GtkIconTheme * icontheme;
	gint size = 16;
	gboolean valid;
	size_t i;
	size_t j;
	char * p = NULL;
	char * q;
	int res;
	GdkPixbuf * pixbuf;
	char bssid[18];
	unsigned int frequency;
	unsigned int level;
	char flags[80];
	char ssid[80];
	gboolean protected;
	GtkTreeIter iter;

	icontheme = gtk_icon_theme_get_default();
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &size, &size);
	/* mark every entry as obsolete */
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
		gtk_list_store_set(wpa->store, &iter, WSR_UPDATED, FALSE, -1);
	for(i = 0; i < cnt; i = j)
	{
		for(j = i; j < cnt; j++)
			if(buf[j] == '\n')
				break;
		if((q = realloc(p, ++j - i)) == NULL)
			continue;
		p = q;
		snprintf(p, j - i, "%s", &buf[i]);
		p[j - i - 1] = '\0';
#ifdef DEBUG
		fprintf(stderr, "DEBUG: line \"%s\"\n", p);
#endif
		if((res = sscanf(p, "%17s %u %u %79s %79[^\n]", bssid,
						&frequency, &level, flags,
						ssid)) >= 3)
		{
			bssid[sizeof(bssid) - 1] = '\0';
			flags[sizeof(flags) - 1] = '\0';
			ssid[sizeof(ssid) - 1] = '\0';
#ifdef DEBUG
			fprintf(stderr, "DEBUG: %s() \"%s\" %u %u %s %s\n",
					__func__, bssid, frequency, level,
					flags, ssid);
#endif
			protected = _read_scan_results_flags(wpa, flags);
			pixbuf = _read_scan_results_pixbuf(icontheme, size,
					level, protected);
			_read_scan_results_iter(wpa, &iter, bssid);
			gtk_list_store_set(wpa->store, &iter, WSR_UPDATED, TRUE,
					WSR_ICON, pixbuf, WSR_BSSID, bssid,
					WSR_FREQUENCY, frequency,
					WSR_LEVEL, level, -1);
			if(res == 5)
				gtk_list_store_set(wpa->store, &iter,
						WSR_SSID, ssid, -1);
		}
	}
	free(p);
	/* remove the outdated entries */
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;)
	{
		gtk_tree_model_get(model, &iter, WSR_UPDATED, &valid, -1);
		if(valid == FALSE)
			valid = gtk_list_store_remove(wpa->store, &iter);
		else
			valid = gtk_tree_model_iter_next(model, &iter);
	}
}

static gboolean _read_scan_results_flags(WPA * wpa, char const * flags)
{
	char const * p;
	char const wpa1[] = "WPA-PSK-";
	char const wpa2[] = "WPA2-PSK-";

	for(p = flags; *p != '\0'; p++)
		if(*p == '[')
		{
			/* FIXME really implement */
			if(strncmp(wpa1, &p[1], sizeof(wpa1) - 1) == 0)
				return TRUE;
			if(strncmp(wpa2, &p[1], sizeof(wpa2) - 1) == 0)
				return TRUE;
		}
	return FALSE;
}

static void _read_scan_results_iter(WPA * wpa, GtkTreeIter * iter,
		char const * bssid)
{
	GtkTreeModel * model = GTK_TREE_MODEL(wpa->store);
	gboolean valid;
	gchar * b;
	int res;

	for(valid = gtk_tree_model_get_iter_first(model, iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, WSR_BSSID, &b, -1);
		res = (b != NULL && strcmp(b, bssid) == 0) ? 0 : -1;
		g_free(b);
		if(res == 0)
			return;
	}
	gtk_list_store_append(wpa->store, iter);
}

static GdkPixbuf * _read_scan_results_pixbuf(GtkIconTheme * icontheme,
		gint size, guint level, gboolean encrypted)
{
	GdkPixbuf * ret;
	char const * name;
#if GTK_CHECK_VERSION(2, 14, 0)
	const int flags = GTK_ICON_LOOKUP_USE_BUILTIN
		| GTK_ICON_LOOKUP_FORCE_SIZE;
#else
	const int flags = GTK_ICON_LOOKUP_USE_BUILTIN;
#endif
	GdkPixbuf * pixbuf;
	/* FIXME implement more levels of security */
	char const * emblem = encrypted ? "stock_lock" : "stock_lock-open";

	/* FIXME check if the mapping is right (and use our own icons) */
	if(level >= 100)
		name = "phone-signal-100";
	else if(level >= 75)
		name = "phone-signal-75";
	else if(level >= 50)
		name = "phone-signal-50";
	else if(level >= 25)
		name = "phone-signal-25";
	else
		name = "phone-signal-00";
	if((pixbuf = gtk_icon_theme_load_icon(icontheme, name, size, 0, NULL))
			== NULL)
		return NULL;
	if((ret = gdk_pixbuf_copy(pixbuf)) == NULL)
		return pixbuf;
	g_object_unref(pixbuf);
	size = min(24, size / 2);
	pixbuf = gtk_icon_theme_load_icon(icontheme, emblem, size, flags, NULL);
	if(pixbuf != NULL)
	{
		gdk_pixbuf_composite(pixbuf, ret, 0, 0, size, size, 0, 0, 1.0,
				1.0, GDK_INTERP_NEAREST, 255);
		g_object_unref(pixbuf);
	}
	return ret;
}

static void _read_status(WPA * wpa, char const * buf, size_t cnt)
{
	size_t i;
	size_t j;
	char * p = NULL;
	char * q;
	char variable[80];
	char value[80];

	for(i = 0; i < cnt; i = j)
	{
		for(j = i; j < cnt; j++)
			if(buf[j] == '\n')
				break;
		if((q = realloc(p, ++j - i)) == NULL)
			continue;
		p = q;
		snprintf(p, j - i, "%s", &buf[i]);
		p[j - i - 1] = '\0';
#ifdef DEBUG
		fprintf(stderr, "DEBUG: line \"%s\"\n", p);
#endif
		if(sscanf(p, "%79[^=]=%79[^\n]", variable, value) != 2)
			continue;
		variable[sizeof(variable) - 1] = '\0';
		value[sizeof(value) - 1] = '\0';
		if(strcmp(variable, "wpa_state") == 0)
		{
			gtk_image_set_from_stock(GTK_IMAGE(wpa->image),
					(strcmp(value, "COMPLETED") == 0)
					? GTK_STOCK_CONNECT
					: GTK_STOCK_DISCONNECT,
					wpa->helper->icon_size);
			if(strcmp(value, "SCANNING") == 0)
				gtk_label_set_text(GTK_LABEL(wpa->label),
						_("Scanning..."));
		}
#ifndef EMBEDDED
		if(strcmp(variable, "ssid") == 0)
			gtk_label_set_text(GTK_LABEL(wpa->label), value);
#endif
	}
	free(p);
}

static void _read_unsolicited(WPA * wpa, char const * buf, size_t cnt)
{
	char const scan_results[] = "CTRL-EVENT-SCAN-RESULTS";
	size_t i;
	size_t j;
	char * p = NULL;
	char * q;
	unsigned int u;
	char event[80];
	unsigned int v;
	char bssid[80];

	for(i = 0; i < cnt; i = j)
	{
		for(j = i; j < cnt; j++)
			if(buf[j] == '\n')
				break;
		if((q = realloc(p, ++j - i)) == NULL)
			continue;
		p = q;
		snprintf(p, j - i, "%s", &buf[i]);
		p[j - i - 1] = '\0';
#ifdef DEBUG
		fprintf(stderr, "DEBUG: line \"%s\"\n", p);
#endif
		if(sscanf(p, "<%u>%79s %u %79s\n", &u, event, &v, bssid) < 2)
			continue;
		event[sizeof(event) - 1] = '\0';
		bssid[sizeof(bssid) - 1] = '\0';
		if(strcmp(event, scan_results) == 0)
			_wpa_queue(wpa, &wpa->channel[0], WC_SCAN_RESULTS);
		/* FIXME implement the other events */
	}
	free(p);
}


/* on_watch_can_write */
static gboolean _on_watch_can_write(GIOChannel * source, GIOCondition condition,
		gpointer data)
{
	WPA * wpa = data;
	WPAChannel * channel;
	WPAEntry * entry;
	gsize cnt = 0;
	GError * error = NULL;
	GIOStatus status;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(condition != G_IO_OUT)
		return FALSE; /* should not happen */
	if(source == wpa->channel[0].channel
			&& wpa->channel[0].queue_cnt > 0
			&& wpa->channel[0].queue[0].buf_cnt != 0)
		channel = &wpa->channel[0];
	else if(source == wpa->channel[1].channel
			&& wpa->channel[1].queue_cnt > 0
			&& wpa->channel[1].queue[0].buf_cnt != 0)
		channel = &wpa->channel[1];
	else
		return FALSE; /* should not happen */
	entry = &channel->queue[0];
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"", __func__);
	fwrite(entry->buf, sizeof(*entry->buf), entry->buf_cnt, stderr);
	fprintf(stderr, "\"\n");
#endif
	status = g_io_channel_write_chars(source, entry->buf, entry->buf_cnt,
			&cnt, &error);
	if(cnt != 0)
	{
		memmove(entry->buf, &entry->buf[cnt], entry->buf_cnt - cnt);
		entry->buf_cnt -= cnt;
	}
	switch(status)
	{
		case G_IO_STATUS_NORMAL:
			break;
		case G_IO_STATUS_ERROR:
			_wpa_error(wpa, error->message, 1);
		case G_IO_STATUS_EOF:
		default: /* should not happen */
			_wpa_reset(wpa);
			return FALSE;
	}
	if(entry->buf_cnt != 0)
		/* partial packet */
		_wpa_reset(wpa);
	else
		channel->wr_source = 0;
	return FALSE;
}
