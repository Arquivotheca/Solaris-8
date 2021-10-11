#
# Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.com	1.1 97/10/21 SMI"
#
# lib/fn/context/DNS/Makefile.com
#

LIBRARYCCC= fn_ctx_inet_domain.a
VERS = .1

OBJECTS= cx.o cx-hard.o ref.o glue.o dns_ops.o dns_obj.o


# include library definitions
include ../../../Makefile.libfn

# do after include Makefile.lib, which also sets ROOTLIBDIR
ROOTLIBDIR=    $(ROOT)/usr/lib/fn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf -lresolv -lsocket -lnsl -lc

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)


