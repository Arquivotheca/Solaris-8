#
#ident	"@(#)Makefile.com	1.9	97/11/20 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/address/onc_fn_nisplus/Makefile
#

LIBRARYCCC= fn_ref_addr_onc_fn_nisplus.a
VERS = .1

OBJECTS= onc_fn_nisplus.o

# include library definitions
include ../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_p -lc

.KEEP_STATE:

all: $(LIBS) 

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include ../../../Makefile.targ
