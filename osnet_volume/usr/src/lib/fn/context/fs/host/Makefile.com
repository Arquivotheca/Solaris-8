#
#pragma ident	"@(#)Makefile.com	1.3	97/10/21 SMI"
#
# Copyright (c) 1994 - 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/fs/host/Makefile.com
#

LIBRARYCCC= fn_ctx_onc_fn_fs_host.a
VERS = .1

OBJECTS = onc_fn_fs_host.o FSHost.o Export.o getexports.o xdr_utils.o

# include library definitions
include ../../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

CPPFLAGS += -I$(SRC)/lib/fn/libxfn/common

LDLIBS += -lxfn -lfn_spf -lrpcsvc -lnsl -lc

all: $(LIBS) 

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

.KEEP_STATE:
