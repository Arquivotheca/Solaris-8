#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.2 99/07/11 SMI"
#
#	Makefile for sparc domestic libami.so.1
#

LIBRARY= libami-domestic.a
AMILINKS= libami.so
LINTLINKS= llib-lami-domestic.ln
VERS= .1

# Mapfile directory
MAPDIR=	../../spec/sparc

# include library definitions
include $(SRC)/lib/Makefile.lib
include ../../Makefile.com

OBJECTS = $(INTLOBJS)
CLOBBERFILES += $(MAPFILE) $(LINTLIB) $(LINTOUT)

.PARALLEL:	$(OBJS) $(PROFS) $(PICS) DUMMY

CPPFLAGS += -DAMI_DOMESTIC

LIBS = $(DYNLIB)

target_all: $(MAPFILE) .WAIT $(LIBS) $(LINTLIB)

$(LIBS): $(LGLOPICS)

# include library targets
include $(SRC)/lib/Makefile.targ

install debug: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTAMILINKS) \
	$(ROOTLINT) $(ROOTLINTLIBS) $(LINTLIBLINKS)
clean:

lint: $(LINTLIB)

.KEEP_STATE:

