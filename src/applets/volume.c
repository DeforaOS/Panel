/* $Id$ */
/* Copyright (c) 2011-2021 Pierre Pronchery <khorben@defora.org> */
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
 * - update the image according to the volume in notification mode
 * - add a pulseaudio version */



#include <sys/ioctl.h>
#if defined(__NetBSD__)
# include <sys/audioio.h>
#elif defined(__linux__)
# include <alsa/asoundlib.h>
#elif !defined(__APPLE__)
# include <sys/soundcard.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef DEBUG
# include <stdio.h>
#endif
#include <errno.h>
#include <System.h>
#include "Panel/applet.h"
#define N_(string) string


/* Volume */
/* types */
typedef struct _PanelApplet
{
	PanelAppletHelper * helper;

	char const * device;
	char const * control;
#if defined(AUDIO_MIXER_DEVINFO) /* audioio */
	int fd;
	int mix;
	int outputs;
#elif defined(SND_LIB_MAJOR) /* Alsa */
	snd_mixer_t * mixer;
	snd_mixer_elem_t * mixer_elem;
	long mixer_elem_max;
#else /* OSS */
	int fd;
#endif
	guint source;

	/* widgets */
	GtkWidget * widget;
	GtkWidget * button;
	GtkWidget * progress;
} Volume;


/* private */
/* prototypes */
static Volume * _volume_init(PanelAppletHelper * helper, GtkWidget ** widget);
#if GTK_CHECK_VERSION(2, 12, 0)
static void _volume_destroy(Volume * volume);
#endif

#if GTK_CHECK_VERSION(2, 12, 0)
static Volume * _volume_new(PanelAppletHelper * helper);
static void _volume_delete(Volume * volume);

/* accessors */
static gdouble _volume_get(Volume * volume);
static int _volume_set(Volume * volume, gdouble value);
# ifdef AUDIO_MIXER_DEVINFO
static int _volume_match(Volume * volume, mixer_devinfo_t * md);
# endif

/* callbacks */
static void _volume_on_value_changed(gpointer data);
static gboolean _volume_on_volume_timeout(gpointer data);
#endif


/* public */
/* variables */
PanelAppletDefinition applet =
{
	N_("Volume"),
	"stock_volume",
	NULL,
	_volume_init,
#if GTK_CHECK_VERSION(2, 12, 0)
	_volume_destroy,
#else
	NULL,
#endif
	NULL,
	FALSE,
	TRUE
};


/* functions */
/* volume_init */
static Volume * _volume_init(PanelAppletHelper * helper, GtkWidget ** widget)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	Volume * volume;
	GtkIconSize iconsize;
	GtkWidget * vbox;

	if((volume = _volume_new(helper)) == NULL)
		return NULL;
	volume->helper = helper;
	volume->button = NULL;
	volume->progress = NULL;
	iconsize = panel_window_get_icon_size(helper->window);
	if(panel_window_get_type(helper->window)
			== PANEL_WINDOW_TYPE_NOTIFICATION)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
#else
		vbox = gtk_vbox_new(FALSE, 4);
#endif
		volume->widget = gtk_image_new_from_icon_name(
				"stock_volume-med", iconsize);
		gtk_box_pack_start(GTK_BOX(vbox), volume->widget, TRUE, TRUE,
				0);
		volume->progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION(3, 0, 0)
		gtk_progress_bar_set_show_text(
				GTK_PROGRESS_BAR(volume->progress), TRUE);
#endif
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(volume->progress),
				"");
		gtk_box_pack_start(GTK_BOX(vbox), volume->progress, TRUE, TRUE,
				0);
		volume->widget = vbox;
	}
	else
	{
		volume->button = gtk_volume_button_new();
		/* FIXME doesn't like registered sizes */
		g_object_set(volume->button, "size", iconsize, NULL);
		g_signal_connect_swapped(volume->button, "value-changed",
				G_CALLBACK(_volume_on_value_changed), volume);
		volume->widget = volume->button;
	}
	_volume_on_volume_timeout(volume);
	gtk_widget_show_all(volume->widget);
	*widget = volume->widget;
	return volume;
#else
	return NULL;
#endif
}


#if GTK_CHECK_VERSION(2, 12, 0)
/* volume_destroy */
static void _volume_destroy(Volume * volume)
{
	gtk_widget_destroy(volume->widget);
	_volume_delete(volume);
}


/* volume_new */
static Volume * _volume_new(PanelAppletHelper * helper)
{
	const int timeout = 500;
	Volume * volume;
#if defined(AUDIO_MIXER_DEVINFO)
	int i;
	mixer_devinfo_t md;
#elif defined(SND_LIB_MAJOR)
	int err;
	snd_mixer_elem_t * elem = NULL;
	long min;
#endif

	if((volume = malloc(sizeof(*volume))) == NULL)
	{
		error_set("%s: %s", applet.name, strerror(errno));
		return NULL;
	}
	volume->helper = helper;
	volume->device = helper->config_get(helper->panel, "volume", "device");
	volume->control = helper->config_get(helper->panel, "volume",
			"control");
	volume->source = 0;
#if defined(AUDIO_MIXER_DEVINFO)
	if(volume->device == NULL)
		volume->device = "/dev/mixer";
	volume->mix = -1;
	volume->outputs = -1;
	if((volume->fd = open(volume->device, O_RDWR)) < 0)
	{
		error_set("%s: %s: %s", applet.name, volume->device,
				strerror(errno));
		helper->error(NULL, error_get(NULL), 1);
		return volume;
	}
	for(i = 0; volume->outputs == -1 || volume->mix == -1; i++)
	{
		md.index = i;
		if(ioctl(volume->fd, AUDIO_MIXER_DEVINFO, &md) < 0)
			break;
		if(md.type != AUDIO_MIXER_CLASS)
			continue;
		if(strcmp(md.label.name, AudioCoutputs) == 0)
			volume->outputs = i;
		else if(strcmp(md.label.name, "mix") == 0)
			volume->mix = i;
	}
	volume->source = g_timeout_add(timeout, _volume_on_volume_timeout,
			volume);
#elif defined(SND_LIB_MAJOR)
	if(volume->device == NULL)
		volume->device = "hw:0";
	if(volume->control == NULL)
		volume->control = "Master";
	if((err = snd_mixer_open(&volume->mixer, 0)) != 0
			|| (err = snd_mixer_attach(volume->mixer,
				       	volume->device)) != 0
			|| (err = snd_mixer_selem_register(volume->mixer, NULL,
				       	NULL)) != 0
			|| (err = snd_mixer_load(volume->mixer)) != 0)
		error_set("%s: %s: %s", applet.name, volume->device,
				snd_strerror(err));
	else
		for(elem = snd_mixer_first_elem(volume->mixer); elem != NULL;
				elem = snd_mixer_elem_next(elem))
			if(strcmp(snd_mixer_selem_get_name(elem),
						volume->control) == 0)
				break;
	if((volume->mixer_elem = elem) != NULL
			&& snd_mixer_selem_get_playback_volume_range(
				volume->mixer_elem, &min,
			       	&volume->mixer_elem_max) == 0)
		volume->source = g_timeout_add(timeout,
				_volume_on_volume_timeout, volume);
	else
		volume->mixer_elem = NULL;
#else
	if(volume->device == NULL)
		volume->device = "/dev/mixer";
	if((volume->fd = open(volume->device, O_RDWR)) < 0)
	{
		error_set("%s: %s: %s", applet.name, volume->device,
				strerror(errno));
		helper->error(NULL, error_get(NULL), 1);
	}
	else
		volume->source = g_timeout_add(timeout,
				_volume_on_volume_timeout, volume);
#endif
	return volume;
}


/* volume_delete */
static void _volume_delete(Volume * volume)
{
	if(volume->source != 0)
		g_source_remove(volume->source);
#if defined(AUDIO_MIXER_DEVINFO)
	if(volume->fd >= 0 && close(volume->fd) != 0)
	{
		error_set("%s: %s: %s", applet.name, volume->device,
				strerror(errno));
		volume->helper->error(NULL, error_get(NULL), 1);
	}
#elif defined(SND_LIB_MAJOR)
	if(volume->mixer != NULL)
		snd_mixer_close(volume->mixer);
#else /* XXX equivalent for now */
	if(volume->fd >= 0 && close(volume->fd) != 0)
	{
		error_set("%s: %s: %s", applet.name, volume->device,
				strerror(errno));
		volume->helper->error(NULL, error_get(NULL), 1);
	}
#endif
	free(volume);
}


/* accessors */
/* volume_get */
static gdouble _volume_get(Volume * volume)
{
	PanelAppletHelper * helper = volume->helper;
	gdouble ret = -1.0;
#if defined(AUDIO_MIXER_DEVINFO)
	mixer_devinfo_t md;
	mixer_ctrl_t mc;
	int i;

	if(volume->fd < 0)
		return ret;
	if(volume->outputs < 0 && volume->mix < 0)
		return ret;
	for(i = 0;; i++)
	{
		md.index = i;
		if(ioctl(volume->fd, AUDIO_MIXER_DEVINFO, &md) < 0)
		{
			error_set("%s: %s: %s", applet.name,
					"AUDIO_MIXER_DEVINFO", strerror(errno));
			helper->error(NULL, error_get(NULL), 1);
			close(volume->fd);
			volume->fd = -1;
			break;
		}
		if(_volume_match(volume, &md) != 1)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_VALUE;
		mc.un.value.num_channels = md.un.v.num_channels;
		if(ioctl(volume->fd, AUDIO_MIXER_READ, &mc) < 0)
		{
			error_set("%s: %s: %s", applet.name, "AUDIO_MIXER_READ",
					strerror(errno));
			helper->error(NULL, error_get(NULL), 1);
		}
		else
			ret = mc.un.value.level[0] / 255.0;
		break;
	}
#elif defined(SND_LIB_MAJOR)
	long value;

	if(volume->mixer_elem == NULL)
		return ret;
	if(snd_mixer_selem_get_playback_volume(volume->mixer_elem,
				SND_MIXER_SCHN_FRONT_LEFT, &value) != 0)
		return ret;
	ret = value;
	ret /= volume->mixer_elem_max;
#elif !defined(__APPLE__)
	int value;

	if(volume->fd < 0)
		return ret;
	if(ioctl(volume->fd, MIXER_READ(SOUND_MIXER_VOLUME), &value) < 0)
	{
		error_set("%s: %s: %s", applet.name, "MIXER_READ",
				strerror(errno));
		helper->error(NULL, error_get(NULL), 1);
		close(volume->fd);
		volume->fd = -1;
	}
	else
		ret = ((value & 0xff) + ((value & 0xff00) >> 8)) / 200.0;
#endif
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() => %f\n", __func__, ret);
#endif
	return ret;
}


/* volume_set */
int _volume_set(Volume * volume, gdouble value)
{
	int ret = 0;
	PanelAppletHelper * helper = volume->helper;
#if defined(AUDIO_MIXER_DEVINFO)
	mixer_devinfo_t md;
	mixer_ctrl_t mc;
	int i;
	int j;

# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%f)\n", __func__, value);
# endif
	if(volume->fd < 0)
		return 1;
	if(volume->outputs < 0 && volume->mix < 0)
		return 1;
	for(i = 0;; i++)
	{
		md.index = i;
		if(ioctl(volume->fd, AUDIO_MIXER_DEVINFO, &md) < 0)
			break;
		/* FIXME use volume->control */
		if(_volume_match(volume, &md) != 1)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_VALUE;
		mc.un.value.num_channels = md.un.v.num_channels;
		mc.un.value.level[0] = value * 255;
		for(j = 1; j < mc.un.value.num_channels; j++) /* XXX overflow */
			mc.un.value.level[j] = mc.un.value.level[0];
		if(ioctl(volume->fd, AUDIO_MIXER_WRITE, &mc) < 0)
		{
			error_set("%s: %s: %s", applet.name,
					"AUDIO_MIXER_WRITE", strerror(errno));
			ret |= helper->error(NULL, error_get(NULL), 1);
		}
		break;
	}
#elif defined(SND_LIB_MAJOR)
	long v = value * volume->mixer_elem_max;

	if(volume->mixer_elem == NULL)
		return 0;
	snd_mixer_selem_set_playback_volume_all(volume->mixer_elem, v);
#elif !defined(__APPLE__)
	int v = value * 100;

	if(volume->fd < 0)
		return 1;
	v |= v << 8;
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%f) 0x%04x\n", __func__, value, v);
# endif
	if(ioctl(volume->fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &v) < 0)
	{
		error_set("%s: %s: %s", applet.name, "MIXER_WRITE",
				strerror(errno));
		ret |= helper->error(NULL, error_get(NULL), 1);
	}
#endif
	return ret;
}


#if defined(AUDIO_MIXER_DEVINFO)
/* volume_match */
static int _volume_match(Volume * volume, mixer_devinfo_t * md)
{
	if(md->type != AUDIO_MIXER_VALUE)
		return 0;
	if(md->mixer_class == volume->mix /* mix.master */
			&& strcmp(md->label.name, "master") == 0)
		return 1;
	if(md->mixer_class == volume->outputs /* outputs.lineout */
			&& strcmp(md->label.name, "lineout") == 0)
		return 1;
	if(md->mixer_class == volume->outputs /* outputs.master */
			|| strcmp(md->label.name, "master") == 0)
		return 1;
	return 0;
}
#endif


/* callbacks */
/* volume_on_value_changed */
static void _volume_on_value_changed(gpointer data)
{
	Volume * volume = data;
	gdouble value;

	value = gtk_scale_button_get_value(GTK_SCALE_BUTTON(volume->button));
	_volume_set(volume, value);
}


/* volume_on_volume_timeout */
static gboolean _volume_on_volume_timeout(gpointer data)
{
	Volume * volume = data;
	gdouble value;

	if((value = _volume_get(volume)) < 0.0)
	{
		volume->source = 0;
		return FALSE;
	}
	if(volume->button != NULL)
		gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume->button),
				value);
	if(volume->progress != NULL)
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(
					volume->progress), value);
	return TRUE;
}
#endif
