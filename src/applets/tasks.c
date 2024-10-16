/* $Id$ */
/* Copyright (c) 2011-2024 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <gtk/gtk.h>
#if defined(GDK_WINDOWING_X11)
# if GTK_CHECK_VERSION(3, 0, 0)
#  include <gtk/gtkx.h>
# else
#  include <gdk/gdkx.h>
# endif
# include <X11/Xatom.h>
#endif
#include <System.h>
#include <Desktop.h>
#include "Panel/applet.h"

#define _(string) gettext(string)
#define N_(string) string

#if !GTK_CHECK_VERSION(3, 0, 0)
# define gdk_error_trap_pop_ignored() gdk_error_trap_pop()
#endif


/* Tasks */
/* private */
/* types */
#if defined(GDK_WINDOWING_X11)
# define atom(a) TASKS_ATOM_ ## a
typedef enum _TasksAtom
{
# include "tasks.atoms"
} TasksAtom;
# define TASKS_ATOM_LAST TASKS_ATOM_UTF8_STRING
# define TASKS_ATOM_COUNT (TASKS_ATOM_LAST + 1)
# undef atom
#endif

typedef struct _PanelApplet Tasks;

#if defined(GDK_WINDOWING_X11)
typedef struct _Task
{
	Tasks * tasks;
	Window window;
	GtkWidget * widget;
	GtkWidget * image;
	GtkWidget * label;
	gboolean delete;
	gboolean reorder;
} Task;
#endif

struct _PanelApplet
{
	PanelAppletHelper * helper;
#if defined(GDK_WINDOWING_X11)
	Task ** tasks;
	size_t tasks_cnt;
	gboolean label;
	gboolean reorder;
	gboolean embedded;

	GtkWidget * widget;
	GtkWidget * hbox;
	GtkIconSize iconsize;
	int icon_width;
	int icon_height;
	gulong source;

	Atom atom[TASKS_ATOM_COUNT];
	GdkDisplay * display;
	GdkScreen * screen;
	GdkWindow * root;
#endif
};


#if defined(GDK_WINDOWING_X11)
/* constants */
# define atom(a) "" # a
static const char * _tasks_atom[TASKS_ATOM_COUNT] =
{
# include "tasks.atoms"
};
# undef atom

# define _NET_WM_MOVERESIZE_MOVE		 8 /* movement only */
# define _NET_WM_MOVERESIZE_SIZE_KEYBOARD	 9 /* size via keyboard */
# define _NET_WM_MOVERESIZE_MOVE_KEYBOARD	10 /* move via keyboard */
#endif


/* prototypes */
#if defined(GDK_WINDOWING_X11)
/* task */
static Task * _task_new(Tasks * tasks, gboolean label, gboolean reorder,
		Window window, char const * name, GdkPixbuf * pixbuf);
static void _task_delete(Task * Task);
static void _task_set(Task * task, char const * name, GdkPixbuf * pixbuf);
static void _task_toggle_state(Task * task, TasksAtom state);
static void _task_toggle_state2(Task * task, TasksAtom state1,
		TasksAtom state2);
#endif

/* tasks */
static Tasks * _tasks_init(PanelAppletHelper * helper, GtkWidget ** widget);
static void _tasks_destroy(Tasks * tasks);

#if defined(GDK_WINDOWING_X11)
/* accessors */
static int _tasks_get_current_desktop(Tasks * tasks);
static int _tasks_get_text_property(Tasks * tasks, Window window, Atom property,
		char ** ret);
static int _tasks_get_window_property(Tasks * tasks, Window window,
		TasksAtom property, Atom atom, unsigned long * cnt,
		unsigned char ** ret);

/* useful */
static void _tasks_do(Tasks * tasks);

/* callbacks */
static gboolean _task_on_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data);
static void _task_on_clicked(gpointer data);
static gboolean _task_on_delete_event(gpointer data);
static GdkFilterReturn _task_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data);
static gboolean _task_on_popup(gpointer data);
static void _task_on_popup_change_desktop(gpointer data);
static void _task_on_popup_close(gpointer data);
static void _task_on_popup_fullscreen(gpointer data);
static void _task_on_popup_maximize(gpointer data);
static void _task_on_popup_maximize_horz(gpointer data);
static void _task_on_popup_maximize_vert(gpointer data);
static void _task_on_popup_minimize(gpointer data);
static void _task_on_popup_move(gpointer data);
static void _task_on_popup_resize(gpointer data);
static void _task_on_popup_shade(gpointer data);
static void _task_on_popup_stick(gpointer data);
static void _task_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data);
#endif


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Tasks"),
	"application-x-executable",
	NULL,
	_tasks_init,
	_tasks_destroy,
	NULL,
#ifndef EMBEDDED
	TRUE,
#else
	FALSE,
#endif
	TRUE
};


#if defined(GDK_WINDOWING_X11)
/* private */
/* functions */
/* Task */
/* task_new */
static Task * _task_new(Tasks * tasks, gboolean label, gboolean reorder,
		Window window, char const * name, GdkPixbuf * pixbuf)
{
	Task * task;
	GtkWidget * hbox;

	if((task = malloc(sizeof(*task))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	task->tasks = tasks;
	task->window = window;
	task->widget = gtk_button_new();
	g_signal_connect(task->widget, "button-press-event", G_CALLBACK(
				_task_on_button_press), task);
	g_signal_connect_swapped(task->widget, "delete-event", G_CALLBACK(
				_task_on_delete_event), task);
	g_signal_connect_swapped(task->widget, "popup-menu", G_CALLBACK(
				_task_on_popup), task);
	g_signal_connect_swapped(task->widget, "clicked", G_CALLBACK(
				_task_on_clicked), task);
	task->image = gtk_image_new();
	task->delete = FALSE;
	task->reorder = reorder;
# if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
# else
	hbox = gtk_hbox_new(FALSE, 0);
# endif
	gtk_box_pack_start(GTK_BOX(hbox), task->image, FALSE, TRUE, 0);
	if(label)
	{
		task->label = gtk_label_new(name);
# if GTK_CHECK_VERSION(3, 0, 0) /* XXX should work with Gtk+ 2 too */
		gtk_label_set_ellipsize(GTK_LABEL(task->label),
				PANGO_ELLIPSIZE_END);
# endif
		if(tasks->iconsize == GTK_ICON_SIZE_LARGE_TOOLBAR)
			gtk_label_set_line_wrap(GTK_LABEL(task->label), TRUE);
# if GTK_CHECK_VERSION(2, 10, 0)
		gtk_label_set_line_wrap_mode(GTK_LABEL(task->label),
				PANGO_WRAP_WORD_CHAR);
# endif
		gtk_box_pack_start(GTK_BOX(hbox), task->label, FALSE, TRUE, 4);
		gtk_widget_set_size_request(task->widget, tasks->icon_width,
				tasks->icon_height);
	}
	else
		task->label = NULL;
	gtk_container_add(GTK_CONTAINER(task->widget), hbox);
	_task_set(task, name, pixbuf);
	return task;
}


/* task_delete */
static void _task_delete(Task * task)
{
	if(task->widget != NULL)
		gtk_widget_destroy(task->widget);
	free(task);
}


/* task_set */
static void _task_set(Task * task, char const * name, GdkPixbuf * pixbuf)
{
	if(task->label != NULL)
		gtk_label_set_text(GTK_LABEL(task->label), name);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(task->widget, name);
#endif
	if(pixbuf != NULL)
		gtk_image_set_from_pixbuf(GTK_IMAGE(task->image), pixbuf);
	else
		gtk_image_set_from_icon_name(GTK_IMAGE(task->image),
				"application-x-executable",
				task->tasks->iconsize);
}


/* task_toggle_state */
static void _task_toggle_state(Task * task, TasksAtom state)
{
	_task_toggle_state2(task, state, 0);
}


/* task_toggle_state2 */
static void _task_toggle_state2(Task * task, TasksAtom state1,
		TasksAtom state2)
{
	Tasks * tasks = task->tasks;
	GdkDisplay * display;
	XEvent xev;

	display = tasks->display;
	memset(&xev, 0, sizeof(xev));
	xev.xclient.type = ClientMessage;
	xev.xclient.window = task->window;
	xev.xclient.message_type = tasks->atom[TASKS_ATOM__NET_WM_STATE];
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = tasks->atom[TASKS_ATOM__NET_WM_STATE_TOGGLE];
	xev.xclient.data.l[1] = tasks->atom[state1];
	xev.xclient.data.l[2] = (state2 != 0) ? tasks->atom[state2] : 0;
	xev.xclient.data.l[3] = 2;
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XID(tasks->root),
			False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
	gdk_error_trap_pop_ignored();
}
#endif


/* Tasks */
/* tasks_init */
static Tasks * _tasks_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
#if defined(GDK_WINDOWING_X11)
	Tasks * tasks;
	char const * p;
	GtkOrientation orientation;

	if((tasks = malloc(sizeof(*tasks))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	tasks->helper = helper;
	tasks->tasks = NULL;
	tasks->tasks_cnt = 0;
	/* config: label */
	if((p = helper->config_get(helper->panel, "tasks", "label")) != NULL)
		tasks->label = strtol(p, NULL, 0) ? TRUE : FALSE;
	else
#ifdef EMBEDDED
		tasks->label = FALSE;
#else
		tasks->label = TRUE;
#endif
	/* config: reorder */
	if((p = helper->config_get(helper->panel, "tasks", "reorder")) != NULL)
		tasks->reorder = strtol(p, NULL, 0) ? TRUE : FALSE;
	else
#ifdef EMBEDDED
		tasks->reorder = TRUE;
#else
		tasks->reorder = FALSE;
#endif
#ifdef EMBEDDED
	tasks->embedded = TRUE;
#else
	tasks->embedded = FALSE;
#endif
	orientation = panel_window_get_orientation(helper->window);
#if GTK_CHECK_VERSION(3, 0, 0)
	tasks->hbox = gtk_box_new(orientation, 0);
	gtk_box_set_homogeneous(GTK_BOX(tasks->hbox), TRUE);
#else
	tasks->hbox = (orientation == GTK_ORIENTATION_VERTICAL)
		? gtk_vbox_new(TRUE, 0) : gtk_hbox_new(TRUE, 0);
#endif
	tasks->source = g_signal_connect(tasks->hbox, "screen-changed",
			G_CALLBACK(_task_on_screen_changed), tasks);
	tasks->iconsize = panel_window_get_icon_size(helper->window);
	tasks->icon_width = 48;
	tasks->icon_height = 48;
	gtk_icon_size_lookup(tasks->iconsize, &tasks->icon_width,
			&tasks->icon_height);
	tasks->icon_width -= 4;
	tasks->icon_height -= 4;
	tasks->display = NULL;
	tasks->screen = NULL;
	tasks->root = NULL;
#ifndef EMBEDDED
	tasks->widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tasks->widget),
			GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(tasks->widget),
			GTK_SHADOW_NONE);
# if GTK_CHECK_VERSION(3, 8, 0)
	gtk_container_add(GTK_CONTAINER(tasks->widget), tasks->hbox);
# else
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(
				tasks->widget), tasks->hbox);
# endif
#else
	tasks->widget = tasks->hbox;
#endif
	gtk_widget_show_all(tasks->widget);
	*widget = tasks->widget;
	return tasks;
#else
	(void) helper;
	(void) widget;

	error_set_code(-ENOSYS, "X11 support not detected");
	return NULL;
#endif
}


/* tasks_destroy */
static void _tasks_destroy(Tasks * tasks)
{
#if defined(GDK_WINDOWING_X11)
	size_t i;

	if(tasks->source != 0)
		g_signal_handler_disconnect(tasks->widget, tasks->source);
	tasks->source = 0;
	if(tasks->root != NULL)
		gdk_window_remove_filter(tasks->root, _task_on_filter, tasks);
	for(i = 0; i < tasks->tasks_cnt; i++)
		_task_delete(tasks->tasks[i]);
	free(tasks->tasks);
	tasks->tasks = NULL;
	tasks->tasks_cnt = 0;
	gtk_widget_destroy(tasks->widget);
	free(tasks);
#else
	(void) tasks;
#endif
}


#if defined(GDK_WINDOWING_X11)
/* accessors */
/* tasks_get_current_desktop */
static int _tasks_get_current_desktop(Tasks * tasks)
{
	unsigned long cnt;
	unsigned long *p;

	if(tasks->embedded)
		/* ignore the current desktop */
		return -1;
	if(_tasks_get_window_property(tasks, GDK_WINDOW_XID(tasks->root),
				TASKS_ATOM__NET_CURRENT_DESKTOP, XA_CARDINAL,
				&cnt, (void *)&p) != 0)
		return -1;
	cnt = *p;
	XFree(p);
	return cnt;
}


/* tasks_get_text_property */
static int _tasks_get_text_property(Tasks * tasks, Window window, Atom property,
		char ** ret)
{
	int res;
	XTextProperty text;
	GdkAtom atom;
	int cnt;
	char ** list;
	int i;

# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(tasks, window, %lu)\n", __func__, property);
# endif
	gdk_error_trap_push();
	res = XGetTextProperty(GDK_DISPLAY_XDISPLAY(tasks->display), window,
			&text, property);
	if(gdk_error_trap_pop() != 0 || res == 0)
		return -1;
	atom = gdk_x11_xatom_to_atom(text.encoding);
# if GTK_CHECK_VERSION(2, 24, 0)
	cnt = gdk_x11_display_text_property_to_text_list(tasks->display,
			atom, text.format, text.value, text.nitems, &list);
# else
	cnt = gdk_text_property_to_utf8_list(atom, text.format, text.value,
			text.nitems, &list);
# endif
	if(cnt > 0)
	{
		*ret = list[0];
		for(i = 1; i < cnt; i++)
			g_free(list[i]);
		g_free(list);
	}
	else
		*ret = NULL;
	if(text.value != NULL)
		XFree(text.value);
	return 0;
}


/* tasks_get_window_property */
static int _tasks_get_window_property(Tasks * tasks, Window window,
		TasksAtom property, Atom atom, unsigned long * cnt,
		unsigned char ** ret)
{
	int res;
	Atom type;
	int format;
	unsigned long bytes;

# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(tasks, window, %s, %lu)\n", __func__,
			_tasks_atom[property], atom);
# endif
	gdk_error_trap_push();
	res = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(tasks->display), window,
			tasks->atom[property], 0, G_MAXLONG, False, atom,
			&type, &format, cnt, &bytes, ret);
	if(gdk_error_trap_pop() != 0 || res != Success)
		return -1;
	if(type != atom)
	{
		if(*ret != NULL)
			XFree(*ret);
		*ret = NULL;
		return 1;
	}
	return 0;
}


/* tasks_do */
static char * _do_name(Tasks * tasks, Window window);
static char * _do_name_text(Tasks * tasks, Window window, Atom property);
static char * _do_name_utf8(Tasks * tasks, Window window, Atom property);
static GdkPixbuf * _do_pixbuf(Tasks * tasks, Window window);
static int _do_tasks_add(Tasks * tasks, int desktop, Window window,
		char const * name, GdkPixbuf * pixbuf);
static void _do_tasks_clean(Tasks * tasks);
static int _do_typehint_normal(Tasks * tasks, Window window);

static void _tasks_do(Tasks * tasks)
{
	unsigned long cnt = 0;
	Window * windows = NULL;
	int desktop;
	unsigned long i;
	char * name;

# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
# endif
	if(_tasks_get_window_property(tasks, GDK_WINDOW_XID(tasks->root),
				TASKS_ATOM__NET_CLIENT_LIST,
				XA_WINDOW, &cnt, (void *)&windows) != 0)
		return;
	desktop = _tasks_get_current_desktop(tasks);
	for(i = 0; i < tasks->tasks_cnt; i++)
		tasks->tasks[i]->delete = TRUE;
	for(i = 0; i < cnt; i++)
	{
		if(_do_typehint_normal(tasks, windows[i]) != 0)
			continue;
		if((name = _do_name(tasks, windows[i])) == NULL)
			continue;
		_do_tasks_add(tasks, desktop, windows[i], name, _do_pixbuf(
					tasks, windows[i]));
		g_free(name);
	}
	_do_tasks_clean(tasks);
	XFree(windows);
}

static char * _do_name(Tasks * tasks, Window window)
{
	char * ret;

	if((ret = _do_name_utf8(tasks, window, TASKS_ATOM__NET_WM_VISIBLE_NAME))
			!= NULL)
		return ret;
	if((ret = _do_name_utf8(tasks, window, TASKS_ATOM__NET_WM_NAME))
			!= NULL)
		return ret;
	if((ret = _do_name_text(tasks, window, XA_WM_NAME)) != NULL)
		return ret;
	return g_strdup(_("(Untitled)"));
}

static char * _do_name_text(Tasks * tasks, Window window, Atom property)
{
	char * ret = NULL;

	if(_tasks_get_text_property(tasks, window, property, (void *)&ret) != 0)
		return NULL;
	return ret;
}

static char * _do_name_utf8(Tasks * tasks, Window window, Atom property)
{
	char * ret = NULL;
	char * str = NULL;
	unsigned long cnt = 0;

	if(_tasks_get_window_property(tasks, window, property,
				tasks->atom[TASKS_ATOM_UTF8_STRING], &cnt,
				(void *)&str) != 0)
		return NULL;
	if(g_utf8_validate(str, cnt, NULL))
		ret = g_strndup(str, cnt);
	XFree(str);
	return ret;
}

static GdkPixbuf * _do_pixbuf(Tasks * tasks, Window window)
{
	GdkPixbuf * ret;
	unsigned long cnt = 0;
	unsigned long * buf = NULL;
	unsigned long i;
	long width;
	long height;
	unsigned long size;
	unsigned char * pixbuf = NULL;
	unsigned long j;
	GdkPixbuf * p;
	unsigned long * best = NULL;

	if(_tasks_get_window_property(tasks, window, TASKS_ATOM__NET_WM_ICON,
				XA_CARDINAL, &cnt, (void *)&buf) != 0)
		return NULL;
	for(i = 0; i < cnt - 3; i += 2 + (width * height))
	{
		width = buf[i];
		height = buf[i + 1];
		if(i + 2 + (width * height) > cnt)
			break;
		if(width <= 0 || height <= 0 || width != height)
			continue;
		if(tasks->icon_width == width)
		{
			best = &buf[i];
			break;
		}
		if(best == NULL || abs(best[0] - tasks->icon_width)
				> abs(width - tasks->icon_width))
			best = &buf[i];
	}
	if(best != NULL)
	{
		width = best[0];
		height = best[1];
		size = width * height * 4;
		pixbuf = malloc(size);
	}
	if(best == NULL || pixbuf == NULL)
	{
		XFree(buf);
		return NULL;
	}
	for(i = 2, j = 0; j < size; i++)
	{
		pixbuf[j++] = (best[i] >> 16) & 0xff; /* red */
		pixbuf[j++] = (best[i] >> 8) & 0xff; /* green */
		pixbuf[j++] = best[i] & 0xff; /* blue */
		pixbuf[j++] = best[i] >> 24; /* alpha */
	}
	p = gdk_pixbuf_new_from_data(pixbuf, GDK_COLORSPACE_RGB, TRUE, 8, width,
			height, width * 4, (GdkPixbufDestroyNotify)free, NULL);
	XFree(buf);
	if(width == tasks->icon_width)
		return p;
	ret = gdk_pixbuf_scale_simple(p, tasks->icon_width, tasks->icon_height,
			GDK_INTERP_BILINEAR);
	g_object_unref(p);
	return ret;
}

static int _do_tasks_add(Tasks * tasks, int desktop, Window window,
		char const * name, GdkPixbuf * pixbuf)
{
	size_t i;
	Task * p = NULL;
# ifndef EMBEDDED
	unsigned long * l;
	unsigned long cnt;
	int cur = -1;
# endif
	Task ** q;

# ifndef EMBEDDED
	if(_tasks_get_window_property(tasks, window, TASKS_ATOM__NET_WM_DESKTOP,
			XA_CARDINAL, &cnt, (void *)&l) == 0)
	{
		if(cnt == 1)
			cur = *l;
		XFree(l);
	}
	if(cur >= 0 && cur != desktop)
		return 0;
# endif
	for(i = 0; i < tasks->tasks_cnt; i++)
		if(tasks->tasks[i]->window == window)
			break;
	if(i < tasks->tasks_cnt) /* found the task */
	{
		p = tasks->tasks[i];
		_task_set(p, name, pixbuf);
		p->delete = FALSE;
		return 0;
	}
	if((q = realloc(tasks->tasks, (tasks->tasks_cnt + 1) * sizeof(*q)))
			== NULL)
		return 1;
	tasks->tasks = q;
	if((p = _task_new(tasks, tasks->label, tasks->reorder, window, name,
					pixbuf)) == NULL)
		return 1;
	tasks->tasks[tasks->tasks_cnt++] = p;
	gtk_widget_show_all(p->widget);
	gtk_box_pack_start(GTK_BOX(tasks->hbox), p->widget, FALSE, TRUE, 0);
	if(tasks->reorder)
		gtk_box_reorder_child(GTK_BOX(tasks->hbox), p->widget, 0);
	return 0;
}

static void _do_tasks_clean(Tasks * tasks)
{
	size_t i;
	size_t cnt;
	size_t j;
	Task ** q;

	for(i = 0, cnt = tasks->tasks_cnt; i < cnt;)
	{
		if(tasks->tasks[i]->delete == FALSE)
		{
			i++;
			continue;
		}
		_task_delete(tasks->tasks[i]);
		cnt--;
		for(j = i; j < cnt; j++)
			tasks->tasks[j] = tasks->tasks[j + 1];
		if((q = realloc(tasks->tasks, cnt * sizeof(*q))) != NULL
				|| cnt == 0)
			tasks->tasks = q;
	}
	tasks->tasks_cnt = cnt;
}

static int _do_typehint_normal(Tasks * tasks, Window window)
{
	Atom typehint;
	Atom * p;
	unsigned long cnt = 0;

	if(_tasks_get_window_property(tasks, window,
				TASKS_ATOM__NET_WM_WINDOW_TYPE, XA_ATOM, &cnt,
				(void *)&p) == 0)
	{
		typehint = *p;
		XFree(p);
		return typehint == tasks->atom[
			TASKS_ATOM__NET_WM_WINDOW_TYPE_NORMAL] ? 0 : 1;
	}
	/* FIXME return 1 if WM_TRANSIENT_FOR is set */
	return 0;
}


/* callbacks */
/* task_on_button_press */
static gboolean _task_on_button_press(GtkWidget * widget,
		GdkEventButton * event, gpointer data)
{
	(void) widget;

	if(event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;
	_task_on_popup(data);
	return TRUE;
}


/* task_on_clicked */
static void _clicked_activate(Task * task);

static void _task_on_clicked(gpointer data)
{
	Task * task = data;

	_clicked_activate(task);
	if(task->reorder)
		gtk_box_reorder_child(GTK_BOX(task->tasks->hbox), task->widget,
				0);
}

static void _clicked_activate(Task * task)
{
	GdkDisplay * display;
	XEvent xev;
# ifdef DEBUG
	int res;
# endif

	display = task->tasks->display;
	memset(&xev, 0, sizeof(xev));
	xev.xclient.type = ClientMessage;
	xev.xclient.window = task->window;
	xev.xclient.message_type = task->tasks->atom[
		TASKS_ATOM__NET_ACTIVE_WINDOW];
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 2;
	xev.xclient.data.l[1] = gdk_x11_display_get_user_time(display);
	xev.xclient.data.l[2] = 0;
	gdk_error_trap_push();
# ifdef DEBUG
	res =
# endif
		XSendEvent(GDK_DISPLAY_XDISPLAY(display),
			GDK_WINDOW_XID(task->tasks->root), False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
# ifdef DEBUG
	if(gdk_error_trap_pop() != 0 || res != Success)
		fprintf(stderr, "DEBUG: %s() error\n", __func__);
# else
	gdk_error_trap_pop_ignored();
# endif
}


/* task_on_delete_event */
static gboolean _task_on_delete_event(gpointer data)
{
	Task * task = data;

	task->widget = NULL;
	return FALSE;
}


/* task_on_filter */
static GdkFilterReturn _task_on_filter(GdkXEvent * xevent, GdkEvent * event,
		gpointer data)
{
	Tasks * tasks = data;
	XEvent * xev = xevent;
	(void) event;

	if(xev->type != PropertyNotify)
		return GDK_FILTER_CONTINUE;
	if(xev->xproperty.atom != tasks->atom[TASKS_ATOM__NET_CLIENT_LIST]
# ifndef EMBEDDED
			&& xev->xproperty.atom
			!= tasks->atom[TASKS_ATOM__NET_CURRENT_DESKTOP]
# endif
			)
		return GDK_FILTER_CONTINUE;
	_tasks_do(tasks);
	return GDK_FILTER_CONTINUE;
}


/* task_on_popup */
static gboolean _task_on_popup(gpointer data)
{
	Task * task = data;
	unsigned long cnt = 0;
	unsigned long * buf = NULL;
	unsigned long i;
	const struct {
		TasksAtom atom;
		void (*callback)(gpointer data);
		char const * stock;
		char const * label;
	} items[] = {
		{ TASKS_ATOM__NET_WM_ACTION_MOVE, _task_on_popup_move, NULL,
			N_("Move") },
		{ TASKS_ATOM__NET_WM_ACTION_RESIZE, _task_on_popup_resize, NULL,
			N_("Resize") },
		{ TASKS_ATOM__NET_WM_ACTION_MINIMIZE, _task_on_popup_minimize,
			NULL, N_("Minimize") },
		{ TASKS_ATOM__NET_WM_ACTION_SHADE, _task_on_popup_shade, NULL,
			N_("Shade") },
		{ TASKS_ATOM__NET_WM_ACTION_STICK, _task_on_popup_stick, NULL,
			N_("Stick") },
		{ TASKS_ATOM__NET_WM_ACTION_MAXIMIZE_HORZ,
			_task_on_popup_maximize_horz, NULL,
			N_("Maximize horizontally") },
		{ TASKS_ATOM__NET_WM_ACTION_MAXIMIZE_VERT,
			_task_on_popup_maximize_vert, NULL,
			N_("Maximize vertically") },
		{ TASKS_ATOM__NET_WM_ACTION_FULLSCREEN, _task_on_popup_fullscreen,
			GTK_STOCK_FULLSCREEN, N_("Fullscreen") },
		{ TASKS_ATOM__NET_WM_ACTION_CHANGE_DESKTOP,
			_task_on_popup_change_desktop, NULL,
			N_("Change desktop") },
		{ TASKS_ATOM__NET_WM_ACTION_CLOSE, _task_on_popup_close,
			GTK_STOCK_CLOSE, N_("Close") }
	};
	const size_t items_cnt = sizeof(items) / sizeof(*items);
	size_t j;
	GtkWidget * menu = NULL;
	GtkWidget * menuitem;
	int max = 0;

	if(_tasks_get_window_property(task->tasks, task->window,
				TASKS_ATOM__NET_WM_ALLOWED_ACTIONS, XA_ATOM,
				&cnt, (void *)&buf) != 0)
		return FALSE;
	for(i = 0; i < cnt; i++)
	{
		for(j = 0; j < items_cnt; j++)
			if(buf[i] == task->tasks->atom[items[j].atom])
				break;
		if(j == items_cnt)
			continue;
		if(items[j].atom == TASKS_ATOM__NET_WM_ACTION_CHANGE_DESKTOP)
			continue; /* FIXME implement as a special case */
		if(menu == NULL)
			menu = gtk_menu_new();
		if(items[j].stock != NULL)
# if GTK_CHECK_VERSION(3, 10, 0)
		{
			menuitem = gtk_image_menu_item_new_with_label(
					items[j].label);
			gtk_image_menu_item_set_image(
					GTK_IMAGE_MENU_ITEM(menuitem),
					gtk_image_new_from_icon_name(
						items[j].stock,
						GTK_ICON_SIZE_MENU));
		}
# else
			menuitem = gtk_image_menu_item_new_from_stock(
					items[j].stock, NULL);
# endif
		else
			menuitem = gtk_menu_item_new_with_label(
					_(items[j].label));
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					items[j].callback), task);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		/* maximizing horizontally and vertically */
		if(items[j].atom != TASKS_ATOM__NET_WM_ACTION_MAXIMIZE_VERT
				&& items[j].atom
				!= TASKS_ATOM__NET_WM_ACTION_MAXIMIZE_HORZ)
			continue;
		if(max++ != 1)
			continue;
		menuitem = gtk_menu_item_new_with_label(_("Maximize"));
		g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(
					_task_on_popup_maximize), task);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	XFree(buf);
	if(menu == NULL)
		return FALSE;
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, task, 2,
			gtk_get_current_event_time());
	return TRUE;
}


/* task_on_popup_change_desktop */
static void _task_on_popup_change_desktop(gpointer data)
{
	/* FIXME implement */
}


/* task_on_popup_close */
static void _task_on_popup_close(gpointer data)
{
	Task * task = data;
	GdkDisplay * display;
	XEvent xev;

	display = task->tasks->display;
	memset(&xev, 0, sizeof(xev));
	xev.xclient.type = ClientMessage;
	xev.xclient.window = task->window;
	xev.xclient.message_type = task->tasks->atom[
		TASKS_ATOM__NET_CLOSE_WINDOW];
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = gdk_x11_display_get_user_time(display);
	xev.xclient.data.l[1] = 2;
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display),
			GDK_WINDOW_XID(task->tasks->root), False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
	gdk_error_trap_pop_ignored();
}


/* task_on_popup_fullscreen */
static void _task_on_popup_fullscreen(gpointer data)
{
	Task * task = data;

	_task_toggle_state(task, TASKS_ATOM__NET_WM_STATE_FULLSCREEN);
}


/* task_on_popup_maximize */
static void _task_on_popup_maximize(gpointer data)
{
	Task * task = data;

	_task_toggle_state2(task, TASKS_ATOM__NET_WM_STATE_MAXIMIZED_HORZ,
			TASKS_ATOM__NET_WM_STATE_MAXIMIZED_VERT);
}


/* task_on_popup_maximize_hort */
static void _task_on_popup_maximize_horz(gpointer data)
{
	Task * task = data;

	_task_toggle_state(task, TASKS_ATOM__NET_WM_STATE_MAXIMIZED_HORZ);
}


/* task_on_popup_maximize_vert */
static void _task_on_popup_maximize_vert(gpointer data)
{
	Task * task = data;

	_task_toggle_state(task, TASKS_ATOM__NET_WM_STATE_MAXIMIZED_VERT);
}


/* task_on_popup_minimize */
static void _task_on_popup_minimize(gpointer data)
{
	Task * task = data;

	gdk_error_trap_push();
	XIconifyWindow(GDK_DISPLAY_XDISPLAY(task->tasks->display), task->window,
			gdk_x11_screen_get_screen_number(task->tasks->screen));
	gdk_error_trap_pop_ignored();
}


/* task_on_popup_move */
static void _task_on_popup_move(gpointer data)
{
	Task * task = data;
	Tasks * tasks = task->tasks;
	GdkDisplay * display;
	XEvent xev;

	display = tasks->display;
	memset(&xev, 0, sizeof(xev));
	xev.xclient.type = ClientMessage;
	xev.xclient.window = task->window;
	xev.xclient.message_type = tasks->atom[TASKS_ATOM__NET_WM_MOVERESIZE];
	xev.xclient.format = 32;
	memset(&xev.xclient.data, 0, sizeof(xev.xclient.data));
	xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
	xev.xclient.data.l[3] = 1; /* XXX may not always be the case */
	xev.xclient.data.l[4] = 2;
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display),
			GDK_WINDOW_XID(tasks->root), False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
	gdk_error_trap_pop_ignored();
}


/* task_on_popup_resize */
static void _task_on_popup_resize(gpointer data)
{
	Task * task = data;
	Tasks * tasks = task->tasks;
	GdkDisplay * display;
	XEvent xev;

	display = tasks->display;
	memset(&xev, 0, sizeof(xev));
	xev.xclient.type = ClientMessage;
	xev.xclient.window = task->window;
	xev.xclient.message_type = tasks->atom[TASKS_ATOM__NET_WM_MOVERESIZE];
	xev.xclient.format = 32;
	memset(&xev.xclient.data, 0, sizeof(xev.xclient.data));
	xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
	xev.xclient.data.l[3] = 1; /* XXX may not always be the case */
	xev.xclient.data.l[4] = 2;
	gdk_error_trap_push();
	XSendEvent(GDK_DISPLAY_XDISPLAY(display),
			GDK_WINDOW_XID(tasks->root), False,
			SubstructureNotifyMask | SubstructureRedirectMask,
			&xev);
	gdk_error_trap_pop_ignored();
}


/* task_on_popup_shade */
static void _task_on_popup_shade(gpointer data)
{
	Task * task = data;

	_task_toggle_state(task, TASKS_ATOM__NET_WM_STATE_SHADED);
}


/* task_on_popup_stick */
static void _task_on_popup_stick(gpointer data)
{
	Task * task = data;

	_task_toggle_state(task, TASKS_ATOM__NET_WM_STATE_STICKY);
}


/* task_on_screen_changed */
static void _task_on_screen_changed(GtkWidget * widget, GdkScreen * previous,
		gpointer data)
{
	Tasks * tasks = data;
	GdkEventMask events;
	size_t i;
	(void) previous;

# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
# endif
	if(tasks->root != NULL)
		gdk_window_remove_filter(tasks->root, _task_on_filter, tasks);
	tasks->screen = gtk_widget_get_screen(widget);
	tasks->display = gdk_screen_get_display(tasks->screen);
	tasks->root = gdk_screen_get_root_window(tasks->screen);
	events = gdk_window_get_events(tasks->root);
	gdk_window_set_events(tasks->root, events | GDK_PROPERTY_CHANGE_MASK);
	gdk_window_add_filter(tasks->root, _task_on_filter, tasks);
	/* atoms */
	for(i = 0; i < TASKS_ATOM_COUNT; i++)
		tasks->atom[i] = gdk_x11_get_xatom_by_name_for_display(
				tasks->display, _tasks_atom[i]);
	_tasks_do(tasks);
}
#endif
