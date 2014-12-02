TARGETS	= $(OBJDIR)applets $(OBJDIR)applets2 $(OBJDIR)tests.log
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
CC	= cc
CPPFLAGSF= -I ../include
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libDesktop`
CFLAGS	= -Wall -g -O2
LDFLAGSF= -W `pkg-config --libs libDesktop`
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

applets_OBJS = $(OBJDIR)applets.o
applets_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
applets_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)applets: $(applets_OBJS)
	$(CC) -o $(OBJDIR)applets $(applets_OBJS) $(applets_LDFLAGS)

applets2_OBJS = $(OBJDIR)applets2.o
applets2_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
applets2_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)applets2: $(applets2_OBJS)
	$(CC) -o $(OBJDIR)applets2 $(applets2_OBJS) $(applets2_LDFLAGS)

$(OBJDIR)tests.log: $(OBJDIR)applets $(OBJDIR)applets2 tests.sh
	./tests.sh -P "$(PREFIX)" -- "$(OBJDIR)tests.log"

$(OBJDIR)applets.o: applets.c
	$(CC) $(applets_CFLAGS) -o $(OBJDIR)applets.o -c applets.c

$(OBJDIR)applets2.o: applets2.c
	$(CC) $(applets2_CFLAGS) -o $(OBJDIR)applets2.o -c applets2.c

clean:
	$(RM) -- $(applets_OBJS) $(applets2_OBJS) $(tests.log_OBJS)
	./tests.sh -c -P "$(PREFIX)" -- "tests.log"

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)

uninstall:

.PHONY: all clean distclean install uninstall
