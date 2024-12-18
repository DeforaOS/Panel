/* $Id$ */
/* Copyright (c) 2012-2024 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <libintl.h>
#include <Desktop.h>
#include "../include/Panel.h"
#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME_PANEL_EMBED
# define PROGNAME_PANEL_EMBED	"panel-embed"
#endif
#ifndef PREFIX
# define PREFIX			"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR		PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR		DATADIR "/locale"
#endif


/* private */
/* prototypes */
static int _embed(int argc, char * argv[]);

/* callbacks */
static gboolean _embed_on_can_read(GIOChannel * channel, GIOCondition condition,
		gpointer data);
static void _embed_on_child(GPid pid, gint status, gpointer data);

static int _error(char const * message, int ret);
static int _usage(void);


/* functions */
/* embed */
static int _embed(int argc, char * argv[])
{
	const unsigned int flags = G_SPAWN_SEARCH_PATH
		| G_SPAWN_CHILD_INHERITS_STDIN | G_SPAWN_DO_NOT_REAP_CHILD;
	GPid pid;
	int fd;
	GError * error = NULL;
	GIOChannel * channel;
	(void) argc;

	if(g_spawn_async_with_pipes(NULL, argv, NULL, flags, NULL, NULL, &pid,
				NULL, &fd, NULL, &error) == FALSE)
	{
		_error(error->message, 1);
		g_error_free(error);
		return -1;
	}
	g_child_watch_add(pid, _embed_on_child, NULL);
	channel = g_io_channel_unix_new(fd);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_add_watch(channel, G_IO_IN, _embed_on_can_read, NULL);
	gtk_main();
	g_io_channel_unref(channel);
	return 0;
}


/* callbacks */
/* embed_on_can_read */
static gboolean _embed_on_can_read(GIOChannel * channel, GIOCondition condition,
		gpointer data)
{
	gchar * str;
	gsize length;
	GError * error = NULL;
	GIOStatus status;
	uint32_t xid;
	char * p;
	(void) data;

	if(condition != G_IO_IN)
		return FALSE; /* should not happen */
	status = g_io_channel_read_line(channel, &str, &length, NULL, &error);
	switch(status)
	{
		case G_IO_STATUS_ERROR:
			_error(error->message, 1);
			g_error_free(error);
			break;
		case G_IO_STATUS_NORMAL:
		case G_IO_STATUS_EOF:
			if(length == 0 || str == NULL)
				break;
			xid = strtoul(str, &p, 10);
			g_free(str);
			if(str[0] == '\0' || *p != '\n')
			{
				_error(_("Could not obtain the XID"), 1);
				break;
			}
#ifdef DEBUG
			fprintf(stderr, "DEBUG: %s() %u\n", __func__, xid);
#endif
			desktop_message_send(PANEL_CLIENT_MESSAGE,
					PANEL_MESSAGE_EMBED, xid, 0);
			return FALSE;
		case G_IO_STATUS_AGAIN:
			break;
	}
	gtk_main_quit();
	return FALSE;
}


/* embed_on_child */
static void _embed_on_child(GPid pid, gint status, gpointer data)
{
	(void) data;

	if(WIFEXITED(status) || WIFSIGNALED(status))
	{
		g_spawn_close_pid(pid);
		gtk_main_quit();
	}
}


/* error */
static int _error(char const * message, int ret)
{
	fprintf(stderr, "%s%s\n", PROGNAME_PANEL_EMBED ": ", message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s command [arguments...]\n"),
			PROGNAME_PANEL_EMBED);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "")) != -1)
		switch(o)
		{
			default:
				return _usage();
		}
	if(optind == argc)
		return _usage();
	return (_embed(argc - optind, &argv[optind]) == 0) ? 0 : 2;
}
