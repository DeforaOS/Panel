PREFIX	= /usr/local
DESTDIR	=
MKDIR	= mkdir -m 0755 -p
INSTALL	= install
RM	= rm -f


all:

clean:

distclean: clean

install:
	$(MKDIR) $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
	$(INSTALL) -m 0644 panel-applet-bluetooth.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/panel-applet-bluetooth.svg

uninstall:
	$(RM) -- $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/panel-applet-bluetooth.svg

.PHONY: all clean distclean install uninstall
