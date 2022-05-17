/* $Id$ */
/* Copyright (c) 2015-2022 Pierre Pronchery <khorben@defora.org> */
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



#ifndef DESKTOP_PANEL_PANEL_H
# define DESKTOP_PANEL_PANEL_H


/* Panel */
/* types */
typedef struct _Panel Panel;

typedef enum _PanelPosition
{
	PANEL_POSITION_BOTTOM = 0,
	PANEL_POSITION_TOP,
	PANEL_POSITION_LEFT,
	PANEL_POSITION_RIGHT
} PanelPosition;
# define PANEL_POSITION_LAST		PANEL_POSITION_RIGHT
# define PANEL_POSITION_COUNT		(PANEL_POSITION_LAST + 1)
# define PANEL_POSITION_DEFAULT		PANEL_POSITION_BOTTOM

typedef enum _PanelMessage
{
	PANEL_MESSAGE_SHOW = 0,		/* PanelMessageShow, gboolean visible */
	PANEL_MESSAGE_EMBED		/* Window */
} PanelMessage;

typedef enum _PanelMessageShow
{
	PANEL_MESSAGE_SHOW_PANEL_BOTTOM	= 0x1,
	PANEL_MESSAGE_SHOW_PANEL_TOP	= 0x2,
	PANEL_MESSAGE_SHOW_PANEL_LEFT	= 0x4,
	PANEL_MESSAGE_SHOW_PANEL_RIGHT	= 0x8,
	PANEL_MESSAGE_SHOW_SETTINGS	= 0x10
} PanelMessageShow;


/* constants */
# define PANEL_CLIENT_MESSAGE		"DEFORAOS_DESKTOP_PANEL_CLIENT"

#endif /* !DESKTOP_PANEL_PANEL_H */
