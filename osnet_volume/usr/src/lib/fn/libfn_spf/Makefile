#
# Copyright (c) 1989 - 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.16 97/11/24 SMI"
#
# lib/fn/libfn_spf/Makefile
#

include		../../../Makefile.master

HDRS= fn_spi.h FN_syntax_standard.h

HHDRS= fn_spi.hh \
        FN_syntax_standard.hh\
        FN_status_svc.hh FN_compound_name_standard.hh \
        FN_namelist_svc.hh FN_bindinglist_svc.hh \
        FN_ext_searchlist_svc.hh FN_searchlist_svc.hh \
        FN_multigetlist_svc.hh FN_valuelist_svc.hh

SUBDIRS=	$(MACH)	$(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.libfn

ROOTDIRS= $(ROOT)/usr/include/xfn
ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)
FNHHDRS= $(HHDRS:%=$(FNINCDIR)/xfn/%)
CHECKHDRS= $(HDRS:%.h=common/%.check)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
debug :=        TARGET = debug
tcov :=         TARGET = tcov
analyse :=      TARGET = analyse
install_h :=    TARGET = install_h
private_h :=    TARGET = private_h
check :=    	TARGET = check

.KEEP_STATE:

all debug install: install_h private_h $(SUBDIRS)
clean clobber : $(SUBDIRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

private_h: $(FNINCDIR) $(FNINCDIR)/xfn $(FNHHDRS)

check: $(CHECKHDRS)

# include library targets
include ../Makefile.targ

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
