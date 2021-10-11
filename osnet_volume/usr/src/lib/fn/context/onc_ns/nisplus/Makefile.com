#
#ident	"@(#)Makefile.com	1.15	97/11/20 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/onc_ns/nisplus/Makefile
#

LIBRARYCCC= fn_ctx_onc_fn_nisplus.a
VERS = .1

CTXOBJS = FNSP_nisplusWeakSlashContext.o FNSP_nisplusHierContext.o \
	FNSP_nisplusFlatContext.o FNSP_nisplusDotContext.o \
	FNSP_HUContext.o FNSP_nisplusOrgContext.o \
	FNSP_HostnameContext.o FNSP_UsernameContext.o \
	FNSP_ENSContext.o \
	FNSP_nisplusPrinternameContext.o FNSP_nisplusPrinterObject.o

OBJECTS = fnsp_internal.o fnsp_nisplus_root.o onc_fn_nisplus.o \
	fnsp_search.o fnsp_hostuser.o fnsp_attrs.o \
	FNSP_nisplus_address.o $(CTXOBJS) 

FNLINKS= fn_ctx_onc_fn_printer_nisplus.so$(VERS)
FNLINKS64= fn_ctx_onc_fn_printer_nisplus.so$(VERS)

# include library definitions
include ../../../../Makefile.libfn

$(DYNLIBCCC):= DYNFLAGS += -Qoption ld -R$(FNRPATH)
FNSPINCDIR=	$(SRC)/lib/fn/context/onc_ns/libfnsp/common
CPPFLAGS +=	-I$(FNSPINCDIR)

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf $(FNSPLIB) -lnsl -lfn_p -lc

debug :=	CPPFLAGS += -g
tcov :=		CPPFLAGS += -a

.KEEP_STATE:

all: $(LIBS) 

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)
