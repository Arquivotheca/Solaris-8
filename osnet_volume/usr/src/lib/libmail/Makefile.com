# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma	ident  "@(#)Makefile.com 1.3     99/03/23 SMI"
#
# lib/libmail/Makefile.com

LIBRARY= libmail.a
VERS= .1

OBJECTS= 	abspath.o  casncmp.o   copystream.o delempty.o \
		getdomain.o maillock.o notifyu.o    popenvp.o \
		s_string.o  setup_exec.o strmove.o  skipspace.o \
		substr.o   systemvp.o  trimnl.o     xgetenv.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-lmail
$(LINTLIB):= LINTFLAGS=-I../inc -nvx
$(LINTLIB):= LINTFLAGS64=-I../inc -nvx -D__sparcv9

LINTOUT=	lint.out
LINTSRC=	$(LINTLIB:%.ln=%)

ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINTDIR64=  $(ROOTLIBDIR64)
ROOTLINT64=     $(LINTSRC:%=$(ROOTLINTDIR64)/%)
ROOTLINKS64=    $(ROOTLIBDIR64)/$(LIBLINKS)

CLEANFILES += 	$(LINTOUT) $(LINTLIB)
CLOBBERFILES +=	$(MAPFILE)

CFLAGS +=	-v -I../inc
CFLAGS64 +=	-v -I../inc
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB) $(DYNLIB64):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../inc/%.h

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# install rule for 64 bit lint library target
$(ROOTLINTDIR64)/%: ../common/%
	$(INS.file)
