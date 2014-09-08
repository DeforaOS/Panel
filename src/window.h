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



#ifndef PANEL_WINDOW_H
# define PANEL_WINDOW_H

# include <stdint.h>
# include "panel.h"


/* PanelWindow */
/* types */
typedef enum _PanelWindowPosition
{
	PANEL_WINDOW_POSITION_BOTTOM = 0,
	PANEL_WINDOW_POSITION_TOP,
	PANEL_WINDOW_POSITION_LEFT,
	PANEL_WINDOW_POSITION_RIGHT,
	PANEL_WINDOW_POSITION_CENTER,
	PANEL_WINDOW_POSITION_FLOATING,
	PANEL_WINDOW_POSITION_MANAGED,
	PANEL_WINDOW_POSITION_EMBEDDED
} PanelWindowPosition;
# define PANEL_WINDOW_POSITION_DEFAULT PANEL_WINDOW_POSITION_BOTTOM

typedef struct _PanelWindow PanelWindow;


/* functions */
PanelWindow * panel_window_new(PanelWindowPosition position,
		PanelAppletHelper * helper, GdkRectangle * root);
void panel_window_delete(PanelWindow * panel);

/* accessors */
int panel_window_get_height(PanelWindow * panel);
GtkOrientation panel_window_get_orientation(PanelWindow * panel);
void panel_window_get_position(PanelWindow * panel, gint * x, gint * y);
void panel_window_get_size(PanelWindow * panel, gint * width, gint * height);
int panel_window_get_width(PanelWindow * panel);
uint32_t panel_window_get_xid(PanelWindow * panel);

void panel_window_set_accept_focus(PanelWindow * panel, gboolean accept);
void panel_window_set_keep_above(PanelWindow * panel, gboolean keep);
void panel_window_set_title(PanelWindow * panel, char const * title);

/* useful */
int panel_window_append(PanelWindow * panel, char const * applet);
void panel_window_remove_all(PanelWindow * panel);

void panel_window_reset(PanelWindow * panel, PanelWindowPosition position,
		GdkRectangle * root);
void panel_window_show(PanelWindow * panel, gboolean show);

#endif /* !PANEL_WINDOW_H */
