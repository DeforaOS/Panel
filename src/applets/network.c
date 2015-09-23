/* $Id$ */
/* Copyright (c) 2014-2015 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <stdlib.h>
#ifdef DEBUG
# include <stdio.h>
#endif
#include <string.h>
#include <errno.h>
#ifdef __NetBSD__
# include <ifaddrs.h>
#endif
#include <libintl.h>
#include <net/if.h>
#include <System.h>
#include "Panel/applet.h"
#define _(string) gettext(string)


/* Network */
/* private */
/* types */
typedef struct _NetworkInterface
{
	String * name;
	unsigned int flags;
	unsigned long ipackets;
	unsigned long opackets;
	unsigned long ibytes;
	unsigned long obytes;
	GtkWidget * widget;
	gboolean updated;
} NetworkInterface;

typedef struct _PanelApplet
{
	PanelAppletHelper * helper;
	guint source;
	int fd;
	NetworkInterface * interfaces;
	size_t interfaces_cnt;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * pr_box;
#ifdef IFF_LOOPBACK
	GtkWidget * pr_loopback;
#endif
#ifdef IFF_UP
	GtkWidget * pr_showdown;
#endif
} Network;


/* prototypes */
/* plug-in */
static Network * _network_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _network_destroy(Network * network);

static GtkWidget * _network_settings(Network * network, gboolean apply,
		gboolean reset);

/* useful */
static void _network_refresh(Network * network);

/* callbacks */
static gboolean _network_on_timeout(gpointer data);

/* NetworkInterface */
static int _networkinterface_init(NetworkInterface * ni, char const * name,
		unsigned int flags);
static void _networkinterface_destroy(NetworkInterface * ni);
static void _networkinterface_update(NetworkInterface * ni, char const * icon,
		GtkIconSize iconsize, gboolean active, unsigned int flags,
		gboolean updated, char const * tooltip);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Network",
	"network-idle",
	NULL,
	_network_init,
	_network_destroy,
	_network_settings,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* network_init */
static Network * _network_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	const unsigned int timeout = 500;
	Network * network;
	GtkOrientation orientation;

	if((network = object_new(sizeof(*network))) == NULL)
		return NULL;
	network->helper = helper;
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	network->widget = gtk_box_new(orientation, 0);
#else
	network->widget = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? gtk_hbox_new(TRUE, 0) : gtk_vbox_new(TRUE, 0);
#endif
	network->pr_box = NULL;
	gtk_widget_show(network->widget);
	network->source = g_timeout_add(timeout, _network_on_timeout, network);
	if((network->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		error_set("%s: %s: %s", applet.name, "socket", strerror(errno));
		network->helper->error(NULL, error_get(), 1);
	}
	network->interfaces = NULL;
	network->interfaces_cnt = 0;
	*widget = network->widget;
	_network_refresh(network);
	return network;
}


/* network_destroy */
static void _network_destroy(Network * network)
{
	size_t i;

	for(i = 0; i < network->interfaces_cnt; i++)
		_networkinterface_destroy(&network->interfaces[i]);
	free(network->interfaces);
	if(network->fd >= 0)
		close(network->fd);
	if(network->source != 0)
		g_source_remove(network->source);
	gtk_widget_destroy(network->widget);
	object_delete(network);
}


/* useful */
/* network_refresh */
static void _refresh_interface(Network * network, char const * name,
		unsigned int flags);
static int _refresh_interface_add(Network * network, char const * name,
		unsigned int flags);
static void _refresh_interface_flags(Network * network, NetworkInterface * ni,
		unsigned int flags);
static void _refresh_purge(Network * network);
static void _refresh_reset(Network * network);

static void _network_refresh(Network * network)
{
	char const * p;
#ifdef __NetBSD__
	struct ifaddrs * ifa;
#endif

	if((p = network->helper->config_get(network->helper->panel, "network",
					"interface")) != NULL)
	{
		/* FIXME obtain some flags if possible */
#ifdef IFF_UP
		_refresh_interface(network, p, IFF_UP);
#else
		_refresh_interface(network, p, 0);
#endif
		return;
	}
#ifdef __NetBSD__
	if(getifaddrs(&ifa) != 0)
		return;
	_refresh_reset(network);
	for(; ifa != NULL; ifa = ifa->ifa_next)
	{
		_refresh_interface(network, ifa->ifa_name, ifa->ifa_flags);
		/* XXX avoid repeated entries */
		for(; ifa->ifa_next != NULL && strcmp(ifa->ifa_name,
					ifa->ifa_next->ifa_name) == 0;
				ifa = ifa->ifa_next);
	}
	/* FIXME also remove/disable the interfaces not listed */
	freeifaddrs(ifa);
	_refresh_purge(network);
#endif
}

static void _refresh_interface(Network * network, char const * name,
		unsigned int flags)
{
	size_t i;
	int res;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, name);
#endif
	for(i = 0; i < network->interfaces_cnt; i++)
		if(strcmp(network->interfaces[i].name, name) == 0)
			break;
	/* FIXME really implement */
	if(i == network->interfaces_cnt)
		/* XXX assumes network->interfaces[i] will be correct */
		if((res = _refresh_interface_add(network, name, flags)) != 0)
		{
			if(res < 0)
				network->helper->error(NULL, error_get(), 1);
			return;
		}
	_refresh_interface_flags(network, &network->interfaces[i], flags);
}

static int _refresh_interface_add(Network * network, char const * name,
		unsigned int flags)
{
	NetworkInterface * p;
#if defined(IFF_LOOPBACK) || defined(IFF_UP)
	char const * q;
#endif

#ifdef IFF_LOOPBACK
	if(flags & IFF_LOOPBACK)
	{
		q = network->helper->config_get(network->helper->panel,
				"network", "loopback");
		if(q == NULL || strtol(q, NULL, 10) == 0)
			/* ignore the interface */
			return 1;
	}
#endif
#ifdef IFF_UP
	if((flags & IFF_UP) == 0)
	{
		q = network->helper->config_get(network->helper->panel,
				"network", "showdown");
		if(q != NULL && strtol(q, NULL, 10) == 0)
			/* ignore the interface */
			return 1;
	}
#endif
	if((p = realloc(network->interfaces, sizeof(*p)
					* (network->interfaces_cnt + 1)))
			== NULL)
		return -error_set_code(1, "%s: %s", applet.name,
				strerror(errno));
	network->interfaces = p;
	p = &network->interfaces[network->interfaces_cnt];
	if(_networkinterface_init(p, name, flags) != 0)
		return -1;
	_refresh_interface_flags(network, p, flags);
	gtk_box_pack_start(GTK_BOX(network->widget), p->widget, FALSE, TRUE, 0);
	gtk_widget_show(p->widget);
	network->interfaces_cnt++;
	return 0;
}

static void _refresh_interface_delete(Network * network, size_t i)
{
	NetworkInterface * ni = &network->interfaces[i];

	_networkinterface_destroy(ni);
	network->interfaces_cnt--;
	memmove(&network->interfaces[i], &network->interfaces[i + 1],
			sizeof(*ni) * (network->interfaces_cnt - i));
	/* XXX realloc() network->interfaces to free some memory */
}

static void _refresh_interface_flags(Network * network, NetworkInterface * ni,
		unsigned int flags)
{
	gboolean active = TRUE;
	char const * icon = "network-offline";
	GtkIconSize iconsize;
#ifdef SIOCGIFDATA
	struct ifdatareq ifdr;
# if GTK_CHECK_VERSION(2, 12, 0)
	unsigned long ibytes;
	unsigned long obytes;
# endif
#endif
	char tooltip[128] = "";

#ifdef IFF_UP
	if((flags & IFF_UP) != IFF_UP)
		active = FALSE;
	else
#endif
	{
#ifdef SIOCGIFDATA
		/* XXX ignore errors */
		memset(&ifdr, 0, sizeof(ifdr));
		strncpy(ifdr.ifdr_name, ni->name, sizeof(ifdr.ifdr_name));
		if(ioctl(network->fd, SIOCGIFDATA, &ifdr) == -1)
			network->helper->error(NULL, "SIOCGIFDATA", 1);
		else
		{
			if(ifdr.ifdr_data.ifi_ipackets > ni->ipackets)
				icon = (ifdr.ifdr_data.ifi_opackets
						> ni->opackets)
					? "network-transmit-receive"
					: "network-receive";
			else if(ifdr.ifdr_data.ifi_opackets > ni->opackets)
				icon = "network-transmit";
# ifdef LINK_STATE_DOWN
			else if(ifdr.ifdr_data.ifi_link_state
					== LINK_STATE_DOWN)
				icon = "network-offline";
# endif
			else
				icon = "network-idle";
# if GTK_CHECK_VERSION(2, 12, 0)
			ibytes = (ifdr.ifdr_data.ifi_ibytes >= ni->ibytes)
				? ifdr.ifdr_data.ifi_ibytes - ni->ibytes
				: ULONG_MAX - ni->ibytes
				+ ifdr.ifdr_data.ifi_ibytes;
			obytes = (ifdr.ifdr_data.ifi_obytes >= ni->obytes)
				? ifdr.ifdr_data.ifi_obytes - ni->obytes
				: ULONG_MAX - ni->obytes
				+ ifdr.ifdr_data.ifi_obytes;
			snprintf(tooltip, sizeof(tooltip),
					_("%s\nIn: %lu kB/s\nOut: %lu kB/s"),
					ni->name, ibytes / 512, obytes / 512);
# endif
			ni->ipackets = ifdr.ifdr_data.ifi_ipackets;
			ni->opackets = ifdr.ifdr_data.ifi_opackets;
			ni->ibytes = ifdr.ifdr_data.ifi_ibytes;
			ni->obytes = ifdr.ifdr_data.ifi_obytes;
		}
#endif
	}
	iconsize = panel_window_get_icon_size(network->helper->window);
	_networkinterface_update(ni, icon, iconsize, active, flags, TRUE,
			(tooltip[0] != '\0') ? tooltip : NULL);
}

static void _refresh_purge(Network * network)
{
	size_t i;

	for(i = 0; i < network->interfaces_cnt;)
		if(network->interfaces[i].updated == FALSE)
			_refresh_interface_delete(network, i);
		else
			i++;
}

static void _refresh_reset(Network * network)
{
	size_t i;

	for(i = 0; i < network->interfaces_cnt; i++)
		network->interfaces[i].updated = FALSE;
}


/* network_settings */
static void _settings_apply(Network * network, PanelAppletHelper * helper);
static void _settings_reset(Network * network, PanelAppletHelper * helper);

static GtkWidget * _network_settings(Network * network, gboolean apply,
		gboolean reset)
{
	PanelAppletHelper * helper = network->helper;

	if(network->pr_box == NULL)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		network->pr_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
		network->pr_box = gtk_vbox_new(TRUE, 4);
#endif
#ifdef IFF_LOOPBACK
		network->pr_loopback = gtk_check_button_new_with_label(
				_("Show local interfaces"));
		gtk_box_pack_start(GTK_BOX(network->pr_box),
				network->pr_loopback, FALSE, TRUE, 0);
#endif
#ifdef IFF_UP
		network->pr_showdown = gtk_check_button_new_with_label(
				_("Show the interfaces disabled"));
		gtk_box_pack_start(GTK_BOX(network->pr_box),
				network->pr_showdown, FALSE, TRUE, 0);
#endif
		gtk_widget_show_all(network->pr_box);
		reset = TRUE;
	}
	if(reset == TRUE)
		_settings_reset(network, helper);
	if(apply == TRUE)
		_settings_apply(network, helper);
	return network->pr_box;
}

static void _settings_apply(Network * network, PanelAppletHelper * helper)
{
#if defined(IFF_LOOPBACK) || defined(IFF_UP)
	gboolean active;
#endif

#ifdef IFF_LOOPBACK
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				network->pr_loopback));
	helper->config_set(helper->panel, "network", "loopback",
			active ? "1" : "0");
#endif
#ifdef IFF_UP
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				network->pr_showdown));
	helper->config_set(helper->panel, "network", "showdown",
			active ? "1" : "0");
#endif
	_network_refresh(network);
}

static void _settings_reset(Network * network, PanelAppletHelper * helper)
{
#ifndef EMBEDDED
# ifdef IFF_LOOPBACK
	gboolean loopback = TRUE;
# endif
# ifdef IFF_UP
	gboolean showdown = TRUE;
# endif
#else
# ifdef IFF_LOOPBACK
	gboolean loopback = FALSE;
# endif
# ifdef IFF_UP
	gboolean showdown = FALSE;
# endif
#endif
#if defined(IFF_LOOPBACK) || defined(IFF_UP)
	char const * p;
#endif

#ifdef IFF_LOOPBACK
	if((p = helper->config_get(helper->panel, "network", "loopback"))
			!= NULL)
		loopback = strtol(p, NULL, 10) ? TRUE : FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(network->pr_loopback),
			loopback);
#endif
#ifdef IFF_UP
	if((p = helper->config_get(helper->panel, "network", "showdown"))
			!= NULL)
		showdown = strtol(p, NULL, 10) ? TRUE : FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(network->pr_showdown),
			showdown);
#endif
}


/* callbacks */
/* network_on_timeout */
static gboolean _network_on_timeout(gpointer data)
{
	const unsigned int timeout = 500;
	Network * network = data;

	_network_refresh(network);
	network->source = g_timeout_add(timeout, _network_on_timeout, network);
	return FALSE;
}


/* NetworkInterface */
/* networkinterface_init */
static int _networkinterface_init(NetworkInterface * ni, char const * name,
		unsigned int flags)
{
	if((ni->name = string_new(name)) == NULL)
		return -1;
	ni->flags = flags;
	ni->ipackets = 0;
	ni->opackets = 0;
	ni->ibytes = 0;
	ni->obytes = 0;
	ni->widget = gtk_image_new();
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(ni->widget, name);
#endif
	ni->updated = FALSE;
	return 0;
}


/* networkinterface_destroy */
static void _networkinterface_destroy(NetworkInterface * ni)
{
	string_delete(ni->name);
	gtk_widget_destroy(ni->widget);
}


/* networkinterface_update */
static void _networkinterface_update(NetworkInterface * ni, char const * icon,
		GtkIconSize iconsize, gboolean active, unsigned int flags,
		gboolean updated, char const * tooltip)
{
	gtk_image_set_from_icon_name(GTK_IMAGE(ni->widget), icon, iconsize);
#ifdef EMBEDDED
	if(active)
		gtk_widget_show(ni->widget);
	else
		gtk_widget_hide(ni->widget);
#else
	gtk_widget_set_sensitive(ni->widget, active);
#endif
#if GTK_CHECK_VERSION(2, 12, 0)
	if(tooltip != NULL)
		gtk_widget_set_tooltip_text(ni->widget, tooltip);
#endif
	ni->flags = flags;
	ni->updated = updated;
}
