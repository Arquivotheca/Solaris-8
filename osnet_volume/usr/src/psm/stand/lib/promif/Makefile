#
#pragma ident	"@(#)Makefile	1.3	99/05/04 SMI"
#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/promif/Makefile
#

include ../../../../Makefile.master

sparcv7_ARCHITECTURES = sparc
sparcv9_ARCHITECTURES = sparcv9
sparc_ARCHITECTURES = $(sparcv9_ARCHITECTURES) $(sparcv7_ARCHITECTURES)
i386_ARCHITECTURES = i386
ia64_ARCHITECTURES = ia64
$(BUILD64)i386_ARCHITECTURES += $(ia64_ARCHITECTURES)

SUBDIRS = $($(MACH)_ARCHITECTURES)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
