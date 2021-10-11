#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.22	99/01/25 SMI"
#
# lib/fn/libxfn/Makefile
#

include ../../../Makefile.master
include $(SRC)/lib/Makefile.lib

HDRS= xfn.h misc_codes.h \
        FN_ctx.h \
        FN_composite_name.h \
        FN_string.h \
        FN_ref.h \
        FN_ref_addr.h \
        FN_attribute.h \
        FN_attrset.h \
        FN_attrmodlist.h \
        FN_compound_name.h \
        FN_status.h \
        FN_identifier.h \
        FN_attrvalue.h \
        FN_nameset.h \
        FN_bindingset.h \
        FN_search_control.h \
        FN_search_filter.h

HHDRS= xfn.hh fn_xdr.hh \
        FN_ctx.hh \
        FN_composite_name.hh \
        FN_string.hh \
        FN_ref.hh \
        FN_ref_addr.hh \
        FN_attribute.hh \
        FN_attrset.hh \
        FN_attrmodlist.hh \
        FN_compound_name.hh \
        FN_status.hh \
        FN_identifier.hh \
        FN_attrvalue.hh \
        FN_nameset.hh \
        FN_bindingset.hh \
        FN_searchset.hh \
        FN_ext_searchset.hh \
        FN_search_control.hh \
        FN_search_filter.hh \
        fns_symbol.hh

SUBDIRS=        spec .WAIT $(MACH) $(BUILD64) .WAIT $(MACH64)

RPCFILES=	common/FN_ref_serial.h common/FN_ref_serial_xdr.c common/FN_ref_serial.cc \
		common/FN_attr_serial.h common/FN_attr_serial_xdr.c common/FN_attr_serial.cc

# include library definitions
include ../Makefile.libfn

ROOTDIRS= $(ROOT)/usr/include/xfn
ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)
FNHHDRS= $(HHDRS:%=$(FNINCDIR)/xfn/%)
CHECKHDRS= $(HDRS:%.h=common/%.check)

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
install :=      TARGET= install
install_h :=	TARGET = install_h
private_h :=	TARGET = private_h
check :=	TARGET = check
debug := 	TARGET = debug
tcov := 	TARGET = tcov
analyse := 	TARGET = analyse

.KEEP_STATE:

all debug install: install_h private_h $(RPCFILES) $(SUBDIRS)
clean clobber tcov analyse: $(SUBDIRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

private_h: $(FNINCDIR) $(FNINCDIR)/xfn $(FNHHDRS)

check: $(CHECKHDRS)

# For generating the RPC files in the libxfn directory

common/FN_ref_serial.h: common/FN_ref_serial.x
	cd common; $(RPCGEN) -C -h -o FN_ref_serial.h FN_ref_serial.x ; cd ..
common/FN_ref_serial_xdr.c: common/FN_ref_serial.h
	cd common; $(RPCGEN) -C -c -o FN_ref_serial_xdr.c  FN_ref_serial.x; cd .. 
common/FN_ref_serial.cc:  common/FN_ref_serial.h

common/FN_attr_serial.h:  common/FN_attr_serial.x
	cd common; $(RPCGEN) -C -h -o  FN_attr_serial.h  FN_attr_serial.x ; cd ..
common/FN_attr_serial_xdr.c:  common/FN_attr_serial.h
	cd common; $(RPCGEN) -C -c -o FN_attr_serial_xdr.c FN_attr_serial.x ; cd ..
common/FN_attr_serial.cc:  common/FN_attr_serial.h

# include library targets
include ../Makefile.targ

spec $(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
