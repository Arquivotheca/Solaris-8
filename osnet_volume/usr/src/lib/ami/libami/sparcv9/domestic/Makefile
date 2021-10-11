#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.1 99/07/11 SMI"
#
#	Makefile for sparcv9 domestic libami.so.1
#

LIBRARY= libami-domestic.a
AMILINKS= libami.so
LINTLINKS= llib-lami-domestic.ln
VERS= .1

# Mapfile directory
MAPDIR=	../../spec/sparcv9

# include library definitions
include $(SRC)/lib/Makefile.lib
include $(SRC)/lib/Makefile.lib.64
include ../../Makefile.com

OBJECTS = $(INTLOBJS)
CLOBBERFILES +=	$(MAPFILE) $(LINTLIB) $(LINTOUT)

.PARALLEL:	$(OBJS) $(PROFS) $(PICS) DUMMY

CPPFLAGS += -DAMI_DOMESTIC

LIBS = $(DYNLIB)

target_all: $(MAPFILE) .WAIT $(LIBS) $(LINTLIB)

$(LIBS): $(LGLOPICS)

# include library targets
include $(SRC)/lib/Makefile.targ

install debug: all $(ROOTLIBDIR64) $(ROOTLIBS64) $(ROOTAMILINKS64) \
	$(ROOTLINT) $(ROOTLINTLIBS64) $(LINTLIBLINKS64)
clean:

lint: $(LINTLIB)

.KEEP_STATE:

