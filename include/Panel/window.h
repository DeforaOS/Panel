/* $Id$ */
/* Copyright (c) 2015 Pierre Pronchery <khorben@defora.org> */
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



#ifndef DESKTOP_PANEL_WINDOW_H
# define DESKTOP_PANEL_WINDOW_H


/* PanelWindow */
/* types */
typedef struct _PanelWindow PanelWindow;

typedef enum _PanelWindowType
{
	PANEL_WINDOW_TYPE_NORMAL = 0,
	PANEL_WINDOW_TYPE_NOTIFICATION
} PanelWindowType;


/* functions */
GtkIconSize panel_window_get_icon_size(PanelWindow * panel);
GtkOrientation panel_window_get_orientation(PanelWindow * panel);
PanelWindowType panel_window_get_type(PanelWindow * panel);

#endif /* !DESKTOP_PANEL_H */
