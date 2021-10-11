#
# Copyright (c) 1989,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.6	99/01/25 SMI"
#
# lib/libplot/vt0/Makefile
#
#LIBRARY= libvt0.a

include		../../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)


$(MACH) $(MACH64) spec:	FRC
	 @cd $@; pwd; $(MAKE) $(TARGET)

FRC:

