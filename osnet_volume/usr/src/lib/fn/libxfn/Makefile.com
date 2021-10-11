#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.8	99/01/25 SMI"
#
# lib/fn/libxfn/Makefile.com
#

LIBRARYCCC= libxfn.a
VERS=	.2		# version of libxfn.so
FNVERS=	.1		# version of private shared objects

MAPFILE=	$(MAPDIR)/mapfile

IVYOBJS= FN_string.o FN_string_rep.o FN_string_char.o FN_string_wchar.o \
	FN_identifier.o \
	List.o NameList.o FN_composite_name.o \
	AddressList.o 	Set.o NameSet.o BindingSet.o AttrSet.o AttrValSet.o \
	FN_attrmodlist.o AttrModList.o \
	FN_ctx.o FN_compound_name.o FN_ref.o FN_ref_addr.o FN_attrvalue.o\
	FN_status.o FN_bindingset.o FN_nameset.o FN_attrset.o FN_attribute.o \
	FN_search_control.o FN_search_filter.o FN_searchset.o SearchSet.o \
	FN_ext_searchset.o ExtSearchSet.o

IVYCOBJS= FN_string_c.o FN_composite_name_c.o FN_ref_c.o FN_ref_addr_c.o\
	FN_status_c.o FN_bindingset_c.o FN_nameset_c.o FN_attrset_c.o \
	FN_attrmodlist_c.o FN_ctx_c.o FN_compound_name_c.o FN_attribute_c.o \
	FN_search_control_c.o FN_search_filter_c.o

HOOKOBJS = FN_ctx_hook.o FN_compound_name_hook.o FN_ref_addr_hook.o

UTILOBJS= FN_ref_serial.o FN_ref_serial_xdr.o fns_symbol.o \
	FN_attr_serial.o FN_attr_serial_xdr.o hash.o

OBJECTS= $(IVYOBJS) $(UTILOBJS) $(IVYCOBJS) $(HOOKOBJS)

CLOBBERFILES += \
	../common/FN_attr_serial.h ../common/FN_attr_serial_xdr.c \
	../common/FN_ref_serial.h ../common/FN_ref_serial_xdr.c	\
	$(MAPFILE)

# include library definitions
include ../../Makefile.libfn

ROOTLIBDIR=	$(ROOT)/usr/lib
ROOTLIBDIR64=	$(ROOT)/usr/lib/$(MACH64)

LIBS = $(DYNLIBCCC)

# dl for dlopen; 
# nsl for xdr routines; 
# C 
LDLIBS += -ldl -lnsl -lc

pics/fns_symbol.o := CPPFLAGS += -DFNVERS=\"$(FNVERS)\"
pics/fns_symbol.o := CPPFLAGS64 += -DFNVERS=\"$(FNVERS)\"

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(DYNLIBCCC):=	DYNFLAGS += -M $(MAPFILE)


$(DYNLIBCCC):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile
