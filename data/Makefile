SUBDIRS	= 16x16 22x22 24x24 32x32 48x48 scalable
MKDIR	= mkdir -m 0755 -p
INSTALL	= install
RM	= rm -f
TARGETS	= $(OBJDIR)Panel.pc
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: subdirs $(TARGETS)

subdirs:
	@for i in $(SUBDIRS); do (cd "$$i" && \
		if [ -n "$(OBJDIR)" ]; then \
		([ -d "$(OBJDIR)$$i" ] || $(MKDIR) -- "$(OBJDIR)$$i") && \
		$(MAKE) OBJDIR="$(OBJDIR)$$i/"; \
		else $(MAKE); fi) || exit; done

$(OBJDIR)Panel.pc: Panel.pc.in ../config.sh
	./pkgconfig.sh -P "$(PREFIX)" -- "$(OBJDIR)Panel.pc"

clean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) clean) || exit; done
	$(RM) -- $(Panel.pc_OBJS)
	./pkgconfig.sh -c -P "$(PREFIX)" -- "Panel.pc"

distclean:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) distclean) || exit; done
	$(RM) -- $(Panel.pc_OBJS)
	./pkgconfig.sh -c -P "$(PREFIX)" -- "Panel.pc"
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) install) || exit; done
	./pkgconfig.sh -P "$(DESTDIR)$(PREFIX)" -i -- "$(OBJDIR)Panel.pc"
	$(MKDIR) $(DESTDIR)$(PREFIX)/share/applications
	$(INSTALL) -m 0644 deforaos-panel-settings.desktop $(DESTDIR)$(PREFIX)/share/applications/deforaos-panel-settings.desktop
	$(MKDIR) $(DESTDIR)$(PREFIX)/share/applications
	$(INSTALL) -m 0644 deforaos-wifibrowser.desktop $(DESTDIR)$(PREFIX)/share/applications/deforaos-wifibrowser.desktop

uninstall:
	@for i in $(SUBDIRS); do (cd "$$i" && $(MAKE) uninstall) || exit; done
	./pkgconfig.sh -P "$(DESTDIR)$(PREFIX)" -u -- "Panel.pc"
	$(RM) -- $(DESTDIR)$(PREFIX)/share/applications/deforaos-panel-settings.desktop
	$(RM) -- $(DESTDIR)$(PREFIX)/share/applications/deforaos-wifibrowser.desktop

.PHONY: all subdirs clean distclean install uninstall
