#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.5	99/01/25 SMI"
#

LIBRARY=	libbsdmalloc.a
VERS=		.1

OBJECTS=  \
	malloc.bsd43.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
SRCS=           $(OBJECTS:%.o=../common/%.c)
CLOBBERFILES +=	$(MAPFILE)

LIBS +=          $(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS =	-uax
LINTFLAGS64 =	-uax -D__sparcv9
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
CPPFLAGS +=	-D_REENTRANT
CPPFLAGS64 +=	-D_REENTRANT
DYNFLAGS +=     -M $(MAPFILE)
LDLIBS +=       -lc

.KEEP_STATE:

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

$(DYNLIB):      $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

#
# create message catalogue files
#
TEXT_DOMAIN= SUNW_OST_OSLIB
_msg:	$(MSGDOMAIN) catalog

catalog:
	sh ../makelibcmdcatalog.sh $(MSGDOMAIN)

$(MSGDOMAIN):
	$(INS.dir)

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)

