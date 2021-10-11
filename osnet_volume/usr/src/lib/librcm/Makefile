#
#ident	"@(#)Makefile	1.1	99/08/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/librcm/Makefile
#
#
include	../Makefile.lib

#
# 64 bit build is turned off pending libsysevent conversion to 64 bit
#
SUBDIRS = spec .WAIT $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
all :=      	TARGET= all
install :=      TARGET= install
clean :=        TARGET= clean
clobber :=      TARGET= clobber
lint :=         TARGET= lint
test :=         TARGET= test

# definitions for install_h target
HDRS=           librcm.h librcm_impl.h librcm_event.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=      $(HDRS:%.h=%.check)

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

# install rule for install_h target

$(ROOTHDRDIR)/%: %
	$(INS.file)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
