#
#ident	"@(#)Makefile	1.12	99/03/30 SMI"
#
# Copyright (c) 1989-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/eeprom/Makefile
#
SUBDIRS= $(MACH)

include ../Makefile.cmd

PROG		= eeprom
FILEMODE	= 555
GROUP		= sys

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : $(SUBDIRS)

all install :	$(PROG)

install:	$(ROOTUSRSBINPROG)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
