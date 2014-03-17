/* $Id$ */
/* Copyright (c) 2011-2014 Pierre Pronchery <khorben@defora.org> */
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
 * - make sure it works with WEP networks
 * - remove the network list from the applet
 * - manage the network list from the browser (networks without reception...)
 * - automatically save the configuration by default (and remove the button)
 * - tooltip with details on the button (interface tracked...) */



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
	uint32_t flags;
	gboolean connect;
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
	WSR_FLAGS,
	WSR_SSID,
	WSR_SSID_DISPLAY,
	WSR_TOOLTIP,
	WSR_ENABLED,
	WSR_CAN_ENABLE
} WPAScanResult;
#define WSR_LAST WSR_CAN_ENABLE
#define WSR_COUNT (WSR_LAST + 1)

typedef enum _WPAScanResultFlag
{
	WSRF_IBSS	= 0x001,
	WSRF_WEP	= 0x002,
	WSRF_WPA	= 0x004,
	WSRF_WPA2	= 0x008,
	WSRF_PSK	= 0x010,
	WSRF_EAP	= 0x020,
	WSRF_CCMP	= 0x040,
	WSRF_TKIP	= 0x080,
	WSRF_PREAUTH	= 0x100,
	WSRF_ESS	= 0x200
} WPAScanResultFlag;

typedef struct _PanelApplet
{
	PanelAppletHelper * helper;

	guint source;
	WPAChannel channel[2];

	/* configuration */
	WPANetwork * networks;
	size_t networks_cnt;
	ssize_t networks_cur;
	gboolean autosave;

	/* status */
	gboolean connected;
	gboolean associated;
	guint level;
	uint32_t flags;

	/* widgets */
	GtkIconTheme * icontheme;
	GtkWidget * widget;
	GtkWidget * image;
	gboolean blink;
#ifndef EMBEDDED
	GtkWidget * label;
#endif
	GtkTreeStore * store;
	GtkWidget * pw_window;
	GtkWidget * pw_entry;
	unsigned int pw_id;
} WPA;


/* prototypes */
/* plug-in */
static WPA * _wpa_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _wpa_destroy(WPA * wpa);

/* accessors */
static GdkPixbuf * _wpa_get_icon(WPA * wpa, gint size, guint level,
		uint32_t flags);
static void _wpa_set_status(WPA * wpa, gboolean connected, gboolean associated,
		char const * network);

/* useful */
static int _wpa_error(WPA * wpa, char const * message, int ret);

static void _wpa_connect(WPA * wpa, char const * ssid, uint32_t flags);
static void _wpa_connect_network(WPA * wpa, WPANetwork * network);
static void _wpa_disconnect(WPA * wpa);
static void _wpa_rescan(WPA * wpa);

static void _wpa_ask_password(WPA * wpa, WPANetwork * network);

static void _wpa_notify(WPA * wpa, char const * message);

static int _wpa_queue(WPA * wpa, WPAChannel * channel, WPACommand command, ...);

static int _wpa_reset(WPA * wpa);
static int _wpa_start(WPA * wpa);
static void _wpa_stop(WPA * wpa);

static void _wpa_tooltip(char * buf, size_t buf_cnt, unsigned int frequency,
		unsigned int level, uint32_t flags);

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
	GtkWidget * hbox;
	PangoFontDescription * bold;
	char const * p;

	if((wpa = object_new(sizeof(*wpa))) == NULL)
		return NULL;
	wpa->helper = helper;
	wpa->source = 0;
	_init_channel(&wpa->channel[0]);
	_init_channel(&wpa->channel[1]);
	wpa->networks = NULL;
	wpa->networks_cnt = 0;
	wpa->networks_cur = -1;
	/* autosave only if explicitly configured */
	p = helper->config_get(helper->panel, "wpa_supplicant", "autosave");
	wpa->autosave = (p != NULL && strtol(p, NULL, 10) != 0) ? TRUE : FALSE;
	wpa->connected = FALSE;
	wpa->associated = FALSE;
	wpa->level = 0;
	wpa->flags = 0;
	/* widgets */
	wpa->icontheme = gtk_icon_theme_get_default();
	bold = pango_font_description_new();
	pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
	hbox = gtk_hbox_new(FALSE, 4);
	wpa->image = gtk_image_new();
	wpa->blink = FALSE;
	gtk_box_pack_start(GTK_BOX(hbox), wpa->image, FALSE, TRUE, 0);
#ifndef EMBEDDED
	wpa->label = gtk_label_new(" ");
	gtk_widget_modify_font(wpa->label, bold);
	gtk_box_pack_start(GTK_BOX(hbox), wpa->label, FALSE, TRUE, 0);
#endif
	wpa->store = gtk_tree_store_new(WSR_COUNT, G_TYPE_BOOLEAN,
			GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_UINT,
			G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	_wpa_start(wpa);
	gtk_widget_show_all(hbox);
	pango_font_description_free(bold);
	if(helper->type == PANEL_APPLET_TYPE_NOTIFICATION)
		wpa->widget = hbox;
	else
	{
		wpa->widget = gtk_button_new();
		gtk_button_set_relief(GTK_BUTTON(wpa->widget), GTK_RELIEF_NONE);
#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_widget_set_tooltip_text(wpa->widget,
				_("Wireless networking"));
#endif
		g_signal_connect_swapped(wpa->widget, "clicked", G_CALLBACK(
					_on_clicked), wpa);
		gtk_container_add(GTK_CONTAINER(wpa->widget), hbox);
	}
	wpa->pw_window = NULL;
	wpa->pw_id = 0;
	_wpa_set_status(wpa, FALSE, FALSE, _("Unavailable"));
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
	if(wpa->pw_window != NULL)
		gtk_widget_destroy(wpa->pw_window);
	gtk_widget_destroy(wpa->widget);
	object_delete(wpa);
}


/* accessors */
/* wpa_get_icon */
static GdkPixbuf * _wpa_get_icon(WPA * wpa, gint size, guint level,
		uint32_t flags)
{
	GdkPixbuf * ret;
	char const * name;
#if GTK_CHECK_VERSION(2, 14, 0)
	const int f = GTK_ICON_LOOKUP_USE_BUILTIN
		| GTK_ICON_LOOKUP_FORCE_SIZE;
#else
	const int f = GTK_ICON_LOOKUP_USE_BUILTIN;
#endif
	GdkPixbuf * pixbuf;
	char const * emblem = (flags & WSRF_WPA2) ? "security-high"
		: ((flags & WSRF_WPA) ? "security-medium"
				: ((flags & WSRF_WEP) ? "security-low" : NULL));

	/* FIXME check if the mapping is right (and use our own icons) */
	if(flags & WSRF_IBSS)
		name = "nm-adhoc";
	else if(level >= 100)
		name = "phone-signal-100";
	else if(level >= 75)
		name = "phone-signal-75";
	else if(level >= 50)
		name = "phone-signal-50";
	else if(level >= 25)
		name = "phone-signal-25";
	else
		name = "phone-signal-00";
	if((pixbuf = gtk_icon_theme_load_icon(wpa->icontheme, name, size, 0,
					NULL)) == NULL)
		return NULL;
	if((ret = gdk_pixbuf_copy(pixbuf)) == NULL)
		return pixbuf;
	g_object_unref(pixbuf);
	size = min(24, size / 2);
	if(emblem == NULL)
		return ret;
	pixbuf = gtk_icon_theme_load_icon(wpa->icontheme, emblem, size, f,
			NULL);
	if(pixbuf != NULL)
	{
		gdk_pixbuf_composite(pixbuf, ret, 0, 0, size, size, 0, 0, 1.0,
				1.0, GDK_INTERP_NEAREST, 255);
		g_object_unref(pixbuf);
	}
	return ret;
}


/* wpa_set_status */
static void _wpa_set_status(WPA * wpa, gboolean connected, gboolean associated,
		char const * network)
{
	char const * stock = connected
		? GTK_STOCK_CONNECT : GTK_STOCK_DISCONNECT;
	char const * icon;
	gint size = 16;
	GdkPixbuf * pixbuf = NULL;
	char buf[80];

	gtk_icon_size_lookup(wpa->helper->icon_size, &size, &size);
	if(connected == FALSE && network == NULL)
	{
		/* an error occurred */
		stock = GTK_STOCK_DIALOG_ERROR;
		icon = "network-error";
		network = _("Error");
	}
	else if(connected == FALSE && associated == FALSE)
		/* not connected to wpa_supplicant */
		icon = "network-offline";
	else if(connected == FALSE && associated == TRUE)
	{
		/* connected to an interface */
		icon = "network-offline";
		if(wpa->connected != connected && wpa->associated != associated
				&& network != NULL)
		{
			snprintf(buf, sizeof(buf),
					_("Connected to interface %s"),
					network);
			_wpa_notify(wpa, buf);
		}
		network = (network != NULL) ? network : _("Connecting...");
	}
	else if(associated == FALSE)
	{
		/* connected but not associated */
		icon = wpa->blink ? "network-idle"
			: "network-transmit-receive";
		wpa->blink = wpa->blink ? FALSE : TRUE;
		network = (network != NULL) ? network :  _("Scanning...");
	}
	else
	{
		/* connected and associated */
		icon = "network-idle";
		/* XXX use real values */
		pixbuf = _wpa_get_icon(wpa, size, wpa->level, wpa->flags);
		if(wpa->connected != connected && wpa->associated != associated
				&& network != NULL)
		{
			snprintf(buf, sizeof(buf), _("Connected to network %s"),
					network);
			_wpa_notify(wpa, buf);
		}
	}
#if GTK_CHECK_VERSION(2, 6, 0)
	if(pixbuf != NULL)
	{
		gtk_image_set_from_pixbuf(GTK_IMAGE(wpa->image), pixbuf);
		g_object_unref(pixbuf);
	}
	else
		gtk_image_set_from_icon_name(GTK_IMAGE(wpa->image), icon,
				wpa->helper->icon_size);
#else
	if(pixbuf != NULL || (pixbuf = gtk_icon_theme_load_icon(wpa->icontheme,
					icon, size, 0, NULL)) != NULL)
	{
		gtk_image_set_from_pixbuf(GTK_IMAGE(wpa->image), pixbuf);
		g_object_unref(pixbuf);
	}
	else
		gtk_image_set_from_stock(GTK_IMAGE(wpa->image),
				(connected && associated)
				? GTK_STOCK_CONNECT : GTK_STOCK_DISCONNECT,
				wpa->helper->icon_size);
#endif
#ifndef EMBEDDED
	gtk_label_set_text(GTK_LABEL(wpa->label), network);
#endif
	wpa->connected = connected;
	wpa->associated = associated;
}


/* useful */
/* wpa_error */
static int _wpa_error(WPA * wpa, char const * message, int ret)
{
	_wpa_set_status(wpa, FALSE, FALSE, NULL);
	return wpa->helper->error(NULL, message, ret);
}


/* wpa_ask_password */
static void _ask_password_window(WPA * wpa);
/* callbacks */
static gboolean _ask_password_on_closex(gpointer data);
static void _ask_password_on_response(GtkWidget * widget, gint response,
		gpointer data);

static void _wpa_ask_password(WPA * wpa, WPANetwork * network)
{
	if(wpa->pw_window == NULL)
		_ask_password_window(wpa);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(
				wpa->pw_window),
#else
	/* FIXME wrong */
	gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(wpa->pw_window),
#endif
			_("The network \"%s\" is protected by a key."),
			network->name);
	/* reset the text if the network has changed */
	if(wpa->pw_id != network->id)
		gtk_entry_set_text(GTK_ENTRY(wpa->pw_entry), "");
	wpa->pw_id = network->id;
	gtk_window_present(GTK_WINDOW(wpa->pw_window));
}

static void _ask_password_window(WPA * wpa)
{
	GtkWidget * dialog;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * label;

	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_OK_CANCEL, "%s", _("Password required"));
	wpa->pw_window = dialog;
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
#if GTK_CHECK_VERSION(2, 10, 0)
	wpa->pw_entry = gtk_image_new_from_icon_name("dialog-password",
			GTK_ICON_SIZE_DIALOG);
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog), wpa->pw_entry);
	gtk_widget_show(wpa->pw_entry);
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "dialog-password");
#endif
	gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Wireless key"));
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif
	hbox = gtk_hbox_new(FALSE, 4);
	label = gtk_label_new(_("Key: "));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	wpa->pw_entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(wpa->pw_entry), TRUE);
	gtk_entry_set_visibility(GTK_ENTRY(wpa->pw_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), wpa->pw_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show_all(hbox);
	g_signal_connect_swapped(wpa->pw_window, "delete-event", G_CALLBACK(
				_ask_password_on_closex), wpa);
	g_signal_connect(wpa->pw_window, "response", G_CALLBACK(
				_ask_password_on_response), wpa);
}

static gboolean _ask_password_on_closex(gpointer data)
{
	WPA * wpa = data;

	gtk_widget_hide(wpa->pw_window);
	return TRUE;
}

static void _ask_password_on_response(GtkWidget * widget, gint response,
		gpointer data)
{
	WPA * wpa = data;
	char const * password;
	size_t i;
	WPAChannel * channel = &wpa->channel[0];

	if(response != GTK_RESPONSE_OK
			|| (password = gtk_entry_get_text(GTK_ENTRY(
						wpa->pw_entry))) == NULL)
		/* enable every network again */
		for(i = 0; i < wpa->networks_cnt; i++)
			_wpa_queue(wpa, &wpa->channel[0], WC_ENABLE_NETWORK, i);
	else
	{
		/* FIXME the network may have changed in the meantime */
		_wpa_queue(wpa, channel, WC_SET_PASSWORD, wpa->pw_id, password);
		if(wpa->autosave)
			_wpa_queue(wpa, channel, WC_SAVE_CONFIGURATION);
	}
	gtk_widget_hide(wpa->pw_window);
}


/* wpa_connect */
static void _wpa_connect(WPA * wpa, char const * ssid, uint32_t flags)
{
	WPAChannel * channel = &wpa->channel[0];
	size_t i;

	/* check if the network is already in the list */
	for(i = 0; i < wpa->networks_cnt; i++)
		if(strcmp(wpa->networks[i].name, ssid) == 0)
			break;
	if(i < wpa->networks_cnt)
		/* select this network directly */
		_wpa_connect_network(wpa, &wpa->networks[i]);
	else
		/* add (and then select) this network */
		_wpa_queue(wpa, channel, WC_ADD_NETWORK, ssid, flags, TRUE);
}


/* wpa_connect_network */
static void _wpa_connect_network(WPA * wpa, WPANetwork * network)
{
	WPAChannel * channel = &wpa->channel[0];

	/* select this network */
	_wpa_queue(wpa, channel, WC_SELECT_NETWORK, network->id);
	_wpa_queue(wpa, channel, WC_LIST_NETWORKS);
}


/* wpa_disconnect */
static void _wpa_disconnect(WPA * wpa)
{
	WPAChannel * channel = &wpa->channel[0];
	size_t i;

	/* enable every network again */
	for(i = 0; i < wpa->networks_cnt; i++)
		_wpa_queue(wpa, channel, WC_ENABLE_NETWORK, i);
	_wpa_queue(wpa, channel, WC_LIST_NETWORKS);
	_wpa_rescan(wpa);
}


/* wpa_notify */
static void _wpa_notify(WPA * wpa, char const * message)
{
	char const * p;
	char * argv[] = { "panel-message", "-I", "--", NULL, NULL };
	GError * error = NULL;

	/* check if notifications are enabled */
	if((p = wpa->helper->config_get(wpa->helper->panel, "wpa_supplicant",
					"notify")) == NULL
			|| strtol(p, NULL, 10) != 1)
		return;
	if((argv[3] = strdup(message)) == NULL)
	{
		wpa->helper->error(NULL, strerror(errno), 1);
		return;
	}
	if(g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
				NULL, &error) == FALSE)
	{
		wpa->helper->error(NULL, error->message, 1);
		g_error_free(error);
	}
	free(argv[3]);
}


/* wpa_queue */
static int _wpa_queue(WPA * wpa, WPAChannel * channel, WPACommand command, ...)
{
	va_list ap;
	unsigned int u;
	char * cmd = NULL;
	WPAEntry * p;
	char const * ssid = NULL;
	uint32_t flags = 0;
	gboolean connect = FALSE;
	char const * s;
	char const * t;
	char const * format;

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
			flags = va_arg(ap, uint32_t);
			connect = va_arg(ap, gboolean);
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
			format = (t != NULL) ? "SET_NETWORK %u %s \"%s\""
				: "SET_NETWORK %u %s NONE";
			cmd = g_strdup_printf(format, u, s, t);
			break;
		case WC_SET_PASSWORD:
			/* FIXME really uses SET_NETWORK (and may not be psk) */
			u = va_arg(ap, unsigned int);
			s = va_arg(ap, char const *);
			cmd = g_strdup_printf("SET_NETWORK %u psk \"%s\"", u,
					s);
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
	p->flags = flags;
	p->connect = connect;
	if(channel->queue_cnt++ == 0)
		channel->wr_source = g_io_add_watch(channel->channel, G_IO_OUT,
				_on_watch_can_write, wpa);
	return 0;
}


/* wpa_rescan */
static void _wpa_rescan(WPA * wpa)
{
	WPAChannel * channel = &wpa->channel[0];

	_wpa_queue(wpa, channel, WC_SCAN);
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
	wpa->connected = FALSE;
	wpa->associated = FALSE;
	/* reconnect to the daemon */
	wpa->source = g_idle_add(_start_timeout, wpa);
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
	/* XXX will be done for every channel */
	_wpa_set_status(wpa, FALSE, TRUE, interface);
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
	gtk_tree_store_clear(wpa->store);
	for(i = 0; i < wpa->networks_cnt; i++)
		free(wpa->networks[i].name);
	free(wpa->networks);
	wpa->networks = NULL;
	wpa->networks_cnt = 0;
	wpa->networks_cur = -1;
	wpa->connected = FALSE;
	wpa->associated = FALSE;
	/* report the status */
	_wpa_set_status(wpa, FALSE, FALSE, _("Unavailable"));
	if(wpa->pw_window != NULL)
		gtk_widget_hide(wpa->pw_window);
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


/* wpa_tooltip */
static void _wpa_tooltip(char * buf, size_t buf_cnt, unsigned int frequency,
		unsigned int level, uint32_t flags)
{
	char const * security = (flags & WSRF_WPA2) ? "WPA2"
		: ((flags & WSRF_WPA) ? "WPA"
				: ((flags & WSRF_WEP) ? "WEP" : NULL));

	/* FIXME mention the channel instead of the frequency */
	snprintf(buf, buf_cnt, _("Frequency: %u\nLevel: %u%s%s"), frequency,
			level, (security != NULL) ? _("\nSecurity: ") : "",
			(security != NULL) ? security : "");
}


/* callbacks */
/* on_clicked */
static void _clicked_available(WPA * wpa, GtkWidget * menu);
static void _clicked_network_list(WPA * wpa, GtkWidget * menu);
static void _clicked_network_view(WPA * wpa, GtkWidget * menu);
static void _clicked_position_menu(GtkMenu * menu, gint * x, gint * y,
		gboolean * push_in, gpointer data);
static void _clicked_unavailable(GtkWidget * menu);
/* callbacks */
static void _clicked_on_disconnect(gpointer data);
static void _clicked_on_network_activated(GtkWidget * widget, gpointer data);
static void _clicked_on_network_toggled(GtkWidget * widget, gpointer data);
static void _clicked_on_reassociate(gpointer data);
static void _clicked_on_rescan(gpointer data);
static void _clicked_on_save_configuration(gpointer data);

static void _on_clicked(gpointer data)
{
	WPA * wpa = data;
	GtkWidget * menu;

	menu = gtk_menu_new();
	if(wpa->channel[0].fd < 0)
		_clicked_unavailable(menu);
	else
		_clicked_available(wpa, menu);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, _clicked_position_menu,
			wpa, 0, gtk_get_current_event_time());
}

static void _clicked_available(WPA * wpa, GtkWidget * menu)
{
	GtkWidget * menuitem;
	GtkWidget * image;

	/* FIXME summarize the status instead */
	_clicked_network_list(wpa, menu);
	if(wpa->networks_cur >= 0)
	{
		/* reassociate */
		menuitem = gtk_image_menu_item_new_with_label(_("Reassociate"));
#if GTK_CHECK_VERSION(2, 12, 0)
		image = gtk_image_new_from_stock(GTK_STOCK_DISCARD,
				GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
				image);
#endif
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_clicked_on_reassociate), wpa);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		/* disconnect */
		menuitem = gtk_image_menu_item_new_from_stock(
				GTK_STOCK_DISCONNECT, NULL);
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_clicked_on_disconnect), wpa);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
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
	guint frequency;
	guint level;
	guint flags;
	gchar * dssid;
#if GTK_CHECK_VERSION(2, 12, 0)
	gchar * tooltip;
#endif
	GtkTreePath * path;
	GtkTreeRowReference * row;

	if((valid = gtk_tree_model_get_iter_first(model, &iter)) == FALSE)
		return;
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	for(; valid == TRUE; valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_model_get(model, &iter, WSR_ICON, &pixbuf,
				WSR_FREQUENCY, &frequency, WSR_LEVEL, &level,
				WSR_FLAGS, &flags, WSR_SSID_DISPLAY, &dssid,
#if GTK_CHECK_VERSION(2, 12, 0)
				WSR_TOOLTIP, &tooltip,
#endif
				-1);
		menuitem = gtk_image_menu_item_new_with_label(dssid);
#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_widget_set_tooltip_text(menuitem, tooltip);
		g_free(tooltip);
#endif
		path = gtk_tree_model_get_path(model, &iter);
		row = gtk_tree_row_reference_new(model, path);
		gtk_tree_path_free(path);
		g_object_set_data(G_OBJECT(menuitem), "row", row);
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
				image);
		g_signal_connect(menuitem, "activate", G_CALLBACK(
					_clicked_on_network_activated), wpa);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		g_free(dssid);
#if 0 /* XXX memory leak (for g_object_set_data() above) */
		gtk_tree_row_reference_free(treeref);
#endif
	}
}

static void _clicked_unavailable(GtkWidget * menu)
{
	GtkWidget * menuitem;

	menuitem = gtk_image_menu_item_new_with_label(
			_("Wireless is disabled"));
	gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

/* callbacks */
static void _clicked_on_disconnect(gpointer data)
{
	WPA * wpa = data;

	_wpa_disconnect(wpa);
}

static void _clicked_on_network_activated(GtkWidget * widget, gpointer data)
{
	WPA * wpa = data;
	GtkTreeRowReference * row;
	GtkTreeModel * model = GTK_TREE_MODEL(wpa->store);
	GtkTreePath * path;
	GtkTreeIter iter;
	gchar * ssid;
	guint f;

	if((row = g_object_get_data(G_OBJECT(widget), "row")) == NULL)
		/* FIXME implement */
		return;
	if((path = gtk_tree_row_reference_get_path(row)) == NULL
			|| gtk_tree_model_get_iter(model, &iter, path) != TRUE)
	{
		gtk_tree_row_reference_free(row);
		return;
	}
	gtk_tree_model_get(model, &iter, WSR_SSID, &ssid, WSR_FLAGS, &f, -1);
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() \"%s\"\n", __func__, ssid);
#endif
	_wpa_connect(wpa, ssid, f);
	g_free(ssid);
#if 1 /* XXX partly remediate memory leak (see above) */
	gtk_tree_row_reference_free(row);
#endif
}

static void _clicked_on_network_toggled(GtkWidget * widget, gpointer data)
{
	WPA * wpa = data;
	WPANetwork * network;

	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == FALSE)
		return;
	if((network = g_object_get_data(G_OBJECT(widget), "network")) != NULL)
		_wpa_connect_network(wpa, network);
	else
		_wpa_disconnect(wpa);
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

	_wpa_rescan(wpa);
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
		size_t cnt, char const * ssid, uint32_t flags,
		gboolean connect);
static void _read_list_networks(WPA * wpa, char const * buf, size_t cnt);
static void _read_scan_results(WPA * wpa, char const * buf, size_t cnt);
static void _read_scan_results_cleanup(WPA * wpa, GtkTreeModel * model);
static uint32_t _read_scan_results_flags(WPA * wpa, char const * flags);
static void _read_scan_results_iter(WPA * wpa, GtkTreeIter * iter,
		char const * bssid);
static void _read_scan_results_iter_ssid(WPA * wpa, GtkTreeIter * iter,
		char const * bssid, char const * ssid, unsigned int level,
		unsigned int frequency, unsigned int flags, GdkPixbuf * pixbuf,
		char const * tooltip);
static void _read_scan_results_reset(WPA * wpa, GtkTreeModel * model);
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
	char const * p;

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
				p = _("Unknown error");
				if(entry->command == WC_SAVE_CONFIGURATION)
					p = _("Could not save the"
							" configuration");
				wpa->helper->error(NULL, p, 0);
				break;
			}
			if(entry->command == WC_ADD_NETWORK)
				_read_add_network(wpa, channel, buf, cnt,
						entry->ssid, entry->flags,
						entry->connect);
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
		size_t cnt, char const * ssid, uint32_t flags, gboolean connect)
{
	unsigned int id;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", 0x%08x)\n", __func__, ssid, flags);
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
	if((flags & (WSRF_WPA | WSRF_WPA2)) == 0)
		/* required to be able to connect to open or WEP networks */
		_wpa_queue(wpa, channel, WC_SET_NETWORK, id, "key_mgmt", NULL);
	if(wpa->autosave)
		_wpa_queue(wpa, channel, WC_SAVE_CONFIGURATION);
	if(connect)
	{
		_wpa_queue(wpa, channel, WC_SELECT_NETWORK, id);
		_wpa_queue(wpa, channel, WC_LIST_NETWORKS);
	}
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
				/* XXX may be associated already */
				_wpa_set_status(wpa, TRUE, FALSE, ssid);
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
	gint size = 16;
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
	uint32_t f;
	char ssid[80];
	char tooltip[80];
	GtkTreeIter iter;

	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &size, &size);
	_read_scan_results_reset(wpa, model);
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
			f = _read_scan_results_flags(wpa, flags);
			pixbuf = _wpa_get_icon(wpa, size, level, f);
			_wpa_tooltip(tooltip, sizeof(tooltip), frequency, level,
					f);
			if(res == 5)
				_read_scan_results_iter_ssid(wpa, &iter, bssid,
						ssid, level, frequency, f,
						pixbuf, tooltip);
			else
				_read_scan_results_iter(wpa, &iter, bssid);
			gtk_tree_store_set(wpa->store, &iter, WSR_UPDATED, TRUE,
					WSR_ICON, pixbuf, WSR_BSSID, bssid,
					WSR_FREQUENCY, frequency,
					WSR_LEVEL, level, WSR_FLAGS, f,
					WSR_TOOLTIP, tooltip, -1);
			if(res == 5)
				gtk_tree_store_set(wpa->store, &iter,
						WSR_SSID, ssid,
						WSR_SSID_DISPLAY, ssid, -1);
			else
			{
				snprintf(tooltip, sizeof(tooltip),
						_("Hidden (%s)"), bssid);
				gtk_tree_store_set(wpa->store, &iter,
						WSR_SSID_DISPLAY, tooltip, -1);
			}
		}
	}
	free(p);
	_read_scan_results_cleanup(wpa, model);
}

static void _read_scan_results_cleanup(WPA * wpa, GtkTreeModel * model)
{
	GtkTreeIter iter;
	GtkTreeIter child;
	gboolean valid;

	/* remove the outdated entries */
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;)
	{
		gtk_tree_model_get(model, &iter, WSR_UPDATED, &valid, -1);
		if(valid == FALSE)
		{
			valid = gtk_tree_store_remove(wpa->store, &iter);
			continue;
		}
		for(valid = gtk_tree_model_iter_children(model, &child, &iter);
				valid == TRUE;)
		{
			gtk_tree_model_get(model, &child, WSR_UPDATED, &valid,
					-1);
			if(valid == FALSE)
				valid = gtk_tree_store_remove(wpa->store,
						&child);
			else
				valid = gtk_tree_model_iter_next(model, &child);
		}
		valid = gtk_tree_model_iter_next(model, &iter);
	}
}

static uint32_t _read_scan_results_flags(WPA * wpa, char const * flags)
{
	uint32_t ret = 0;
	char const * p;
	char const wep[] = "WEP";
	char const wpa1[] = "WPA-";
	char const wpa2[] = "WPA2-";
	char const psk[] = "PSK";
	char const eap[] = "EAP";
	char const ccmp[] = "CCMP";
	char const tkipccmp[] = "TKIP+CCMP";
	char const tkip[] = "TKIP";
	char const preauth[] = "preauth";
	char const ess[] = "ESS";
	char const ibss[] = "IBSS";

	for(p = flags; *p != '\0';)
	{
		if(*(p++) != '[')
			continue;
		/* FIXME parse more consistently */
		if(strncmp(wep, p, sizeof(wep) - 1) == 0)
		{
			ret |= WSRF_WEP;
			p += sizeof(wep) - 1;
		}
		else if(strncmp(wpa1, p, sizeof(wpa1) - 1) == 0)
		{
			ret |= WSRF_WPA;
			p += sizeof(wpa1) - 1;
		}
		else if(strncmp(wpa2, p, sizeof(wpa2) - 1) == 0)
		{
			ret |= WSRF_WPA2;
			p += sizeof(wpa2) - 1;
		}
		else if(strncmp(ess, p, sizeof(ess) - 1) == 0)
		{
			ret |= WSRF_ESS;
			p += sizeof(ess) - 1;
		}
		else if(strncmp(ibss, p, sizeof(ibss) - 1) == 0)
		{
			ret |= WSRF_IBSS;
			p += sizeof(ibss) - 1;
		}
		else
			continue;
		if(strncmp(psk, p, sizeof(psk) - 1) == 0)
		{
			ret |= WSRF_PSK;
			p += sizeof(psk) - 1;
		}
		else if(strncmp(eap, p, sizeof(eap) - 1) == 0)
		{
			ret |= WSRF_EAP;
			p += sizeof(eap) - 1;
		}
		if(*p == '-')
			p++;
		if(strncmp(ccmp, p, sizeof(ccmp) - 1) == 0)
		{
			ret |= WSRF_CCMP;
			p += sizeof(ccmp) - 1;
		}
		else if(strncmp(tkipccmp, p, sizeof(tkipccmp) - 1) == 0)
		{
			ret |= WSRF_TKIP | WSRF_CCMP;
			p += sizeof(tkipccmp) - 1;
		}
		else if(strncmp(tkip, p, sizeof(tkip) - 1) == 0)
		{
			ret |= WSRF_TKIP;
			p += sizeof(tkip) - 1;
		}
		else
			continue;
		if(*p == '-')
			p++;
		if(strncmp(preauth, p, sizeof(preauth) - 1) == 0)
		{
			ret |= WSRF_PREAUTH;
			p += sizeof(preauth) - 1;
		}
		else
			continue;
		/* FIXME implement more */
		for(p++; *p != '\0' && *p != ']'; p++);
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() => 0x%x\n", __func__, ret);
#endif
	return ret;
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
	gtk_tree_store_append(wpa->store, iter, NULL);
}

static void _read_scan_results_iter_ssid(WPA * wpa, GtkTreeIter * iter,
		char const * bssid, char const * ssid, unsigned int level,
		unsigned int frequency, unsigned int flags, GdkPixbuf * pixbuf,
		char const * tooltip)
{
	GtkTreeModel * model = GTK_TREE_MODEL(wpa->store);
	gboolean valid;
	gchar * s;
	unsigned int f;
	unsigned int l = 0;
	int res;
	GtkTreeIter parent;

	/* look for the network in the list */
	for(valid = gtk_tree_model_get_iter_first(model, iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, WSR_SSID, &s, WSR_LEVEL, &l,
				WSR_FLAGS, &f, -1);
		res = (s != NULL && strcmp(s, ssid) == 0 && f == flags)
			? 0 : -1;
		g_free(s);
		if(res == 0)
			break;
	}
	if(valid != TRUE)
	{
		gtk_tree_store_append(wpa->store, iter, NULL);
		/* FIXME determine the true value for WSR_ENABLED */
		gtk_tree_store_set(wpa->store, iter, WSR_UPDATED, TRUE,
				WSR_LEVEL, level, WSR_FREQUENCY, frequency,
				WSR_FLAGS, flags, WSR_SSID, ssid,
				WSR_SSID_DISPLAY, ssid, WSR_ICON, pixbuf,
				WSR_TOOLTIP, tooltip, WSR_ENABLED, FALSE,
				WSR_CAN_ENABLE, TRUE, -1);
	}
	else
		gtk_tree_store_set(wpa->store, iter, WSR_UPDATED, TRUE,
				/* refresh the level and icon if relevant */
				(level > l) ? WSR_LEVEL : -1, level,
				WSR_FREQUENCY, frequency, WSR_ICON, pixbuf,
				WSR_TOOLTIP, tooltip, WSR_ENABLED, FALSE,
				WSR_CAN_ENABLE, FALSE, -1);
	/* look for the BSSID in the network */
	parent = *iter;
	for(valid = gtk_tree_model_iter_children(model, iter, &parent);
			valid == TRUE;
			valid = gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, WSR_BSSID, &s, -1);
		res = (s != NULL && strcmp(s, bssid) == 0) ? 0 : -1;
		g_free(s);
		if(res == 0)
			break;
	}
	if(valid != TRUE)
		gtk_tree_store_append(wpa->store, iter, &parent);
}

static void _read_scan_results_reset(WPA * wpa, GtkTreeModel * model)
{
	GtkTreeIter iter;
	GtkTreeIter child;
	gboolean valid;

	/* mark every entry as obsolete */
	for(valid = gtk_tree_model_get_iter_first(model, &iter); valid == TRUE;
			valid = gtk_tree_model_iter_next(model, &iter))
	{
		gtk_tree_store_set(wpa->store, &iter, WSR_UPDATED, FALSE, -1);
		for(valid = gtk_tree_model_iter_children(model, &child, &iter);
				valid == TRUE;
				valid = gtk_tree_model_iter_next(model, &child))
			gtk_tree_store_set(wpa->store, &child,
					WSR_UPDATED, FALSE, -1);
	}
}

static void _read_status(WPA * wpa, char const * buf, size_t cnt)
{
	gboolean associated = FALSE;
	char * network = NULL;
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
			if(strcmp(value, "COMPLETED") == 0)
				associated = TRUE;
			else if(strcmp(value, "4WAY_HANDSHAKE") == 0)
				associated = FALSE;
			else if(strcmp(value, "SCANNING") == 0)
				associated = FALSE;
		}
#ifndef EMBEDDED
		if(strcmp(variable, "ssid") == 0)
			/* XXX may fail */
			network = strdup(value);
#endif
	}
	free(p);
	/* reflect the status */
	if(associated == TRUE)
	{
		if(wpa->pw_window != NULL)
			gtk_widget_hide(wpa->pw_window);
	}
	_wpa_set_status(wpa, TRUE, associated, network);
	free(network);
}

static void _read_unsolicited(WPA * wpa, char const * buf, size_t cnt)
{
	char const scan_results[] = "CTRL-EVENT-SCAN-RESULTS";
	char const wpa_handshake[] = "WPA: 4-Way Handshake failed"
		" - pre-shared key may be incorrect";
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
		/* XXX hackish, blame wpa_supplicant(8) */
		else if(strncmp(&p[3], wpa_handshake, sizeof(wpa_handshake) - 1)
				== 0 && wpa->networks_cur >= 0)
			/* FIXME does not work if networks_cur is not set */
			_wpa_ask_password(wpa,
					&wpa->networks[wpa->networks_cur]);
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
