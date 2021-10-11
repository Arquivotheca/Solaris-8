#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.8	99/05/05 SMI"
#
# lib/libdevinfo/Makefile
#

LIBRARY=	libdevinfo.a
VERS=		.1

OBJECTS=	devfsinfo.o devinfo.o devinfo_prop_decode.o

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
LDLIBS +=	-ldl -lc

$(LINTLIB) :=	SRCS = ../llib-ldevinfo
$(LINTLIB) :=	LINTFLAGS = -nvx -I..
$(LINTLIB) :=	LINTFLAGS64 = -nvx -Xarch=v9 -I..

STATICLIBDIR=	$(ROOTLIBDIR)
STATICLIB=	$(LIBRARY:%=$(STATICLIBDIR)/%)

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
