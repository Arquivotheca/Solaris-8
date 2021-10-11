#
#pragma	ident	"@(#)Makefile	1.1	97/06/30 SMI"
#
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/promif/sparcv9/Makefile
#

include ../../../../../Makefile.master

SUBDIRS	= ieee1275

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
