/* $Id$ */
/* Copyright (c) 2009-2015 Pierre Pronchery <khorben@defora.org> */
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



#ifndef PANEL_PANEL_H
# define PANEL_PANEL_H

# include <System/string.h>
# include <Desktop.h>
# include "../include/Panel/panel.h"
# include "../config.h"


/* Panel */
/* types */
typedef struct _PanelPrefs
{
	String const * iconsize;
	int monitor;
} PanelPrefs;


/* constants */
# define PANEL_BORDER_WIDTH		4

# define PANEL_CONFIG_FILE		"Panel.conf"
# define PANEL_CONFIG_VENDOR		"DeforaOS/" VENDOR

# define PANEL_ICON_SIZE_DEFAULT	"panel-large"
# define PANEL_ICON_SIZE_UNSET		NULL
# define PANEL_ICON_SIZE_SMALL		"panel-small"
# define PANEL_ICON_SIZE_SMALLER	"panel-smaller"
# define PANEL_ICON_SIZE_LARGE		"panel-large"


/* functions */
Panel * panel_new(PanelPrefs const * prefs);
void panel_delete(Panel * panel);

/* accessors */
String const * panel_get_config(Panel * panel, String const * section,
		String const * variable);

/* useful */
int panel_error(Panel * panel, String const * message, int ret);
int panel_load(Panel * panel, PanelPosition position, String const * applet);
int panel_reset(Panel * panel);
int panel_save(Panel * panel);

void panel_show_preferences(Panel * panel, gboolean show);

#endif /* !PANEL_PANEL_H */
