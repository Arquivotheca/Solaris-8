#
#ident	"@(#)Makefile	1.1	97/08/25 SMI"
#
# Copyright (c) 1993 - 1997 by Sun Microsystems, Inc.
#
# lib/fn/libxfn.1/Makefile.com
#

LIBRARYCCC= libxfn.a
#LIBRARY= libxfn.a
VERS = .1

COMPATOBJS = \
	FN_attribute_compat.o \
	FN_attrmodlist_compat.o \
	FN_attrset_compat.o \
	FN_composite_name_compat.o \
	FN_compound_name_compat.o \
	FN_ctx_compat.o \
	FN_ref_compat.o \
	FN_ref_addr_compat.o \
	FN_status_compat.o \
	FN_string_compat.o

INCOMPATOBJS = \
	FN_string_incompat.o \
	FN_ctx_incompat.o

OBJECTS= compat.o $(COMPATOBJS) $(INCOMPATOBJS)

# include library definitions
include ../../Makefile.libfn

ROOTLIBDIR=	$(ROOT)/usr/lib
ROOTLIBDIR64=	$(ROOT)/usr/lib/$(MACH64)

LIBS = $(DYNLIBCCC)

# dl for dlopen; 
# C
LDLIBS += -ldl -lc

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)


