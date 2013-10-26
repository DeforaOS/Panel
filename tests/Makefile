TARGETS	= applets
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

applets_OBJS = applets.o
applets_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
applets_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

applets: $(applets_OBJS)
	$(CC) -o applets $(applets_OBJS) $(applets_LDFLAGS)

applets.o: applets.c
	$(CC) $(applets_CFLAGS) -c applets.c

clean:
	$(RM) -- $(applets_OBJS)

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)

uninstall:

.PHONY: all clean distclean install uninstall
