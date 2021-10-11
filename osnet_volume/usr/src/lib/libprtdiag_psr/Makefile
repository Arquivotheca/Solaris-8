#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/04/28 SMI"
#
# lib/libprtdiag_psr/Makefile

include		../../Makefile.master

SUBDIRS=	$(MACH) 

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
package :=	TARGET= package

.KEEP_STATE:

all clean clobber delete install lint package: $(SUBDIRS)

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
