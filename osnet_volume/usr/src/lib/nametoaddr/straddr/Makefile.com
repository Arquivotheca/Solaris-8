#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	99/01/25 SMI"
#
# lib/nametoaddr/straddr/Makefile.com

LIBRARY= libstraddr.a
VERS= .2

OBJECTS= straddr.o

# include library definitions
include ../../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

# set exclusively to avoid libtcpip.so being built up.
# do not change ordering of includes and DYNLIB
DYNLIB=	straddr.so$(VERS)
LIBLINKS= straddr.so

LINTFLAGS64=	-ux -errchk=longptr64
CFLAGS +=	-v
CFLAGS64 +=	-v
CPPFLAGS += -I../../inc -D_REENTRANT
LDLIBS +=	-lnsl -lc
DYNFLAGS +=	-M $(MAPFILE)

LIBS +=		$(DYNLIB)
SRCS=		$(OBJECTS:%.o=../common/%.c)

LINTOUT=	lint.out
CLEANFILES += $(LINTOUT) $(LINTLIB)


.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

