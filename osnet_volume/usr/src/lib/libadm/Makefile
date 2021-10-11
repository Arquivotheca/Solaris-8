#
# Copyright (c) 1990,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.17	99/05/04 SMI"
#
# lib/libadm/Makefile

include		../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

