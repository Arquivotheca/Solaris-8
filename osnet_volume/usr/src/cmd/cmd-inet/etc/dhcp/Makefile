#
#ident	"@(#)Makefile	1.1	99/04/09 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/etc/dhcp/Makefile
#

DHCPDIR=	dhcp
ETCPROG=	inittab

include ../../../Makefile.cmd

ETCDHCPDIR=	$(ROOTETC)/$(DHCPDIR)
ETCDHCPPROG=	$(ETCPROG:%=$(ETCDHCPDIR)/%)

FILEMODE=	0644
OWNER=		root
GROUP=		sys

.KEEP_STATE:

all:		$(ETCPROG)

install:	all $(ETCDHCPDIR) $(ETCDHCPPROG)

$(ETCDHCPDIR)/% : %
		$(INS.file)

$(ETCDHCPDIR):
		$(INS.dir)

FRC:

clean clobber lint:
