#
# Copyright (c) 1990-1992,1994,1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.16	99/01/25 SMI"
#
# lib/libmp/Makefile
#
# The builds occur in isa subdirectories.
#

include ../../Makefile.master
include	../Makefile.lib

SUBDIRS = spec .WAIT $(MACH) $(BUILD64)  .WAIT $(MACH64)

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
test :=		TARGET= test

.KEEP_STATE:

all install clean clobber lint test: $(SUBDIRS)

spec $(MACH) $(MACH64) $(SPEC64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
