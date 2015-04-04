PACKAGE	= Panel
VERSION	= 0.3.1
SUBDIRS	= data doc include po src tests tools
RM	= rm -f
LN	= ln -f
TAR	= tar
MKDIR	= mkdir -m 0755 -p


all: subdirs

subdirs:
	@for i in $(SUBDIRS); do (cd "$$i" && \
		if [ -n "$(OBJDIR)" ]; then \
		([ -d "$(OBJDIR)$$i" ] || $(MKDIR) -- "$(OBJDIR)$$i") && \
		$(MAKE) OBJDIR="$(OBJDIR)$$i/"; \
		else $(MAKE); fi) || exit; done

clean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) clean) || exit; done

distclean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) distclean) || exit; done

dist:
	$(RM) -r -- $(OBJDIR)$(PACKAGE)-$(VERSION)
	$(LN) -s -- "$$PWD" $(OBJDIR)$(PACKAGE)-$(VERSION)
	@cd $(OBJDIR). && $(TAR) -czvf $(OBJDIR)$(PACKAGE)-$(VERSION).tar.gz -- \
		$(PACKAGE)-$(VERSION)/data/Makefile \
		$(PACKAGE)-$(VERSION)/data/Panel.pc.in \
		$(PACKAGE)-$(VERSION)/data/deforaos-panel-settings.desktop \
		$(PACKAGE)-$(VERSION)/data/deforaos-wifibrowser.desktop \
		$(PACKAGE)-$(VERSION)/data/pkgconfig.sh \
		$(PACKAGE)-$(VERSION)/data/project.conf \
		$(PACKAGE)-$(VERSION)/data/16x16/Makefile \
		$(PACKAGE)-$(VERSION)/data/16x16/panel-applet-bluetooth.png \
		$(PACKAGE)-$(VERSION)/data/16x16/panel-applet-desktop.png \
		$(PACKAGE)-$(VERSION)/data/16x16/panel-applet-usb.png \
		$(PACKAGE)-$(VERSION)/data/16x16/panel-settings.png \
		$(PACKAGE)-$(VERSION)/data/16x16/project.conf \
		$(PACKAGE)-$(VERSION)/data/22x22/Makefile \
		$(PACKAGE)-$(VERSION)/data/22x22/panel-applet-bluetooth.png \
		$(PACKAGE)-$(VERSION)/data/22x22/panel-applet-desktop.png \
		$(PACKAGE)-$(VERSION)/data/22x22/panel-settings.png \
		$(PACKAGE)-$(VERSION)/data/22x22/project.conf \
		$(PACKAGE)-$(VERSION)/data/24x24/Makefile \
		$(PACKAGE)-$(VERSION)/data/24x24/panel-applet-bluetooth.png \
		$(PACKAGE)-$(VERSION)/data/24x24/panel-applet-desktop.png \
		$(PACKAGE)-$(VERSION)/data/24x24/panel-applet-usb.png \
		$(PACKAGE)-$(VERSION)/data/24x24/panel-settings.png \
		$(PACKAGE)-$(VERSION)/data/24x24/project.conf \
		$(PACKAGE)-$(VERSION)/data/32x32/Makefile \
		$(PACKAGE)-$(VERSION)/data/32x32/panel-applet-bluetooth.png \
		$(PACKAGE)-$(VERSION)/data/32x32/panel-applet-desktop.png \
		$(PACKAGE)-$(VERSION)/data/32x32/panel-settings.png \
		$(PACKAGE)-$(VERSION)/data/32x32/project.conf \
		$(PACKAGE)-$(VERSION)/data/48x48/Makefile \
		$(PACKAGE)-$(VERSION)/data/48x48/panel-applet-bluetooth.png \
		$(PACKAGE)-$(VERSION)/data/48x48/panel-applet-desktop.png \
		$(PACKAGE)-$(VERSION)/data/48x48/panel-settings.png \
		$(PACKAGE)-$(VERSION)/data/48x48/project.conf \
		$(PACKAGE)-$(VERSION)/data/scalable/Makefile \
		$(PACKAGE)-$(VERSION)/data/scalable/panel-applet-bluetooth.svg \
		$(PACKAGE)-$(VERSION)/data/scalable/project.conf \
		$(PACKAGE)-$(VERSION)/doc/Makefile \
		$(PACKAGE)-$(VERSION)/doc/docbook.sh \
		$(PACKAGE)-$(VERSION)/doc/manual.css.xml \
		$(PACKAGE)-$(VERSION)/doc/panel.css.xml \
		$(PACKAGE)-$(VERSION)/doc/panel.xml \
		$(PACKAGE)-$(VERSION)/doc/panelctl.css.xml \
		$(PACKAGE)-$(VERSION)/doc/panelctl.xml \
		$(PACKAGE)-$(VERSION)/doc/panel-embed.css.xml \
		$(PACKAGE)-$(VERSION)/doc/panel-embed.xml \
		$(PACKAGE)-$(VERSION)/doc/panel-message.css.xml \
		$(PACKAGE)-$(VERSION)/doc/panel-message.xml \
		$(PACKAGE)-$(VERSION)/doc/panel-notify.css.xml \
		$(PACKAGE)-$(VERSION)/doc/panel-notify.xml \
		$(PACKAGE)-$(VERSION)/doc/wifibrowser.css.xml \
		$(PACKAGE)-$(VERSION)/doc/wifibrowser.xml \
		$(PACKAGE)-$(VERSION)/doc/project.conf \
		$(PACKAGE)-$(VERSION)/include/Panel.h \
		$(PACKAGE)-$(VERSION)/include/Makefile \
		$(PACKAGE)-$(VERSION)/include/project.conf \
		$(PACKAGE)-$(VERSION)/po/Makefile \
		$(PACKAGE)-$(VERSION)/po/gettext.sh \
		$(PACKAGE)-$(VERSION)/po/POTFILES \
		$(PACKAGE)-$(VERSION)/po/es.po \
		$(PACKAGE)-$(VERSION)/po/fr.po \
		$(PACKAGE)-$(VERSION)/po/project.conf \
		$(PACKAGE)-$(VERSION)/src/panel.c \
		$(PACKAGE)-$(VERSION)/src/window.c \
		$(PACKAGE)-$(VERSION)/src/main.c \
		$(PACKAGE)-$(VERSION)/src/panelctl.c \
		$(PACKAGE)-$(VERSION)/src/run.c \
		$(PACKAGE)-$(VERSION)/src/Makefile \
		$(PACKAGE)-$(VERSION)/src/helper.c \
		$(PACKAGE)-$(VERSION)/src/panel.h \
		$(PACKAGE)-$(VERSION)/src/window.h \
		$(PACKAGE)-$(VERSION)/src/project.conf \
		$(PACKAGE)-$(VERSION)/src/applets/battery.c \
		$(PACKAGE)-$(VERSION)/src/applets/bluetooth.c \
		$(PACKAGE)-$(VERSION)/src/applets/clock.c \
		$(PACKAGE)-$(VERSION)/src/applets/close.c \
		$(PACKAGE)-$(VERSION)/src/applets/cpu.c \
		$(PACKAGE)-$(VERSION)/src/applets/cpufreq.c \
		$(PACKAGE)-$(VERSION)/src/applets/desktop.c \
		$(PACKAGE)-$(VERSION)/src/applets/embed.c \
		$(PACKAGE)-$(VERSION)/src/applets/gps.c \
		$(PACKAGE)-$(VERSION)/src/applets/gsm.c \
		$(PACKAGE)-$(VERSION)/src/applets/lock.c \
		$(PACKAGE)-$(VERSION)/src/applets/logout.c \
		$(PACKAGE)-$(VERSION)/src/applets/menu.c \
		$(PACKAGE)-$(VERSION)/src/applets/memory.c \
		$(PACKAGE)-$(VERSION)/src/applets/mixer.c \
		$(PACKAGE)-$(VERSION)/src/applets/mpd.c \
		$(PACKAGE)-$(VERSION)/src/applets/network.c \
		$(PACKAGE)-$(VERSION)/src/applets/pager.c \
		$(PACKAGE)-$(VERSION)/src/applets/phone.c \
		$(PACKAGE)-$(VERSION)/src/applets/rotate.c \
		$(PACKAGE)-$(VERSION)/src/applets/separator.c \
		$(PACKAGE)-$(VERSION)/src/applets/spacer.c \
		$(PACKAGE)-$(VERSION)/src/applets/swap.c \
		$(PACKAGE)-$(VERSION)/src/applets/systray.c \
		$(PACKAGE)-$(VERSION)/src/applets/tasks.c \
		$(PACKAGE)-$(VERSION)/src/applets/template.c \
		$(PACKAGE)-$(VERSION)/src/applets/title.c \
		$(PACKAGE)-$(VERSION)/src/applets/usb.c \
		$(PACKAGE)-$(VERSION)/src/applets/user.c \
		$(PACKAGE)-$(VERSION)/src/applets/volume.c \
		$(PACKAGE)-$(VERSION)/src/applets/wpa_supplicant.c \
		$(PACKAGE)-$(VERSION)/src/applets/Makefile \
		$(PACKAGE)-$(VERSION)/src/applets/tasks.atoms \
		$(PACKAGE)-$(VERSION)/src/applets/project.conf \
		$(PACKAGE)-$(VERSION)/tests/applets.c \
		$(PACKAGE)-$(VERSION)/tests/applets2.c \
		$(PACKAGE)-$(VERSION)/tests/Makefile \
		$(PACKAGE)-$(VERSION)/tests/tests.sh \
		$(PACKAGE)-$(VERSION)/tests/project.conf \
		$(PACKAGE)-$(VERSION)/tools/embed.c \
		$(PACKAGE)-$(VERSION)/tools/message.c \
		$(PACKAGE)-$(VERSION)/tools/notify.c \
		$(PACKAGE)-$(VERSION)/tools/test.c \
		$(PACKAGE)-$(VERSION)/tools/wifibrowser.c \
		$(PACKAGE)-$(VERSION)/tools/Makefile \
		$(PACKAGE)-$(VERSION)/tools/helper.c \
		$(PACKAGE)-$(VERSION)/tools/project.conf \
		$(PACKAGE)-$(VERSION)/COPYING \
		$(PACKAGE)-$(VERSION)/README.md \
		$(PACKAGE)-$(VERSION)/Makefile \
		$(PACKAGE)-$(VERSION)/config.h \
		$(PACKAGE)-$(VERSION)/config.sh \
		$(PACKAGE)-$(VERSION)/project.conf
	$(RM) -- $(OBJDIR)$(PACKAGE)-$(VERSION)

distcheck: dist
	$(TAR) -xzvf $(OBJDIR)$(PACKAGE)-$(VERSION).tar.gz
	$(MKDIR) -- $(PACKAGE)-$(VERSION)/objdir
	$(MKDIR) -- $(PACKAGE)-$(VERSION)/destdir
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/")
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/" DESTDIR="$$PWD/destdir" install)
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/" DESTDIR="$$PWD/destdir" uninstall)
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) OBJDIR="$$PWD/objdir/" distclean)
	(cd "$(PACKAGE)-$(VERSION)" && $(MAKE) dist)
	$(RM) -r -- $(PACKAGE)-$(VERSION)

install:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) install) || exit; done

uninstall:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) uninstall) || exit; done

.PHONY: all subdirs clean distclean dist distcheck install uninstall
