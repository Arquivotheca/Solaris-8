#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.2 97/11/20 SMI"
#
#	lib/fn/context/onc_ns/files/nisplus
#
include         ../../../../../Makefile.master

SUBDIRS=        $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../../../Makefile.libfn

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
install :=      TARGET= install
install_h :=	TARGET = install_h
debug := 	TARGET = debug
tcov := 	TARGET = tcov
analyse := 	TARGET = analyse

.KEEP_STATE:

clean clobber : $(SUBDIRS)
all debug install: install_h $(SUBDIRS)

install_h:

# include library targets
include ../../../Makefile.targ

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
