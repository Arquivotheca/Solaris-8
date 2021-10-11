#
# Copyright (c) 1989 - 1997 by Sun Microsystems, Inc.
#
#pragma ident "@(#)Makefile.com	1.3 97/11/20 SMI"
#
# lib/fn/context/onc_ns/libfnsp/Makefile.com
#

LIBRARYCCC= libfnsp.a
VERS = .1

CTXOBJS = FNSP_FlatContext.o  FNSP_HierContext.o \
	FNSP_WeakSlashContext.o \
	FNSP_PrinterContext.o \
	FNSP_PrinternameContext.o FNSP_PrinterObject.o \
	FNSP_defaultContext.o FNSP_OrgContext.o 

COMMONOBJS = FNSP_Syntax.o FNSP_Address.o FNSP_Impl.o

OBJECTS = fnsp_utils.o $(CTXOBJS) $(COMMONOBJS)

# include library definitions
include ../../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf -lfn_p -lnsl -lc

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



