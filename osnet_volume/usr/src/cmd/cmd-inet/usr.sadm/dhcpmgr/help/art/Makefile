#
#ident	"@(#)Makefile	1.6	99/05/14 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/help/art/Makefile

include $(SRC)/cmd/Makefile.cmd

HELPFILES= 	bannersmc.gif \
		dot1.gif \
		dot2.gif \
		folder.gif \
		macro2.gif \
		macroflow.gif \
		tip2.gif

ROOTHELPDIR=    $(ROOT)/usr/sadm/admin/dhcpmgr/help/art

ROOTDIRS= 	$(ROOTHELPDIR)

IHELPFILES=	$(HELPFILES:%=$(ROOTHELPDIR)/%)

MSGDIR=		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr/client/help/art
MSGDIRS =       $(ROOT)/usr/share/lib/locale \
                $(ROOT)/usr/share/lib/locale/com \
                $(ROOT)/usr/share/lib/locale/com/sun \
		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr \
		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr/client \
		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr/client/help \
		$(MSGDIR)

MSGFILES=	$(HELPFILES)
MSGS=		$(MSGFILES:%=$(MSGDIR)/%)

FILEMODE= 0444

$(ROOTHELPDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(HELPFILES)

install: all $(ROOTDIRS) $(IHELPFILES)

$(ROOTDIRS):
	$(INS.dir)

clean clobber lint:

_msg: $(MSGDIRS) $(MSGS)

$(MSGDIR)/%: %
	$(INS.file)

$(MSGDIRS):
	$(INS.dir)

