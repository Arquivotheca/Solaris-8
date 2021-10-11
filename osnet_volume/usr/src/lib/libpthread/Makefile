#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.6	99/01/25 SMI"
#

include		../Makefile.lib

all:=		TARGET=	all
install:=	TARGET=	install
clean:=		TARGET=	clean
clobber:=	TARGET=	clobber
lint:=		TARGET=	lint

.KEEP_STATE:

all clean clobber install lint:	spec lib32 $(BUILD64) lib64

lib32:	FRC
	@cd $(MACH); pwd; $(MAKE) $(TARGET)

lib64:	FRC
	@cd $(MACH64); pwd; $(MAKE) $(TARGET)

spec:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
FRC:
