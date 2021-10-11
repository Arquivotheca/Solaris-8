#
#ident	"@(#)Makefile	1.7	97/11/24 SMI"
#
# Copyright (c) 1993 - 1997 by Sun Microsystems, Inc.
#
# lib/fn/libxfn.1/Makefile
#

include         ../../../Makefile.master 

SUBDIRS=        $(MACH) $(BUILD64) $(MACH64)

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
debug := 	TARGET = debug
tcov := 	TARGET = tcov
analyse := 	TARGET = analyse
install_h := 	TARGET = install_h
private_h := 	TARGET = private_h
check := 	TARGET = check

.KEEP_STATE:

all debug install: install_h private_h $(SUBDIRS)
clean clobber : $(SUBDIRS)

install_h private_h: 

check: $(CHECKHDRS)

# include library targets
include ../Makefile.targ

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

