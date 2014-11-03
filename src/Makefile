SUBDIRS	= applets
TARGETS	= $(OBJDIR)libPanel.a $(OBJDIR)libPanel.so.0.0 libPanel.so.0 libPanel.so $(OBJDIR)panel $(OBJDIR)panelctl $(OBJDIR)run
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
LIBDIR	= $(PREFIX)/lib
CC	= cc
CPPFLAGSF=
CPPFLAGS=
CFLAGSF	= -W
CFLAGS	= -Wall -g -O2 -pedantic
AR	= ar
RANLIB	= ranlib
CCSHARED= $(CC) -shared
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: subdirs $(TARGETS)

subdirs:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE)) || exit; done

libPanel_OBJS = $(OBJDIR)panel.o $(OBJDIR)window.o
libPanel_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS) `pkg-config --cflags libDesktop` -fPIC
libPanel_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs libDesktop` -lintl

$(OBJDIR)libPanel.a: $(libPanel_OBJS)
	$(AR) -rc $(OBJDIR)libPanel.a $(libPanel_OBJS)
	$(RANLIB) $(OBJDIR)libPanel.a

$(OBJDIR)libPanel.so.0.0 libPanel.so.0 libPanel.so: $(libPanel_OBJS)
	$(CCSHARED) -o $(OBJDIR)libPanel.so.0.0 -Wl,-soname,libPanel.so.0 $(libPanel_OBJS) $(libPanel_LDFLAGS)
	$(LN) -s -- libPanel.so.0.0 $(OBJDIR)libPanel.so.0
	$(LN) -s -- libPanel.so.0.0 $(OBJDIR)libPanel.so

panel_OBJS = $(OBJDIR)main.o
panel_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS) `pkg-config --cflags libDesktop`
panel_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs libDesktop` -lintl -L. -L$(OBJDIR). -Wl,-rpath,$(LIBDIR) -lPanel

$(OBJDIR)panel: $(panel_OBJS) $(OBJDIR)libPanel.a
	$(CC) -o $(OBJDIR)panel $(panel_OBJS) $(panel_LDFLAGS)

panelctl_OBJS = $(OBJDIR)panelctl.o
panelctl_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS) `pkg-config --cflags libDesktop`
panelctl_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs libDesktop` -lintl

$(OBJDIR)panelctl: $(panelctl_OBJS)
	$(CC) -o $(OBJDIR)panelctl $(panelctl_OBJS) $(panelctl_LDFLAGS)

run_OBJS = $(OBJDIR)run.o
run_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) -D PREFIX=\"$(PREFIX)\" $(CFLAGSF) $(CFLAGS) `pkg-config --cflags libSystem gtk+-2.0`
run_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) `pkg-config --libs libSystem gtk+-2.0` -lintl

$(OBJDIR)run: $(run_OBJS)
	$(CC) -o $(OBJDIR)run $(run_OBJS) $(run_LDFLAGS)

$(OBJDIR)panel.o: panel.c panel.h window.h ../include/Panel.h helper.c ../config.h
	$(CC) $(libPanel_CFLAGS) -o $(OBJDIR)panel.o -c panel.c

$(OBJDIR)window.o: window.c ../include/Panel.h panel.h window.h
	$(CC) $(libPanel_CFLAGS) -o $(OBJDIR)window.o -c window.c

$(OBJDIR)main.o: main.c ../include/Panel.h panel.h ../config.h
	$(CC) $(panel_CFLAGS) -o $(OBJDIR)main.o -c main.c

$(OBJDIR)panelctl.o: panelctl.c ../include/Panel.h panel.h ../config.h
	$(CC) $(panelctl_CFLAGS) -o $(OBJDIR)panelctl.o -c panelctl.c

$(OBJDIR)run.o: run.c
	$(CC) $(run_CFLAGS) -o $(OBJDIR)run.o -c run.c

clean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) clean) || exit; done
	$(RM) -- $(libPanel_OBJS) $(panel_OBJS) $(panelctl_OBJS) $(run_OBJS)

distclean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) distclean) || exit; done
	$(RM) -- $(libPanel_OBJS) $(panel_OBJS) $(panelctl_OBJS) $(run_OBJS)
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) install) || exit; done
	$(MKDIR) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 $(OBJDIR)libPanel.a $(DESTDIR)$(LIBDIR)/libPanel.a
	$(INSTALL) -m 0755 $(OBJDIR)libPanel.so.0.0 $(DESTDIR)$(LIBDIR)/libPanel.so.0.0
	$(LN) -s -- libPanel.so.0.0 $(DESTDIR)$(LIBDIR)/libPanel.so.0
	$(LN) -s -- libPanel.so.0.0 $(DESTDIR)$(LIBDIR)/libPanel.so
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)panel $(DESTDIR)$(BINDIR)/panel
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)panelctl $(DESTDIR)$(BINDIR)/panelctl
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)run $(DESTDIR)$(BINDIR)/run

uninstall:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) uninstall) || exit; done
	$(RM) -- $(DESTDIR)$(LIBDIR)/libPanel.a
	$(RM) -- $(DESTDIR)$(LIBDIR)/libPanel.so.0.0
	$(RM) -- $(DESTDIR)$(LIBDIR)/libPanel.so.0
	$(RM) -- $(DESTDIR)$(LIBDIR)/libPanel.so
	$(RM) -- $(DESTDIR)$(BINDIR)/panel
	$(RM) -- $(DESTDIR)$(BINDIR)/panelctl
	$(RM) -- $(DESTDIR)$(BINDIR)/run

.PHONY: all subdirs clean distclean install uninstall
