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
 * - configuration value for the interface to track
 * - more error checking
 * - determine if there is an asynchronous mode */



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
#include "Panel.h"

/* constants */
#ifndef TMPDIR
# define TMPDIR			"/tmp"
#endif
#ifndef WPA_SUPPLICANT_PATH
# define WPA_SUPPLICANT_PATH	"/var/run/wpa_supplicant"
#endif

/* portability */
#ifndef SUN_LEN
# define SUN_LEN(su)		sizeof(struct sockaddr_un)
#endif


/* wpa_supplicant */
/* private */
/* types */
typedef enum _WPACommand
{
	WC_ENABLE_NETWORK = 0,		/* unsigned int id */
	WC_LIST_NETWORKS,
	WC_REASSOCIATE,
	WC_SAVE_CONFIGURATION,
	WC_SCAN,
	WC_SCAN_RESULTS,
	WC_SELECT_NETWORK,		/* unsigned int id */
	WC_STATUS
} WPACommand;

typedef struct _WPANetwork
{
	unsigned int id;
	char * name;
	int enabled;
} WPANetwork;

typedef struct _WPAEntry
{
	WPACommand command;
	char * buf;
	size_t buf_cnt;
} WPAEntry;

typedef struct _PanelApplet
{
	PanelAppletHelper * helper;

	/* FIXME dynamically allocate instead */
	char path[256];
	guint source;
	int fd;
	GIOChannel * channel;
	guint rd_source;
	guint wr_source;

	WPAEntry * queue;
	size_t queue_cnt;

	WPANetwork * networks;
	size_t networks_cnt;
	ssize_t networks_cur;

	/* widgets */
	GtkWidget * image;
#ifndef EMBEDDED
	GtkWidget * label;
#endif
} WPA;


/* prototypes */
static WPA * _wpa_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _wpa_destroy(WPA * wpa);

static int _wpa_error(WPA * wpa, char const * message, int ret);
static int _wpa_queue(WPA * wpa, WPACommand command, ...);
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
	wpa->fd = -1;
	wpa->channel = NULL;
	wpa->rd_source = 0;
	wpa->wr_source = 0;
	wpa->queue = NULL;
	wpa->queue_cnt = 0;
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
	_wpa_start(wpa);
	gtk_widget_show_all(hbox);
	pango_font_description_free(bold);
	if(helper->type == PANEL_APPLET_TYPE_NOTIFICATION)
		*widget = hbox;
	else
	{
		ret = gtk_button_new();
		gtk_button_set_relief(GTK_BUTTON(ret), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_widget_set_tooltip_text(ret, "Wireless networking");
#endif
		g_signal_connect_swapped(ret, "clicked", G_CALLBACK(
					_on_clicked), wpa);
		gtk_container_add(GTK_CONTAINER(ret), hbox);
		*widget = ret;
	}
	return wpa;
}


/* wpa_destroy */
static void _wpa_destroy(WPA * wpa)
{
	_wpa_stop(wpa);
	object_delete(wpa);
}


/* wpa_error */
static int _wpa_error(WPA * wpa, char const * message, int ret)
{
	gtk_image_set_from_icon_name(GTK_IMAGE(wpa->image), "error",
			wpa->helper->icon_size);
#ifndef EMBEDDED
	gtk_label_set_text(GTK_LABEL(wpa->label), "Error");
#endif
	return wpa->helper->error(NULL, message, ret);
}


/* wpa_queue */
static int _wpa_queue(WPA * wpa, WPACommand command, ...)
{
	va_list ap;
	unsigned int u;
	char * cmd = NULL;
	WPAEntry * p;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%u, ...)\n", __func__, command);
#endif
	if(wpa->channel == NULL)
		return -1;
	va_start(ap, command);
	switch(command)
	{
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
		case WC_STATUS:
			cmd = strdup("STATUS-VERBOSE");
			break;
	}
	va_end(ap);
	if(cmd == NULL)
		return -1;
	if((p = realloc(wpa->queue, sizeof(*p) * (wpa->queue_cnt + 1))) == NULL)
	{
		free(cmd);
		return -1;
	}
	wpa->queue = p;
	p = &wpa->queue[wpa->queue_cnt];
	p->command = command;
	p->buf = cmd;
	p->buf_cnt = strlen(cmd);
	if(wpa->queue_cnt++ == 0)
		wpa->wr_source = g_io_add_watch(wpa->channel, G_IO_OUT,
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

static int _wpa_start(WPA * wpa)
{
	if(wpa->source != 0)
		g_source_remove(wpa->source);
	wpa->source = 0;
	/* reconnect to the daemon */
	if(_start_timeout(wpa) == FALSE)
		return 0;
	/* try again every five seconds */
	wpa->source = g_timeout_add(5000, _start_timeout, wpa);
	return 0;
}

static gboolean _start_timeout(gpointer data)
{
	gboolean ret = TRUE;
	WPA * wpa = data;
	char const path[] = WPA_SUPPLICANT_PATH;
	char const * p;
	DIR * dir;
	struct dirent * de;
	struct stat st;
	struct sockaddr_un lu;
	struct sockaddr_un ru;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if((p = getenv("TMPDIR")) == NULL)
		p = TMPDIR;
	snprintf(wpa->path, sizeof(wpa->path), "%s%s", p,
			"/panel_wpa_supplicant.XXXXXX");
	if(mktemp(wpa->path) == NULL)
	{
		wpa->helper->error(NULL, "mktemp", 1);
		return TRUE;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, wpa->path);
#endif
	if((dir = opendir(path)) == NULL)
	{
		gtk_image_set_from_stock(GTK_IMAGE(wpa->image),
				GTK_STOCK_DISCONNECT, wpa->helper->icon_size);
#ifndef EMBEDDED
		gtk_label_set_text(GTK_LABEL(wpa->label), "Not running");
#endif
		return wpa->helper->error(NULL, path, TRUE);
	}
	/* create the local socket */
	memset(&lu, 0, sizeof(lu));
	if(snprintf(lu.sun_path, sizeof(lu.sun_path), "%s", wpa->path)
			>= (int)sizeof(lu.sun_path))
	{
		unlink(wpa->path);
		/* XXX make sure this error is explicit enough */
		return _wpa_error(wpa, wpa->path, TRUE);
	}
	lu.sun_family = AF_LOCAL;
	if((wpa->fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1)
		return _wpa_error(wpa, "socket", TRUE);
	if(bind(wpa->fd, (struct sockaddr *)&lu, SUN_LEN(&lu)) != 0)
	{
		close(wpa->fd);
		unlink(wpa->path);
		return _wpa_error(wpa, wpa->path, TRUE);
	}
	/* connect to the wpa_supplicant daemon */
	memset(&ru, 0, sizeof(ru));
	ru.sun_family = AF_UNIX;
	while((de = readdir(dir)) != NULL)
	{
		if(snprintf(ru.sun_path, sizeof(ru.sun_path), "%s/%s", path,
					de->d_name) >= (int)sizeof(ru.sun_path)
				|| lstat(ru.sun_path, &st) != 0
				|| (st.st_mode & S_IFSOCK) != S_IFSOCK)
			continue;
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, de->d_name);
#endif
		if(connect(wpa->fd, (struct sockaddr *)&ru, SUN_LEN(&ru)) != 0)
		{
			wpa->helper->error(NULL, "connect", 1);
			continue;
		}
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() connected\n", __func__);
#endif
#ifndef EMBEDDED
		gtk_label_set_text(GTK_LABEL(wpa->label), de->d_name);
#endif
		wpa->channel = g_io_channel_unix_new(wpa->fd);
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() %p\n", __func__,
				(void *)wpa->channel);
#endif
		g_io_channel_set_encoding(wpa->channel, NULL, NULL);
		g_io_channel_set_buffered(wpa->channel, FALSE);
		_on_timeout(wpa);
		wpa->source = g_timeout_add(5000, _on_timeout, wpa);
		wpa->rd_source = g_io_add_watch(wpa->channel, G_IO_IN,
				_on_watch_can_read, wpa);
		ret = FALSE;
		break;
	}
	if(ret == TRUE)
	{
		unlink(wpa->path);
		close(wpa->fd);
		wpa->fd = -1;
	}
	closedir(dir);
	return ret;
}


/* wpa_stop */
static void _wpa_stop(WPA * wpa)
{
	size_t i;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	/* de-register the event sources */
	if(wpa->source != 0)
		g_source_remove(wpa->source);
	wpa->source = 0;
	if(wpa->rd_source != 0)
		g_source_remove(wpa->rd_source);
	wpa->rd_source = 0;
	if(wpa->wr_source != 0)
		g_source_remove(wpa->wr_source);
	wpa->wr_source = 0;
	/* free the command queue */
	for(i = 0; i < wpa->queue_cnt; i++)
		free(wpa->queue[i].buf);
	free(wpa->queue);
	wpa->queue = NULL;
	wpa->queue_cnt = 0;
	/* free the network list */
	for(i = 0; i < wpa->networks_cnt; i++)
		free(wpa->networks[i].name);
	free(wpa->networks);
	wpa->networks = NULL;
	wpa->networks_cnt = 0;
	wpa->networks_cur = -1;
	/* close and remove the socket */
	if(wpa->channel != NULL)
	{
		g_io_channel_shutdown(wpa->channel, TRUE, NULL);
		g_io_channel_unref(wpa->channel);
		wpa->channel = NULL;
		wpa->fd = -1;
	}
	unlink(wpa->path);
	if(wpa->fd != -1 && close(wpa->fd) != 0)
		wpa->helper->error(NULL, wpa->path, 1);
	wpa->fd = -1;
}


/* callbacks */
/* on_clicked */
static void _clicked_position_menu(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data);
/* callbacks */
static void _clicked_on_network_toggled(GtkWidget * widget, gpointer data);
static void _clicked_on_reassociate(gpointer data);
static void _clicked_on_rescan(gpointer data);
static void _clicked_on_save_configuration(gpointer data);

static void _on_clicked(gpointer data)
{
	WPA * wpa = data;
	GtkWidget * menu;
	GtkWidget * menuitem;
	GtkWidget * submenu;
	GtkWidget * image;
	GSList * group;
	size_t i;

	menu = gtk_menu_new();
	/* FIXME summarize the status instead */
	/* network list */
	menuitem = gtk_image_menu_item_new_with_label("Network list");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
	/* network list: any network */
	menuitem = gtk_radio_menu_item_new_with_label(NULL, "Any network");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
	g_signal_connect(menuitem, "toggled", G_CALLBACK(
				_clicked_on_network_toggled), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	/* network list: separator (if relevant) */
	if(wpa->networks_cnt > 0)
	{
		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	}
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
	/* actions */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	/* actions: reassociate */
	menuitem = gtk_menu_item_new_with_label("Reassociate");
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_clicked_on_reassociate), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	/* actions: rescan */
	menuitem = gtk_menu_item_new_with_label("Rescan");
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_clicked_on_rescan), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	/* actions: save configuration */
	menuitem = gtk_image_menu_item_new_with_label("Save configuration");
	image = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
				_clicked_on_save_configuration), wpa);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, _clicked_position_menu,
			wpa, 0, gtk_get_current_event_time());
}

static void _clicked_on_network_toggled(GtkWidget * widget, gpointer data)
{
	WPA * wpa = data;
	WPANetwork * network;
	size_t i;

	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == FALSE)
		return;
	if((network = g_object_get_data(G_OBJECT(widget), "network")) == NULL)
	{
		/* enable every network again */
		for(i = 0; i < wpa->networks_cnt; i++)
			_wpa_queue(wpa, WC_ENABLE_NETWORK, i);
		_wpa_queue(wpa, WC_LIST_NETWORKS);
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
	_wpa_queue(wpa, WC_SELECT_NETWORK, network->id);
}

static void _clicked_on_reassociate(gpointer data)
{
	WPA * wpa = data;

	_wpa_queue(wpa, WC_REASSOCIATE);
}

static void _clicked_on_rescan(gpointer data)
{
	WPA * wpa = data;

	_wpa_queue(wpa, WC_SCAN);
}

static void _clicked_on_save_configuration(gpointer data)
{
	WPA * wpa = data;

	_wpa_queue(wpa, WC_SAVE_CONFIGURATION);
}

static void _clicked_position_menu(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data)
{
	WPA * wpa = data;

	wpa->helper->position_menu(wpa->helper->panel, menu, x, y, push_in);
}


/* on_timeout */
static gboolean _on_timeout(gpointer data)
{
	WPA * wpa = data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(wpa->networks == NULL)
	{
		_wpa_queue(wpa, WC_LIST_NETWORKS);
#ifdef DEBUG
		_wpa_queue(wpa, WC_SCAN);
#endif
	}
	_wpa_queue(wpa, WC_STATUS);
#ifdef DEBUG
	_wpa_queue(wpa, WC_SCAN_RESULTS);
#endif
	return TRUE;
}


/* on_watch_can_read */
static void _read_scan_results(WPA * wpa, char const * buf, size_t cnt);
static void _read_status(WPA * wpa, char const * buf, size_t cnt);
static void _read_list_networks(WPA * wpa, char const * buf, size_t cnt);

static gboolean _on_watch_can_read(GIOChannel * source, GIOCondition condition,
		gpointer data)
{
	WPA * wpa = data;
	WPAEntry * entry = &wpa->queue[0];
	char buf[4096]; /* XXX in wpa */
	gsize cnt;
	GError * error = NULL;
	GIOStatus status;

	if(condition != G_IO_IN || source != wpa->channel
			|| wpa->queue_cnt == 0 || entry->buf_cnt != 0)
		return FALSE; /* should not happen */
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
			if(cnt == 3 && strncmp(buf, "OK\n", cnt) == 0)
				break;
			if(cnt == 5 && strncmp(buf, "FAIL\n", cnt) == 0)
			{
				/* FIXME improve the error message */
				wpa->helper->error(NULL, "An error occured", 0);
				break;
			}
			if(entry->command == WC_LIST_NETWORKS)
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
	memmove(entry, &wpa->queue[1], sizeof(*entry) * (--wpa->queue_cnt));
	if(wpa->queue_cnt != 0 && wpa->wr_source == 0)
		wpa->wr_source = g_io_add_watch(wpa->channel, G_IO_OUT,
				_on_watch_can_write, wpa);
	return TRUE;
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
	for(i = 0; i < cnt;)
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
		if((res = sscanf(p, "%u %79[^\t] %79[^\t] [%79[^]]", &u, ssid,
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
			else if(res >= 3 && wpa->networks_cur < 0)
				/* XXX may not be the only one enabled */
				wpa->networks_cur = wpa->networks_cnt - 1;
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
		i = j;
	}
	free(p);
}

static void _read_scan_results(WPA * wpa, char const * buf, size_t cnt)
{
	size_t i;
	size_t j;
	char * p = NULL;
	char * q;
	int res;
	char bssid[18];
	unsigned int frequency;
	unsigned int level;
	char flags1[32];
	char flags2[32];
	char ssid[80];

	for(i = 0; i < cnt;)
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
		if((res = sscanf(p, "%17s %u %u [%31[^]]][%31[^]] %79s", bssid,
						&frequency, &level, flags1,
						flags2, ssid)) == 6)
		{
			bssid[sizeof(bssid) - 1] = '\0';
			flags1[sizeof(flags1) - 1] = '\0';
			flags2[sizeof(flags2) - 1] = '\0';
			ssid[sizeof(ssid) - 1] = '\0';
#ifdef DEBUG
			fprintf(stderr, "DEBUG: %s() \"%s\" %u %u %s %s %s\n",
					__func__, bssid, frequency, level,
					flags1, flags2, ssid);
#endif
		}
		i = j;
	}
	free(p);
}

static void _read_status(WPA * wpa, char const * buf, size_t cnt)
{
	size_t i;
	size_t j;
	char * p = NULL;
	char * q;
	char variable[80];
	char value[80];

	for(i = 0; i < cnt;)
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
			gtk_image_set_from_stock(GTK_IMAGE(wpa->image),
					(strcmp(value, "COMPLETED") == 0)
					? GTK_STOCK_CONNECT
					: GTK_STOCK_DISCONNECT,
					wpa->helper->icon_size);
#ifndef EMBEDDED
		if(strcmp(variable, "ssid") == 0)
			gtk_label_set_text(GTK_LABEL(wpa->label), value);
#endif
		i = j;
	}
	free(p);
}


/* on_watch_can_write */
static gboolean _on_watch_can_write(GIOChannel * source, GIOCondition condition,
		gpointer data)
{
	WPA * wpa = data;
	WPAEntry * entry = &wpa->queue[0];
	gsize cnt = 0;
	GError * error = NULL;
	GIOStatus status;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	if(condition != G_IO_OUT || source != wpa->channel
			|| wpa->queue_cnt == 0 || entry->buf_cnt == 0)
	{
		wpa->wr_source = 0;
		return FALSE; /* should not happen */
	}
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
	{
		_wpa_reset(wpa);
		return FALSE;
	}
	wpa->wr_source = 0;
	return FALSE;
}
