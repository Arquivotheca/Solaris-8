#
#ident	"@(#)Makefile.com	1.2	97/11/20 SMI"
#
# Copyright (c) 1989 - 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/address/fn_link_addr/Makefile.com
#

LIBRARYCCC= fn_ref_addr_fn_link_addr.a
VERS = .1

OBJECTS= fn_link_addr.o

# include library definitions
include ../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lc

.KEEP_STATE:

all: $(LIBS) 

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)


