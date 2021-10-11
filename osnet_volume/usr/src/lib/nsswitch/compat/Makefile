#
#ident	"@(#)Makefile	1.10	97/08/12 SMI"
#
# Copyright (c) 1991 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/compat/Makefile

include     ../../../Makefile.master

FILES_SUBDIRS=    $(MACH) $(BUILD64) $(MACH64)

all:=		TARGET= all
clean:=		TARGET= clean
clobber:=	TARGET= clobber
install:=	TARGET= install
lint:=		TARGET= lint

.KEEP_STATE:

all clean clobber install lint: $(FILES_SUBDIRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
