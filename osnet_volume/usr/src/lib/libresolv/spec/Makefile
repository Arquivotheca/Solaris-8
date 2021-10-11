#ident	"@(#)Makefile	1.1	99/01/25 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libresolv/spec/Makefile

#
# no sparcv9 build of libresolv
# if it is desirable to build one then uncomment the line below and delete
# the rest of the lines.
#
#include	$(SRC)/lib/Makefile.spec.arch

i386_ARCHITECTURES =	i386
sparc_ARCHITECTURES =	sparc

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $($(MACH)_ARCHITECTURES)

# spec files do not have msgs or catalogs
_msg catalog:

$($(MACH)_ARCHITECTURES): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
