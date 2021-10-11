#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/10/14 SMI"
#
# lib/liblwp/Makefile

include ../Makefile.lib

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

SYMLINK32=	$(ROOT)/usr/lib/lwp/32
SYMLINK64=	$(ROOT)/usr/lib/lwp/64

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

TEXT_DOMAIN=	SUNW_OST_OSLIB

.KEEP_STATE:

all clean clobber: spec .WAIT $(SUBDIRS)

install: spec .WAIT $(SUBDIRS)
	$(RM) $(SYMLINK32)
	$(SYMLINK) . $(SYMLINK32)
	$(CHOWN) $(OWNER):$(GROUP) $(SYMLINK32)
	$(BUILD64)$(RM) $(SYMLINK64)
	$(BUILD64)$(SYMLINK) $(MACH64) $(SYMLINK64)
	$(BUILD64)$(CHOWN) $(OWNER):$(GROUP) $(SYMLINK64)

lint: $(SUBDIRS)

spec $(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
