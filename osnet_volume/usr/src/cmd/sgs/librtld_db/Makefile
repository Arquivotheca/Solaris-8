#
#pragma ident	"@(#)Makefile	1.5	99/05/04 SMI"
#
# Copyright (c) 1996,1999 by Sun Microsystems, Inc.
# All rights reserved.

include		../../../Makefile.master

SUBDIRS=	$(MACH) rdb_demo
 
all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

.KEEP_STATE:

all clean clobber delete install lint catalog package: \
		$(SUBDIRS)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)
		$(BUILD64)@if [ -d $(MACH64) ]; then                      \
		$(BUILD64)	cd $(MACH64); pwd; $(MAKE) $(TARGET);   \
		$(BUILD64)else /bin/true; fi


FRC:
