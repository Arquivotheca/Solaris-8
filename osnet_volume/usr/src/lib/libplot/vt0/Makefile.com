#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.3	99/01/25 SMI"
#
# lib/libplot/vt0/Makefile.com

LIBRARY= libvt0.a
VERS= .1

OBJECTS=	\
	arc.o	box.o	circle.o	close.o	\
	dot.o	erase.o		label.o	frame.o	\
	line.o	linmod.o	move.o	open.o	\
	point.o	space.o	subr.o

# include library definitions
include ../../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%=-M %)
SRCS=           $(OBJECTS:%.o=../common/%.c)
CLOBBERFILES += $(MAPFILE)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-lvt0
$(LINTLIB):= LINTFLAGS=-nvx
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	$(MAPOPTS)
LDLIBS += -lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
