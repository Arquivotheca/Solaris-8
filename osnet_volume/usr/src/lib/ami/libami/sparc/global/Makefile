#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.1 99/07/11 SMI"
#
#	Makefile for sparc global libami.so.1
#

LIBRARY= libami-global.a
AMILINKS= libami.so
LINTLINKS= llib-lami-global.ln
VERS= .1

# Mapfile directory
MAPDIR=	../../spec/sparc

# include library definitions
include $(SRC)/lib/Makefile.lib
include ../../Makefile.com

OBJECTS = $(INTLOBJS)
CLOBBERFILES +=	$(MAPFILE) $(LINTLIB) $(LINTOUT)

.PARALLEL:	$(OBJS) $(PROFS) $(PICS) DUMMY

# For 56 bits / 1024 bytes
CPPFLAGS += -DAMI_GLOBAL

# For 40 bits / 512 bytes
# CPPFLAGS += -DAMI_WEAK

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
