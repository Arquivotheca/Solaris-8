#
#ident	"@(#)Makefile	1.1	95/02/03 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/eeprom/i386/Makefile
#
SUBDIRS= i86pc

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
