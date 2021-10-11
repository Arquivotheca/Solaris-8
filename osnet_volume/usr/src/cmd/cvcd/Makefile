#
# ident	"@(#)Makefile	1.2	97/10/17 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile definitions for Starfire Network Console related items.
#
# cmd/cvcd/Makefile
#
# These utilities are specific to the Ultra-Enterprise-10000 platform at this
# time.
#
SUBDIRS	=	sparc

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install lint clean clobber: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
