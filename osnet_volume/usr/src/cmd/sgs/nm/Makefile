#
#pragma ident	"@(#)Makefile	1.5	99/05/04 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/nm/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

CSRC=		common/nm.c

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
package :=	TARGET= package

# for messaging catalog
#
POFILE= nm.po

.KEEP_STATE:

all clean clobber install lint package:	$(SUBDIRS)

catalog:	$(POFILE)

$(POFILE):
	$(RM) $@
	$(XGETTEXT) $(XGETFLAGS) $(CSRC)
	sed "/^domain/d" < messages.po > $@
	$(RM) messages.po

$(MACH) $(MACH64): FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
