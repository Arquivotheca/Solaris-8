#
#pragma ident	"@(#)Makefile	1.1	99/05/14 SMI"
#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

all:=		TARGET=	all
install:=	TARGET=	install
clean:=		TARGET=	clean
clobber:=	TARGET=	clobber
lint:=		TARGET=	lint
install_h:=	TARGET= install_h

.PARALLEL: $(ROOTDEMOFILES)

.KEEP_STATE:

all clean clobber lint: \
		$(SUBDIRS)

install: $(SUBDIRS)

install_h: $(MACH)

$(MACH) $(MACH64):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
