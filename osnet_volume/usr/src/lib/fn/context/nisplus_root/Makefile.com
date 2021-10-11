#
#ident	"@(#)Makefile.com	1.5	97/11/20 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/nisplus_root/Makefile
#

LIBRARYCCC= fn_ctx_onc_fn_nisplus_root.a
VERS = .1

OBJECTS = from_ref.o nisplus_cache.o fnsp_nisplus_root.o

# include library definitions
include ../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf -lnsl -lfn_p -lc

debug :=	CPPFLAGS += -g
tcov :=		CPPFLAGS += -a

# for testing
HDIR3 = $(SRC)/lib/fn/include
LDLIBS += -lxfn
CPPFLAGS += -I$(HDIR3)

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

# for fnsp_nisplus_root.o
objs/%.o profs/%.o pics/%.o: ../../onc_ns/nisplus/common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include ../../../Makefile.targ
