#
#pragma ident	"@(#)Makefile	1.4	99/05/19 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/Makefile

SUBDIRS=	dhcpmgr

include ../../Makefile.cmd
#
# Message catalog
#
POFILES= dhcpmgr/lib/dhcpmgr.po
POFILE= usr.sadm.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=		TARGET= _msg

.KEEP_STATE:

.PARALLEL: $(SUBDIRS)

all install clean clobber lint: $(SUBDIRS)

#
# message catalog
#
_msg: $(SUBDIRS) .WAIT $(POFILE)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
