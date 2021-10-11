#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.export	1.2 99/07/23 SMI"
#
#	Makefile for ami.jar
#

PKGDIR=		com/sun/ami
MSGDIR=		usr/share/lib/locale
AMIPROPS=	com/sun/ami/AMI_Messages.properties \
	com/sun/ami/AMI_Messages_Log.properties

include $(SRC)/Makefile.master

MSGFILES=	$(AMIPROPS:%=$(ROOT)/$(MSGDIR)/%)

.KEEP_STATE:

$(MSGFILES) 	:= GROUP = sys

# make rules
all install clean clobber debug check lint install_h: FRC

catalog: _msg

_msg: $(ROOT)/$(MSGDIR)/$(PKGDIR) $(MSGFILES)

$(ROOT)/$(MSGDIR)/$(PKGDIR):
	$(INS.dir)

$(ROOT)/$(MSGDIR)/%: %
	$(INS.file)

FRC:
