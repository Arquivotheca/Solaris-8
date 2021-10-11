#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	98/06/14 SMI"
#

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
package :=	TARGET= package

# include library definitions
include ../../../Makefile.master

SUBDIRS=	$(MACH)	$(BUILD64) $(MACH64)

all clean clobber delete install lint package: $(SUBDIRS)

OWNER = root
GROUP = sys

.KEEP_STATE:

_msg:

catalog:

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

# include library targets
include ../../Makefile.targ
