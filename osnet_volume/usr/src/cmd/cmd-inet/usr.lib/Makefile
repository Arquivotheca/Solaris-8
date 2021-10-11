#
#pragma ident	"@(#)Makefile	1.6	99/03/31 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/Makefile

SUBDIRS=	in.dhcpd in.ndpd in.ripngd slpd
MSGSUBDIRS=	in.dhcpd

include ../../Makefile.cmd

POFILES=        in.dhcpd/in.dhcpd.po
POFILE=         usr.lib.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=		TARGET= _msg

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS) 

_msg: $(MSGSUBDIRS) 

_msg:
	cd in.dhcpd; $(MAKE) $(MFLAGS) $(TARGET)
	$(RM) $(POFILE)
	cat $(POFILES)  > $(POFILE)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
