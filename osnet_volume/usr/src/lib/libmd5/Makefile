#
# ident	"@(#)Makefile	1.1	99/07/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5/Makefile
#

include $(SRC)/Makefile.master

SUBDIRS= spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
install :=	TARGET= install

.KEEP_STATE:

.PARALLEL: $(MACH) $(MACH64)

all clean clobber lint install: $(SUBDIRS)

spec $(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

_msg install_h check:

FRC:
