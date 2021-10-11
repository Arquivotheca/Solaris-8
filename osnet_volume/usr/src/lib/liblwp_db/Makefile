#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/10/14 SMI"
#
# lib/liblwp_db/Makefile

include ../Makefile.lib

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

TEXT_DOMAIN=	SUNW_OST_OSLIB

.KEEP_STATE:

all clean clobber: spec .WAIT $(SUBDIRS)

install: spec .WAIT $(SUBDIRS)

lint: $(SUBDIRS)

spec $(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
