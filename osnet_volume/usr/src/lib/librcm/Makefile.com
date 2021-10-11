#
#ident	"@(#)Makefile.com	1.1	99/08/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/librcm/Makefile.com
#
#
LIBRARY=	librcm.a
VERS=		.1
OBJECTS=	librcm.o librcm_event.o

# include library definitions
include ../../Makefile.lib

SRCS=		$(OBJECTS:%.o=../%.c)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

LIBS =		$(DYNLIB) $(LINTLIB)

LINTFLAGS =	-mux -I..
LINTFLAGS64 =	-mux -D__sparcv9 -I..
LINTOUT=	lint.out

LINTSRC =       $(LINTLIB:%.ln=%)
ROOTLINTDIR =   $(ROOTLIBDIR)
ROOTLINT =      $(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES=	$(LINTOUT) $(LINTLIB)

CPPFLAGS +=	-v
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-ldoor -lthread -ldl -lc

$(LINTLIB) :=	SRCS = ../llib-lrcm
$(LINTLIB) :=	LINTFLAGS = -nvx -I..
$(LINTLIB) :=	LINTFLAGS64 = -nvx -Xarch=v9 -I..

.KEEP_STATE:

all : $(LIBS)

lint :
	$(LINT.c) $(SRCS) $(LDLIBS)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o:	../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(STATICLIBDIR)/%: %
	$(INS.file)

$(ROOTLINTDIR)/%: ../%
	$(INS.file)
