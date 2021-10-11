#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	99/08/15 SMI"
#
# lib/libpctx/Makefile.com

LIBRARY = libpctx.a
VERS = .1

OBJECTS = libpctx.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

SRCS =		$(OBJECTS:%.o=../common/%.c)

LIBS = $(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS =	-mux -I../common
LINTFLAGS64 =	-mux -Xarch=v9 -I../common
LINTOUT =	lint.out

LINTSRC =	$(LINTLIB:%.ln=%)
ROOTLINTDIR =	$(ROOTLIBDIR)
ROOTLINT =	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES =	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I../common
CFLAGS64 +=	-v -I../common
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lproc -lc

$(LINTLIB) :=	SRCS = ../common/llib-lpctx
$(LINTLIB) :=	LINTFLAGS = -nvx -I../common
$(LINTLIB) :=	LINTFLAGS64 = -nvx -Xarch=v9 -I../common

.KEEP_STATE:

all: $(LIBS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
