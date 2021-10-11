#
# Copyright (c) 1989,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.11	99/01/25 SMI"
#
# lib/libeti/menu/Makefile
#

include		../../Makefile.lib

SUBDIRS=        spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
delete :=       TARGET= delete
install :=      TARGET= install
lint :=         TARGET= lint
catalog :=      TARGET= catalog
package :=      TARGET= package

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

# definitions for install_h target
HDRS=           menu.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=      $(HDRS:%.h=inc/%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: inc/%
	$(INS.file)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

spec $(SPEC64) $(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
