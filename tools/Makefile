TARGETS	= panel-embed panel-message panel-notify panel-test wifibrowser
PREFIX	= /usr/local
DESTDIR	=
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
CC	= cc
CPPFLAGSF=
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libDesktop`
CFLAGS	= -Wall -g -O2
LDFLAGSF= `pkg-config --libs libDesktop`
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

panel-embed_OBJS = embed.o
panel-embed_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS)
panel-embed_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

panel-embed: $(panel-embed_OBJS)
	$(CC) -o panel-embed $(panel-embed_OBJS) $(panel-embed_LDFLAGS)

panel-message_OBJS = message.o
panel-message_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS)
panel-message_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

panel-message: $(panel-message_OBJS)
	$(CC) -o panel-message $(panel-message_OBJS) $(panel-message_LDFLAGS)

panel-notify_OBJS = notify.o
panel-notify_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS)
panel-notify_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L../src -Wl,-rpath,$(PREFIX)/lib -lPanel

panel-notify: $(panel-notify_OBJS)
	$(CC) -o panel-notify $(panel-notify_OBJS) $(panel-notify_LDFLAGS)

panel-test_OBJS = test.o
panel-test_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS)
panel-test_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -L../src -Wl,-rpath,$(PREFIX)/lib -lPanel

panel-test: $(panel-test_OBJS)
	$(CC) -o panel-test $(panel-test_OBJS) $(panel-test_LDFLAGS)

wifibrowser_OBJS = wifibrowser.o
wifibrowser_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -I ../include $(CFLAGSF) $(CFLAGS)
wifibrowser_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

wifibrowser: $(wifibrowser_OBJS)
	$(CC) -o wifibrowser $(wifibrowser_OBJS) $(wifibrowser_LDFLAGS)

embed.o: embed.c ../include/Panel.h ../config.h
	$(CC) $(panel-embed_CFLAGS) -c embed.c

message.o: message.c ../include/Panel.h ../config.h
	$(CC) $(panel-message_CFLAGS) -c message.c

notify.o: notify.c helper.c ../src/helper.c ../src/panel.h ../config.h
	$(CC) $(panel-notify_CFLAGS) -c notify.c

test.o: test.c helper.c ../src/helper.c ../src/panel.h ../config.h
	$(CC) $(panel-test_CFLAGS) -c test.c

wifibrowser.o: wifibrowser.c ../src/applets/wpa_supplicant.c ../config.h
	$(CC) $(wifibrowser_CFLAGS) -c wifibrowser.c

clean:
	$(RM) -- $(panel-embed_OBJS) $(panel-message_OBJS) $(panel-notify_OBJS) $(panel-test_OBJS) $(wifibrowser_OBJS)

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 panel-embed $(DESTDIR)$(BINDIR)/panel-embed
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 panel-message $(DESTDIR)$(BINDIR)/panel-message
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 panel-notify $(DESTDIR)$(BINDIR)/panel-notify
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 wifibrowser $(DESTDIR)$(BINDIR)/wifibrowser

uninstall:
	$(RM) -- $(DESTDIR)$(BINDIR)/panel-embed
	$(RM) -- $(DESTDIR)$(BINDIR)/panel-message
	$(RM) -- $(DESTDIR)$(BINDIR)/panel-notify
	$(RM) -- $(DESTDIR)$(BINDIR)/wifibrowser

.PHONY: all clean distclean install uninstall
