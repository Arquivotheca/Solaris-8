#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.6	99/01/25 SMI"
#
# lib/libmtmalloc/Makefile.com

LIBRARY = libmtmalloc.a
VERS = .1

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)
OBJECTS = mtmalloc.o
SRCS = ../common/mtmalloc.c

LINTFLAGS 	= -mux -I../common
LINTFLAGS64 	= -mux -D__sparcv9 -I../common
LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)

LIBS 		 = $(DYNLIB)
LDLIBS 		+= -lc
GENCFLAGS	= -v -K pic -I../common -D_REENTRANT -D_TS_ERRNO
CFLAGS 		+= $(GENCFLAGS)
CFLAGS64	+= $(GENCFLAGS)
CPPFLAGS	+= -D_REENTRANT -D_TS_ERRNO
DYNFLAGS 	+= -z nodlopen -M $(MAPFILE)

CLEANFILES 	+= $(LINTOUT) $(LINTLIB)
PLATFORM	= `uname -p`

.KEEP_STATE:

all: $(LIBS)

lint:	$(LINTLIB)

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
