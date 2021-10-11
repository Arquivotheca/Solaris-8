#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.11	99/01/25 SMI"
#
# lib/libaio/Makefile.com

LIBRARY=	libaio.a
VERS=		.1

COBJS=		\
	aio.o	\
	posix_aio.o	\
	scalls.o	\
	sig.o	\
	subr.o	\
	ma.o

OBJECTS= $(COBJS) $(MOBJS)

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile

CLOBBERFILES +=	$(MAPFILE)

SRCS=		$(COBJS:%.o=../common/%.c)

# Override LIBS so that only a dynamic library is built.

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=      -u
LINTFLAGS64=    -u -D__sparcv9
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

COMDIR=		../common

CPPFLAGS += -I. -Iinc -I.. -I$(COMDIR) $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB) $(DYNLIB64): 	$(MAPFILE)


#
# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: $(MDIR)/%.s
	$(BUILD.s)
	$(POST_PROCESS_0)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

