TARGETS	= $(OBJDIR)battery.so $(OBJDIR)bluetooth.so $(OBJDIR)clock.so $(OBJDIR)close.so $(OBJDIR)cpu.so $(OBJDIR)cpufreq.so $(OBJDIR)desktop.so $(OBJDIR)embed.so $(OBJDIR)gps.so $(OBJDIR)gsm.so $(OBJDIR)lock.so $(OBJDIR)logout.so $(OBJDIR)menu.so $(OBJDIR)memory.so $(OBJDIR)mixer.so $(OBJDIR)mpd.so $(OBJDIR)network.so $(OBJDIR)pager.so $(OBJDIR)phone.so $(OBJDIR)rotate.so $(OBJDIR)separator.so $(OBJDIR)spacer.so $(OBJDIR)swap.so $(OBJDIR)systray.so $(OBJDIR)tasks.so $(OBJDIR)template.so $(OBJDIR)title.so $(OBJDIR)usb.so $(OBJDIR)user.so $(OBJDIR)volume.so $(OBJDIR)wpa_supplicant.so
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
LIBDIR	= $(PREFIX)/lib
CC	= cc
CPPFLAGSF= -I ../../include
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libSystem gtk+-2.0` -fPIC
CFLAGS	= -Wall -g -O2 -pedantic
LDFLAGSF= `pkg-config --libs libSystem gtk+-2.0`
AR	= ar
RANLIB	= ranlib
CCSHARED= $(CC) -shared
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

battery_OBJS = $(OBJDIR)battery.o
battery_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
battery_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)battery.so: $(battery_OBJS)
	$(CCSHARED) -o $(OBJDIR)battery.so $(battery_OBJS) $(battery_LDFLAGS)

bluetooth_OBJS = $(OBJDIR)bluetooth.o
bluetooth_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
bluetooth_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)bluetooth.so: $(bluetooth_OBJS)
	$(CCSHARED) -o $(OBJDIR)bluetooth.so $(bluetooth_OBJS) $(bluetooth_LDFLAGS)

clock_OBJS = $(OBJDIR)clock.o
clock_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
clock_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)clock.so: $(clock_OBJS)
	$(CCSHARED) -o $(OBJDIR)clock.so $(clock_OBJS) $(clock_LDFLAGS)

close_OBJS = $(OBJDIR)close.o
close_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11`
close_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11`

$(OBJDIR)close.so: $(close_OBJS)
	$(CCSHARED) -o $(OBJDIR)close.so $(close_OBJS) $(close_LDFLAGS)

cpu_OBJS = $(OBJDIR)cpu.o
cpu_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
cpu_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)cpu.so: $(cpu_OBJS)
	$(CCSHARED) -o $(OBJDIR)cpu.so $(cpu_OBJS) $(cpu_LDFLAGS)

cpufreq_OBJS = $(OBJDIR)cpufreq.o
cpufreq_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
cpufreq_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)cpufreq.so: $(cpufreq_OBJS)
	$(CCSHARED) -o $(OBJDIR)cpufreq.so $(cpufreq_OBJS) $(cpufreq_LDFLAGS)

desktop_OBJS = $(OBJDIR)desktop.o
desktop_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11`
desktop_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11`

$(OBJDIR)desktop.so: $(desktop_OBJS)
	$(CCSHARED) -o $(OBJDIR)desktop.so $(desktop_OBJS) $(desktop_LDFLAGS)

embed_OBJS = $(OBJDIR)embed.o
embed_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags libDesktop`
embed_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs libDesktop`

$(OBJDIR)embed.so: $(embed_OBJS)
	$(CCSHARED) -o $(OBJDIR)embed.so $(embed_OBJS) $(embed_LDFLAGS)

gps_OBJS = $(OBJDIR)gps.o
gps_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
gps_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)gps.so: $(gps_OBJS)
	$(CCSHARED) -o $(OBJDIR)gps.so $(gps_OBJS) $(gps_LDFLAGS)

gsm_OBJS = $(OBJDIR)gsm.o
gsm_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
gsm_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)gsm.so: $(gsm_OBJS)
	$(CCSHARED) -o $(OBJDIR)gsm.so $(gsm_OBJS) $(gsm_LDFLAGS)

lock_OBJS = $(OBJDIR)lock.o
lock_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
lock_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)lock.so: $(lock_OBJS)
	$(CCSHARED) -o $(OBJDIR)lock.so $(lock_OBJS) $(lock_LDFLAGS)

logout_OBJS = $(OBJDIR)logout.o
logout_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
logout_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)logout.so: $(logout_OBJS)
	$(CCSHARED) -o $(OBJDIR)logout.so $(logout_OBJS) $(logout_LDFLAGS)

menu_OBJS = $(OBJDIR)menu.o
menu_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS)
menu_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)menu.so: $(menu_OBJS)
	$(CCSHARED) -o $(OBJDIR)menu.so $(menu_OBJS) $(menu_LDFLAGS)

memory_OBJS = $(OBJDIR)memory.o
memory_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
memory_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)memory.so: $(memory_OBJS)
	$(CCSHARED) -o $(OBJDIR)memory.so $(memory_OBJS) $(memory_LDFLAGS)

mixer_OBJS = $(OBJDIR)mixer.o
mixer_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
mixer_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)mixer.so: $(mixer_OBJS)
	$(CCSHARED) -o $(OBJDIR)mixer.so $(mixer_OBJS) $(mixer_LDFLAGS)

mpd_OBJS = $(OBJDIR)mpd.o
mpd_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
mpd_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)mpd.so: $(mpd_OBJS)
	$(CCSHARED) -o $(OBJDIR)mpd.so $(mpd_OBJS) $(mpd_LDFLAGS)

network_OBJS = $(OBJDIR)network.o
network_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
network_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)network.so: $(network_OBJS)
	$(CCSHARED) -o $(OBJDIR)network.so $(network_OBJS) $(network_LDFLAGS)

pager_OBJS = $(OBJDIR)pager.o
pager_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11`
pager_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11`

$(OBJDIR)pager.so: $(pager_OBJS)
	$(CCSHARED) -o $(OBJDIR)pager.so $(pager_OBJS) $(pager_LDFLAGS)

phone_OBJS = $(OBJDIR)phone.o
phone_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags libDesktop`
phone_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs libDesktop`

$(OBJDIR)phone.so: $(phone_OBJS)
	$(CCSHARED) -o $(OBJDIR)phone.so $(phone_OBJS) $(phone_LDFLAGS)

rotate_OBJS = $(OBJDIR)rotate.o
rotate_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
rotate_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)rotate.so: $(rotate_OBJS)
	$(CCSHARED) -o $(OBJDIR)rotate.so $(rotate_OBJS) $(rotate_LDFLAGS)

separator_OBJS = $(OBJDIR)separator.o
separator_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
separator_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)separator.so: $(separator_OBJS)
	$(CCSHARED) -o $(OBJDIR)separator.so $(separator_OBJS) $(separator_LDFLAGS)

spacer_OBJS = $(OBJDIR)spacer.o
spacer_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
spacer_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)spacer.so: $(spacer_OBJS)
	$(CCSHARED) -o $(OBJDIR)spacer.so $(spacer_OBJS) $(spacer_LDFLAGS)

swap_OBJS = $(OBJDIR)swap.o
swap_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
swap_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)swap.so: $(swap_OBJS)
	$(CCSHARED) -o $(OBJDIR)swap.so $(swap_OBJS) $(swap_LDFLAGS)

systray_OBJS = $(OBJDIR)systray.o
systray_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11`
systray_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11`

$(OBJDIR)systray.so: $(systray_OBJS)
	$(CCSHARED) -o $(OBJDIR)systray.so $(systray_OBJS) $(systray_LDFLAGS)

tasks_OBJS = $(OBJDIR)tasks.o
tasks_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11`
tasks_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11`

$(OBJDIR)tasks.so: $(tasks_OBJS)
	$(CCSHARED) -o $(OBJDIR)tasks.so $(tasks_OBJS) $(tasks_LDFLAGS)

template_OBJS = $(OBJDIR)template.o
template_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
template_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)template.so: $(template_OBJS)
	$(CCSHARED) -o $(OBJDIR)template.so $(template_OBJS) $(template_LDFLAGS)

title_OBJS = $(OBJDIR)title.o
title_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags x11`
title_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs x11`

$(OBJDIR)title.so: $(title_OBJS)
	$(CCSHARED) -o $(OBJDIR)title.so $(title_OBJS) $(title_LDFLAGS)

usb_OBJS = $(OBJDIR)usb.o
usb_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
usb_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)usb.so: $(usb_OBJS)
	$(CCSHARED) -o $(OBJDIR)usb.so $(usb_OBJS) $(usb_LDFLAGS)

user_OBJS = $(OBJDIR)user.o
user_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
user_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)user.so: $(user_OBJS)
	$(CCSHARED) -o $(OBJDIR)user.so $(user_OBJS) $(user_LDFLAGS)

volume_OBJS = $(OBJDIR)volume.o
volume_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) `pkg-config --cflags alsa` $(CFLAGSF) $(CFLAGS)
volume_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs alsa`

$(OBJDIR)volume.so: $(volume_OBJS)
	$(CCSHARED) -o $(OBJDIR)volume.so $(volume_OBJS) $(volume_LDFLAGS)

wpa_supplicant_OBJS = $(OBJDIR)wpa_supplicant.o
wpa_supplicant_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
wpa_supplicant_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)wpa_supplicant.so: $(wpa_supplicant_OBJS)
	$(CCSHARED) -o $(OBJDIR)wpa_supplicant.so $(wpa_supplicant_OBJS) $(wpa_supplicant_LDFLAGS)

$(OBJDIR)battery.o: battery.c ../../include/Panel.h
	$(CC) $(battery_CFLAGS) -o $(OBJDIR)battery.o -c battery.c

$(OBJDIR)bluetooth.o: bluetooth.c ../../include/Panel.h
	$(CC) $(bluetooth_CFLAGS) -o $(OBJDIR)bluetooth.o -c bluetooth.c

$(OBJDIR)clock.o: clock.c ../../include/Panel.h
	$(CC) $(clock_CFLAGS) -o $(OBJDIR)clock.o -c clock.c

$(OBJDIR)close.o: close.c ../../include/Panel.h
	$(CC) $(close_CFLAGS) -o $(OBJDIR)close.o -c close.c

$(OBJDIR)cpu.o: cpu.c ../../include/Panel.h
	$(CC) $(cpu_CFLAGS) -o $(OBJDIR)cpu.o -c cpu.c

$(OBJDIR)cpufreq.o: cpufreq.c ../../include/Panel.h
	$(CC) $(cpufreq_CFLAGS) -o $(OBJDIR)cpufreq.o -c cpufreq.c

$(OBJDIR)desktop.o: desktop.c ../../include/Panel.h
	$(CC) $(desktop_CFLAGS) -o $(OBJDIR)desktop.o -c desktop.c

$(OBJDIR)embed.o: embed.c ../../include/Panel.h
	$(CC) $(embed_CFLAGS) -o $(OBJDIR)embed.o -c embed.c

$(OBJDIR)gps.o: gps.c ../../include/Panel.h
	$(CC) $(gps_CFLAGS) -o $(OBJDIR)gps.o -c gps.c

$(OBJDIR)gsm.o: gsm.c ../../include/Panel.h
	$(CC) $(gsm_CFLAGS) -o $(OBJDIR)gsm.o -c gsm.c

$(OBJDIR)lock.o: lock.c ../../include/Panel.h
	$(CC) $(lock_CFLAGS) -o $(OBJDIR)lock.o -c lock.c

$(OBJDIR)logout.o: logout.c ../../include/Panel.h
	$(CC) $(logout_CFLAGS) -o $(OBJDIR)logout.o -c logout.c

$(OBJDIR)menu.o: menu.c ../../include/Panel.h ../../config.h
	$(CC) $(menu_CFLAGS) -o $(OBJDIR)menu.o -c menu.c

$(OBJDIR)memory.o: memory.c ../../include/Panel.h
	$(CC) $(memory_CFLAGS) -o $(OBJDIR)memory.o -c memory.c

$(OBJDIR)mixer.o: mixer.c ../../include/Panel.h
	$(CC) $(mixer_CFLAGS) -o $(OBJDIR)mixer.o -c mixer.c

$(OBJDIR)mpd.o: mpd.c ../../include/Panel.h
	$(CC) $(mpd_CFLAGS) -o $(OBJDIR)mpd.o -c mpd.c

$(OBJDIR)network.o: network.c ../../include/Panel.h
	$(CC) $(network_CFLAGS) -o $(OBJDIR)network.o -c network.c

$(OBJDIR)pager.o: pager.c ../../include/Panel.h
	$(CC) $(pager_CFLAGS) -o $(OBJDIR)pager.o -c pager.c

$(OBJDIR)phone.o: phone.c ../../include/Panel.h
	$(CC) $(phone_CFLAGS) -o $(OBJDIR)phone.o -c phone.c

$(OBJDIR)rotate.o: rotate.c ../../include/Panel.h
	$(CC) $(rotate_CFLAGS) -o $(OBJDIR)rotate.o -c rotate.c

$(OBJDIR)separator.o: separator.c ../../include/Panel.h
	$(CC) $(separator_CFLAGS) -o $(OBJDIR)separator.o -c separator.c

$(OBJDIR)spacer.o: spacer.c ../../include/Panel.h
	$(CC) $(spacer_CFLAGS) -o $(OBJDIR)spacer.o -c spacer.c

$(OBJDIR)swap.o: swap.c ../../include/Panel.h
	$(CC) $(swap_CFLAGS) -o $(OBJDIR)swap.o -c swap.c

$(OBJDIR)systray.o: systray.c ../../include/Panel.h
	$(CC) $(systray_CFLAGS) -o $(OBJDIR)systray.o -c systray.c

$(OBJDIR)tasks.o: tasks.c ../../include/Panel.h tasks.atoms
	$(CC) $(tasks_CFLAGS) -o $(OBJDIR)tasks.o -c tasks.c

$(OBJDIR)template.o: template.c ../../include/Panel.h
	$(CC) $(template_CFLAGS) -o $(OBJDIR)template.o -c template.c

$(OBJDIR)title.o: title.c ../../include/Panel.h
	$(CC) $(title_CFLAGS) -o $(OBJDIR)title.o -c title.c

$(OBJDIR)usb.o: usb.c ../../include/Panel.h
	$(CC) $(usb_CFLAGS) -o $(OBJDIR)usb.o -c usb.c

$(OBJDIR)user.o: user.c ../../include/Panel.h
	$(CC) $(user_CFLAGS) -o $(OBJDIR)user.o -c user.c

$(OBJDIR)volume.o: volume.c ../../include/Panel.h
	$(CC) $(volume_CFLAGS) -o $(OBJDIR)volume.o -c volume.c

$(OBJDIR)wpa_supplicant.o: wpa_supplicant.c ../../include/Panel.h
	$(CC) $(wpa_supplicant_CFLAGS) -o $(OBJDIR)wpa_supplicant.o -c wpa_supplicant.c

clean:
	$(RM) -- $(battery_OBJS) $(bluetooth_OBJS) $(clock_OBJS) $(close_OBJS) $(cpu_OBJS) $(cpufreq_OBJS) $(desktop_OBJS) $(embed_OBJS) $(gps_OBJS) $(gsm_OBJS) $(lock_OBJS) $(logout_OBJS) $(menu_OBJS) $(memory_OBJS) $(mixer_OBJS) $(mpd_OBJS) $(network_OBJS) $(pager_OBJS) $(phone_OBJS) $(rotate_OBJS) $(separator_OBJS) $(spacer_OBJS) $(swap_OBJS) $(systray_OBJS) $(tasks_OBJS) $(template_OBJS) $(title_OBJS) $(usb_OBJS) $(user_OBJS) $(volume_OBJS) $(wpa_supplicant_OBJS)

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)battery.so $(DESTDIR)$(LIBDIR)/Panel/applets/battery.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)bluetooth.so $(DESTDIR)$(LIBDIR)/Panel/applets/bluetooth.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)clock.so $(DESTDIR)$(LIBDIR)/Panel/applets/clock.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)close.so $(DESTDIR)$(LIBDIR)/Panel/applets/close.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)cpu.so $(DESTDIR)$(LIBDIR)/Panel/applets/cpu.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)cpufreq.so $(DESTDIR)$(LIBDIR)/Panel/applets/cpufreq.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)desktop.so $(DESTDIR)$(LIBDIR)/Panel/applets/desktop.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)embed.so $(DESTDIR)$(LIBDIR)/Panel/applets/embed.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)gps.so $(DESTDIR)$(LIBDIR)/Panel/applets/gps.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)gsm.so $(DESTDIR)$(LIBDIR)/Panel/applets/gsm.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)lock.so $(DESTDIR)$(LIBDIR)/Panel/applets/lock.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)logout.so $(DESTDIR)$(LIBDIR)/Panel/applets/logout.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)menu.so $(DESTDIR)$(LIBDIR)/Panel/applets/menu.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)memory.so $(DESTDIR)$(LIBDIR)/Panel/applets/memory.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)mixer.so $(DESTDIR)$(LIBDIR)/Panel/applets/mixer.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)network.so $(DESTDIR)$(LIBDIR)/Panel/applets/network.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)pager.so $(DESTDIR)$(LIBDIR)/Panel/applets/pager.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)phone.so $(DESTDIR)$(LIBDIR)/Panel/applets/phone.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)rotate.so $(DESTDIR)$(LIBDIR)/Panel/applets/rotate.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)separator.so $(DESTDIR)$(LIBDIR)/Panel/applets/separator.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)spacer.so $(DESTDIR)$(LIBDIR)/Panel/applets/spacer.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)swap.so $(DESTDIR)$(LIBDIR)/Panel/applets/swap.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)systray.so $(DESTDIR)$(LIBDIR)/Panel/applets/systray.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)tasks.so $(DESTDIR)$(LIBDIR)/Panel/applets/tasks.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)title.so $(DESTDIR)$(LIBDIR)/Panel/applets/title.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)usb.so $(DESTDIR)$(LIBDIR)/Panel/applets/usb.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)user.so $(DESTDIR)$(LIBDIR)/Panel/applets/user.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)volume.so $(DESTDIR)$(LIBDIR)/Panel/applets/volume.so
	$(MKDIR) $(DESTDIR)$(LIBDIR)/Panel/applets
	$(INSTALL) -m 0755 $(OBJDIR)wpa_supplicant.so $(DESTDIR)$(LIBDIR)/Panel/applets/wpa_supplicant.so

uninstall:
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/battery.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/bluetooth.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/clock.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/close.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/cpu.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/cpufreq.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/desktop.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/embed.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/gps.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/gsm.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/lock.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/logout.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/menu.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/memory.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/mixer.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/network.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/pager.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/phone.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/rotate.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/separator.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/spacer.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/swap.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/systray.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/tasks.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/title.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/usb.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/user.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/volume.so
	$(RM) -- $(DESTDIR)$(LIBDIR)/Panel/applets/wpa_supplicant.so

.PHONY: all clean distclean install uninstall
