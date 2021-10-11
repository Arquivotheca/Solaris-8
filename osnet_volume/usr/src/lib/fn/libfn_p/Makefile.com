#
# Copyright (c) 1989 - 1997 by Sun Microsystems, Inc.
#
#pragma ident "@(#)Makefile.com	1.4 97/11/20 SMI"
#
# lib/fn/libfn_p/Makefile.com
#

LIBRARYCCC= libfn_p.a
VERS = .1

OBJECTS= fnsp_reference.o fnselect_util.o

# include library definitions
include ../../Makefile.libfn

ROOTLIBDIR=	$(ROOT)/usr/lib
ROOTLIBDIR64=	$(ROOT)/usr/lib/$(MACH64)

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lnsl -lc

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

