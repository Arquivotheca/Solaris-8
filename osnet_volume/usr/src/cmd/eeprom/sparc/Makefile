#
#ident	"@(#)Makefile	1.10	99/10/04 SMI"
#
# Copyright (c) 1993, 1995 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/eeprom/sparc/Makefile
#
SRCDIR=..

SUBDIRS= sun4m sun4d sun4u

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
