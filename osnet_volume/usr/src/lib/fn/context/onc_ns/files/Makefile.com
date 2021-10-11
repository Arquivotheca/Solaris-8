#
#ident	"@(#)Makefile.com	1.10	97/11/20 SMI"
#
# Copyright (c) 1994 - 1996,by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/onc_ns/files/Makefile.com
#

LIBRARYCCC= fn_ctx_onc_fn_files.a
VERS = .1

FILESOBJECTS = FNSP_filesOrgContext_default.o \
	FNSP_filesFlatContext_default.o 

NISOBJS = \
	fnsp_internal_common.o \
	FNSP_nisAddress.o \
	FNSP_nisOrgContext_default.o \
	FNSP_nisFlatContext_default.o  \
	FNSP_hash_function.o

CTXOBJS = \
	FNSP_filesHierContext.o FNSP_filesDotContext.o \
	FNSP_filesFlatContext.o FNSP_filesHUContext.o \
	FNSP_filesWeakSlashContext.o \
	FNSP_filesPrinternameContext.o FNSP_filesPrinterObject.o \
	FNSP_filesOrgContext.o 

OBJECTS =  $(FILESOBJECTS) $(NISOBJS) $(CTXOBJS) \
	FNSP_filesImpl.o \
	fnsp_files_internal.o \
	onc_fn_files.o

FNLINKS= fn_ctx_onc_fn_printer_files.so$(VERS)
FNLINKS64= fn_ctx_onc_fn_printer_files.so$(VERS)

# include library definitions
include ../../../../Makefile.libfn

$(DYNLIBCCC):= DYNFLAGS += -Qoption ld -R$(FNRPATH)
FNSPINCDIR=	$(SRC)/lib/fn/context/onc_ns/libfnsp/common
FNSNISINCDIR=	$(SRC)/lib/fn/context/onc_ns/nis/common
CPPFLAGS +=	-I$(FNSPINCDIR) -I$(FNSNISINCDIR)

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

# for $(NISOBJS)
objs/%.o profs/%.o pics/%.o: ../../nis/common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../../nis/common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
