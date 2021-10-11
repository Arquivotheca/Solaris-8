#
# Copyright (c) 1989 - 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.com	1.2 97/10/30 SMI"
#
# lib/fn/address/onc_fn_nis/Makefile.com
#

LIBRARYCCC= fn_ref_addr_onc_fn_nis.a
VERS = .1

OBJECTS= onc_fn_nis.o

# include library definitions
include ../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_p -lc

.KEEP_STATE:

all: $(LIBS) 

install_h:

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)


