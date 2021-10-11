#
#ident	"@(#)Makefile.com	1.6	98/01/15 SMI"
#
# Copyright (c) 1994 - 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/onc_ns/nis/Makefile.com
#

LIBRARYCCC= fn_ctx_onc_fn_nis.a
VERS = .1

DEFOBJS = \
	FNSP_nisOrgContext_default.o \
	FNSP_nisFlatContext_default.o

CTXOBJS = \
	FNSP_nisFlatContext.o FNSP_nisHUContext.o \
	FNSP_nisWeakSlashContext.o \
	FNSP_nisDotContext.o FNSP_nisHierContext.o \
	FNSP_nisPrinternameContext.o FNSP_nisPrinterObject.o \
	FNSP_nisOrgContext.o 

OBJECTS = $(CTXOBJS) $(DEFOBJS) \
	FNSP_nisAddress.o \
	FNSP_nisImpl.o fnsp_nis_internal.o fnsp_internal_common.o \
	onc_fn_nis.o FNSP_hash_function.o fnsypprot_client.o \
	fnsypprot_clnt.o fnsypprot_xdr.o

CLEANFILES += ../common/fnsypprot.h ../common/fnsypprot_clnt.c \
	../common/fnsypprot_svc.c ../common/fnsypprot_xdr.c

FNLINKS= fn_ctx_onc_fn_printer_nis.so$(VERS) fn_ctx_onc_fn_nis_root.so$(VERS)
FNLINKS64= fn_ctx_onc_fn_printer_nis.so$(VERS) fn_ctx_onc_fn_nis_root.so$(VERS)

# include library definitions
include ../../../../Makefile.libfn

$(DYNLIBCCC):= DYNFLAGS += -Qoption ld -R$(FNRPATH)
FNSPINCDIR=	$(SRC)/lib/fn/context/onc_ns/libfnsp/common
CPPFLAGS +=	-I$(FNSPINCDIR)

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf $(FNSPLIB) -lnsl -lfn_p -ldl -lc

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

objs/%.o profs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

