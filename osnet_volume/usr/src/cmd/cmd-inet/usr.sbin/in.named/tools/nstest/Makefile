#
#
#pragma ident   "@(#)Makefile 1.1     97/12/18 SMI"
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# in.named/tools/nstest/Makefile
#

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)


all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package


.KEEP_STATE:

all: $(SUBDIRS)

install: all $(SUBDIRS)

clean clobber delete lint catalog package:	$(SUBDIRS)


$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
 
FRC:
