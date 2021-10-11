#
# Copyright (c) 1989 - 1997 by Sun Microsystems, Inc.
#
#pragma ident "@(#)Makefile	1.19 97/11/24 SMI"
#
# lib/fn/libfn_p/Makefile
#

include         ../../../Makefile.master

HHDRS= fn_p.hh fnselect.hh

SUBDIRS=        $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.libfn

FNHHDRS= $(HHDRS:%=$(FNINCDIR)/xfn/%)

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
install :=      TARGET= install
debug :=        TARGET = debug
tcov :=         TARGET = tcov
analyse :=      TARGET = analyse
install_h :=    TARGET = install_h
private_h :=    TARGET = private_h
check :=    	TARGET = check

.KEEP_STATE:

all debug install: install_h private_h $(SUBDIRS)
clean clobber : $(SUBDIRS)

install_h check:

private_h: $(FNINCDIR) $(FNINCDIR)/xfn $(FNHHDRS)

include ../Makefile.targ

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

