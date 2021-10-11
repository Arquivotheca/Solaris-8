#
#ident	"@(#)Makefile.com	1.5	97/11/20 SMI"
#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/address/osi_paddr/Makefile
#

LIBRARYCCC= fn_ref_addr_osi_paddr.a
VERS = .1

OBJECTS= osi_paddr.o

# include library definitions
include ../../../Makefile.libfn


LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lc

.KEEP_STATE:

all: $(LIBS) 

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include ../../../Makefile.targ
