#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.6	99/09/21 SMI"
#
# ucblib/libcurses/Makefile.com

LIBRARY=	libcurses.a
VERS=		.1

OBJECTS= 		\
	addch.o		\
	addstr.o	\
	box.o		\
	clear.o		\
	clrtobot.o	\
	clrtoeol.o	\
	cr_put.o	\
	cr_tty.o	\
	curses.o	\
	delch.o		\
	deleteln.o	\
	delwin.o	\
	endwin.o	\
	erase.o		\
	fullname.o	\
	getch.o		\
	getstr.o	\
	id_subwins.o	\
	idlok.o		\
	initscr.o	\
	insch.o		\
	insertln.o	\
	longname.o	\
	move.o		\
	mvprintw.o	\
	mvscanw.o	\
	mvwin.o		\
	newwin.o	\
	overlay.o	\
	overwrite.o	\
	printw.o	\
	putchar.o	\
	refresh.o	\
	scanw.o		\
	scroll.o	\
	standout.o	\
	toucholap.o	\
	touchwin.o	\
	tstp.o		\
	unctrl.o

# include library definitions
include $(SRC)/lib/Makefile.lib

ROOTLIBDIR=	$(ROOT)/usr/ucblib
ROOTLIBDIR64=	$(ROOT)/usr/ucblib/$(MACH64)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES += $(MAPFILE)
SRCS=		$(OBJECTS:%.o=../%.c)

# static is only for 32-bit
LIBS = $(DYNLIB) $(LINTLIB)

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINTDIR64= $(ROOTLIBDIR)/$(MACH64)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINT64= $(LINTSRC:%=$(ROOTLINTDIR64)/%)
STATICLIB= $(LIBRARY:%=$(ROOTLIBDIR)/%)

# install rule for lint source file target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)
$(ROOTLINTDIR64)/%: ../%
	$(INS.file)

$(LINTLIB):= SRCS=../llib-lcurses
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out
CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	
DYNFLAGS32 =	-Wl,-M,$(MAPFILE)
DYNFLAGS64 =	-Wl,-M,$(MAPFILE)
LDLIBS +=	-ltermcap -lucb -lc

CPPFLAGS = -I$(ROOT)/usr/ucbinclude -I../../../lib/libc/inc $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

#
# Include library targets
#
include $(SRC)/lib/Makefile.targ

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for 32-bit libcurses.a
$(STATICLIBDIR)/%: %
	$(INS.file)

