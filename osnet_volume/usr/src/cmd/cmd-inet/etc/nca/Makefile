#
#ident	"@(#)Makefile	1.1	99/08/06 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/etc/nca/Makefile
#

NCADIR=		nca
ETCPROG=	nca.if ncakmod.conf ncalogd.conf

include ../../../Makefile.cmd

ETCNCADIR=	$(ROOTETC)/$(NCADIR)
ETCNCAPROG=	$(ETCPROG:%=$(ETCNCADIR)/%)

FILEMODE=	0644
OWNER=		root
GROUP=		sys

.KEEP_STATE:

all:		$(ETCPROG)

install:	all $(ETCNCADIR) $(ETCNCAPROG)

$(ETCNCADIR)/% : %
		$(INS.file)

$(ETCNCADIR):
		$(INS.dir)

FRC:

clean clobber lint:
