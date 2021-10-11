#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.com	1.2 97/11/20 SMI"
#
# lib/fn/address/inet_ipaddress_string/Makefile.com
#

LIBRARYCCC= fn_ref_addr_inet_ipaddr_string.a
VERS = .1

OBJECTS= inet_ipaddr_string.o

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

