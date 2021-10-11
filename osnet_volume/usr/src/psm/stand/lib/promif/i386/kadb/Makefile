#
#ident	"@(#)Makefile	1.2	95/06/30 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/i386/promif/boot/Makefile
#

include ../../../../../../Makefile.master

# Sigh.  This is a gross hack to build different
# flavours of the firmware interface.

SUBDIRS	= common

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
