#
# ident	"@(#)Makefile	1.7	98/08/12 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile definitions for Starfire Network Console related items.
#
# cmd/dr_daemon/Makefile
#
# These utilities are specific to the Ultra-Enterprise-10000 platform at this
# time.

SUBDIRS=	sparc

# conditional assignments
all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all install lint clean clobber: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
