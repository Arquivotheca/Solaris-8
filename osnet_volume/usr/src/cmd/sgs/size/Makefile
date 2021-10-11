#
#pragma ident	"@(#)Makefile	1.4	99/05/04 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/size/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
package:=	TARGET= package

.KEEP_STATE:

all clean clobber install lint package:	$(SUBDIRS)

$(MACH) $(MACH64): FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
