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
	char * name;
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
	GtkWidget * widget;
	guint source;
	int fd;
	NetworkInterface * interfaces;
	size_t interfaces_cnt;
} Network;


/* prototypes */
static Network * _network_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _network_destroy(Network * network);

/* useful */
static void _network_refresh(Network * network);

/* callbacks */
static gboolean _network_on_timeout(gpointer data);

/* NetworkInterface */
static int _networkinterface_init(NetworkInterface * ni, char const * name,
		unsigned int flags);
static void _networkinterface_destroy(NetworkInterface * ni);
static void _networkinterface_update(NetworkInterface * ni, char const * icon,
		GtkIconSize iconsize, unsigned int flags, gboolean updated,
		char const * tooltip);


/* public */
/* variables */
PanelAppletDefinition applet =
{
	"Network",
	"network-idle",
	NULL,
	_network_init,
	_network_destroy,
	NULL,
	FALSE,
	TRUE
};


/* private */
/* functions */
/* network_init */
static Network * _network_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
	Network * network;
	GtkOrientation orientation;

	if((network = object_new(sizeof(*network))) == NULL)
	{
		helper->error(NULL, error_get(), 1);
		return NULL;
	}
	network->helper = helper;
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	network->widget = gtk_box_new(orientation, 0);
#else
	network->widget = (orientation == GTK_ORIENTATION_HORIZONTAL)
		? gtk_hbox_new(TRUE, 0) : gtk_vbox_new(TRUE, 0);
#endif
	gtk_widget_show(network->widget);
	network->source = g_timeout_add(500, _network_on_timeout, network);
	if((network->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		network->helper->error(NULL, "socket", 1);
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

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, name);
#endif
	for(i = 0; i < network->interfaces_cnt; i++)
		if(strcmp(network->interfaces[i].name, name) == 0)
			break;
	/* FIXME really implement */
	if(i == network->interfaces_cnt)
		/* XXX assumes network->interfaces[i] will be correct */
		if(_refresh_interface_add(network, name, flags) != 0)
			return;
	_refresh_interface_flags(network, &network->interfaces[i], flags);
}

static int _refresh_interface_add(Network * network, char const * name,
		unsigned int flags)
{
	NetworkInterface * p;
#ifdef IFF_LOOPBACK
	char const * q;

	if(flags & IFF_LOOPBACK)
	{
		q = network->helper->config_get(network->helper->panel,
				"network", "loopback");
		if(q == NULL || strtol(q, NULL, 10) == 0)
			/* ignore the interface */
			return 1;
	}
#endif
	if((p = realloc(network->interfaces, sizeof(*p)
					* (network->interfaces_cnt + 1)))
			== NULL)
		return -1;
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
	char const * icon = "network-idle";
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
		icon = "network-offline";
	else
#endif
	{
#ifdef SIOCGIFDATA
		icon = "network-idle";
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
	_networkinterface_update(ni, icon,
			panel_window_get_icon_size(network->helper->window),
			flags, TRUE, (tooltip[0] != '\0') ? tooltip : NULL);
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


/* callbacks */
/* network_on_timeout */
static gboolean _network_on_timeout(gpointer data)
{
	Network * network = data;

	_network_refresh(network);
	network->source = g_timeout_add(500, _network_on_timeout, network);
	return FALSE;
}


/* NetworkInterface */
/* networkinterface_init */
static int _networkinterface_init(NetworkInterface * ni, char const * name,
		unsigned int flags)
{
	if((ni->name = strdup(name)) == NULL)
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
	free(ni->name);
	gtk_widget_destroy(ni->widget);
}


/* networkinterface_update */
static void _networkinterface_update(NetworkInterface * ni, char const * icon,
		GtkIconSize iconsize, unsigned int flags, gboolean updated,
		char const * tooltip)
{
	gtk_image_set_from_icon_name(GTK_IMAGE(ni->widget), icon, iconsize);
#if GTK_CHECK_VERSION(2, 12, 0)
	if(tooltip != NULL)
		gtk_widget_set_tooltip_text(ni->widget, tooltip);
#endif
	ni->flags = flags;
	ni->updated = updated;
}
