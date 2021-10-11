#
#pragma ident	"@(#)Makefile	1.7	97/03/12 SMI"
#
# Copyright (c) 1990,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libTL/Makefile
#
LIBRARY= 	libTL.a

TLTEST=		TLtest

OBJECTS= 	TLappend.o     TLassign.o     TLclose.o     TLdelete.o \
		TLgetentry.o   TLgetfield.o   TLopen.o      TLread.o   \
		TLsearches.o   TLsubst.o      TLsync.o      TLwrite.o  \
		description.o  entry.o        field.o       file.o parse.o \
		search.o       space.o        table.o       utils.o

# include library definitions
include ../Makefile.lib

TXTS +=		$(TLTEST).c

CPPFLAGS=	-Ihdrs $(CPPFLAGS.master)
LINTFLAGS=	-u
LINTOUT=	lint.out
ARFLAGS=	cr
AROBJS=		`$(LORDER) $(OBJS) | $(TSORT)`
CLEANFILES=	$(TLTEST) $(LINTLIB) $(LINTOUT)

# conditional assignment
$(TLTEST):=	LDLIBS=$(LIBRARY)

LIBS += $(LINTLIB)

$(LINTLIB):= SRCS=llib-lTL
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=
 
LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)

.KEEP_STATE:

all:		$(LIBS) $(TXTS)

install:	all $(ROOTLIBS) $(ROOTLINT)

lint:		$(LINTLIB)

# TLtest can be built from default rules, using its LDLIBS above

# include library targets
include ../Makefile.targ

# install rule for lint library target
$(ROOTLINTDIR)/%: %
	$(INS.file)
