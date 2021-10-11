#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.7	99/01/25 SMI"
#
# Copyright (c) 1989-1998 by Sun Microsystems, Inc.
#
# lib/libplot/plot/Makefile
#
include		../../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all clean clobber delete install lint: $(SUBDIRS)


$(MACH) $(MACH64) spec:	FRC
	 @cd $@; pwd; $(MAKE) $(TARGET)

FRC:
