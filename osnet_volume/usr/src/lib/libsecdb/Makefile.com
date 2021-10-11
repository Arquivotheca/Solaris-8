#
# Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
#
# lib/libsecdb/Makefile.com
#
#ident	"@(#)Makefile.com	1.2	99/07/16 SMI"
#

LIBRARY= libsecdb.a
VERS= .1

OBJECTS=	secdb.o getauthattr.o getexecattr.o getprofattr.o \
	 getuserattr.o chkauthattr.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile

SRCS=           $(OBJECTS:%.o=../common/%.c)

LOCFLAGS +=	-D_REENTRANT

# definitions for lint

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
LINTFLAGS+=	-I../common $(LOCFLAGS)
LINTFLAGS64+=	-I../common -Xarch=v9 $(LOCFLAGS)
LINTOUT=	lint.out

STATICLIBDIR=	$(ROOTLIBDIR)
STATICLIB=	$(LIBRARY:%=$(STATICLIBDIR)/%)

DYNLINKLIBDIR=	$(ROOTLIBDIR)
DYNLINKLIB=	$(LIBLINKS:%=$(DYNLINKLIBDIR)/%)

CFLAGS +=	-v $(LOCFLAGS)
CFLAGS64 +=	-v $(LOCFLAGS)
CPPFLAGS +=	-v $(LOCFLAGS)
DYNFLAGS +=     -M $(MAPFILE)
LDLIBS +=	-lc -lnsl -lcmd

.KEEP_STATE:

lint:	$(LINTLIB)

$(DYNLIB) :	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# Include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o:	../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

$(STATICLIBDIR)/%: %
	$(INS.file)

$(DYNLINKLIBDIR)/%: %$(VERS)
	$(INS.liblink)
