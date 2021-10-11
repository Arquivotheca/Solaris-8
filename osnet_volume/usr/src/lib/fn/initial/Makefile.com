#
# Copyright (c) 1989 - 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.com	1.4 97/11/20 SMI"
#
# lib/fn/initial/Makefile.com
#

LIBRARYCCC= fn_ctx_initial.a
VERS = .1

OBJECTS= Entry.o Table.o FNSP_InitialContext.o from_initial.o \
	FNSP_GlobalContext.o \
	FNSP_user_entries.o FNSP_host_entries.o FNSP_global_entries.o \
	FNSP_enterprise.o FNSP_enterprise_nisplus.o \
	FNSP_enterprise_nis.o FNSP_enterprise_files.o fnsp_nisplus_root.o

# include library definitions
include ../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf -lfn_p -lnsl -lc

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ

# for fnsp_nisplus_root.o
objs/%.o profs/%.o pics/%.o: ../../context/onc_ns/nisplus/common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)
