#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.com	1.3 97/11/20 SMI"
#
# lib/fn/address/onc_fn_nisplus_root/Makefile.com
#
#
# produces fn_inet_nisplus.so$(VERS) (a symbolic link) as well

LIBRARYCCC= fn_ref_addr_onc_fn_nisplus_root.a
VERS = .1

OBJECTS= description.o to_ref.o

FNLINKS= fn_inet_nisplus.so$(VERS)
FNLINKS64= fn_inet_nisplus.so$(VERS)

# include library definitions
include ../../../Makefile.libfn


LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lnsl -lc 

.KEEP_STATE:

all: $(LIBS) 

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

