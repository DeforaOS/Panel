PACKAGE	= Panel
VERSION	= 0.0.0
SUBDIRS	= include src
RM	= rm -f
LN	= ln -f
TAR	= tar -czvf


all: subdirs

subdirs:
	@for i in $(SUBDIRS); do (cd $$i && $(MAKE)) || exit; done

clean:
	@for i in $(SUBDIRS); do (cd $$i && $(MAKE) clean) || exit; done

distclean:
	@for i in $(SUBDIRS); do (cd $$i && $(MAKE) distclean) || exit; done

dist:
	$(RM) -r $(PACKAGE)-$(VERSION)
	$(LN) -s . $(PACKAGE)-$(VERSION)
	@$(TAR) $(PACKAGE)-$(VERSION).tar.gz \
		$(PACKAGE)-$(VERSION)/include/panel.h \
		$(PACKAGE)-$(VERSION)/include/project.conf \
		$(PACKAGE)-$(VERSION)/src/panel.c \
		$(PACKAGE)-$(VERSION)/src/main.c \
		$(PACKAGE)-$(VERSION)/src/Makefile \
		$(PACKAGE)-$(VERSION)/src/panel.h \
		$(PACKAGE)-$(VERSION)/src/project.conf \
		$(PACKAGE)-$(VERSION)/src/applets/clock.c \
		$(PACKAGE)-$(VERSION)/src/applets/cpu.c \
		$(PACKAGE)-$(VERSION)/src/applets/desktop.c \
		$(PACKAGE)-$(VERSION)/src/applets/lock.c \
		$(PACKAGE)-$(VERSION)/src/applets/logout.c \
		$(PACKAGE)-$(VERSION)/src/applets/main.c \
		$(PACKAGE)-$(VERSION)/src/applets/memory.c \
		$(PACKAGE)-$(VERSION)/src/applets/tasks.c \
		$(PACKAGE)-$(VERSION)/src/applets/project.conf \
		$(PACKAGE)-$(VERSION)/Makefile \
		$(PACKAGE)-$(VERSION)/project.conf
	$(RM) $(PACKAGE)-$(VERSION)

install: all
	@for i in $(SUBDIRS); do (cd $$i && $(MAKE) install) || exit; done

uninstall:
	@for i in $(SUBDIRS); do (cd $$i && $(MAKE) uninstall) || exit; done

.PHONY: all subdirs clean distclean dist install uninstall
