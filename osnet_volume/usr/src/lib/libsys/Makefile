#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	99/01/25 SMI"
#
# lib/libsys/Makefile

include		../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH)

all:=		TARGET=	all
clean:=		TARGET=	clean
clobber:=	TARGET=	clobber
install:=	TARGET=	install
lint:=		TARGET=	lint

.KEEP_STATE:

all clean clobber install lint:	\
		$(SUBDIRS)

$(SPEC) $(MACH) spec:	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
