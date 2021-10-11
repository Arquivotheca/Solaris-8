#
#ident	"@(#)Makefile	1.8	99/03/30 SMI"
#
# Copyright (c) 1996,1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../../../Makefile.cmd

ETCNFS= $(ROOTETC)/nfs
VARNFS= $(ROOT)/var/nfs
TXTS= nfssec.conf
NFSTXTS= nfslog.conf

IETCFILES=			$(TXTS:%=$(ROOTETC)/%)
INFSETCFILES=			$(NFSTXTS:%=$(ROOTETC)/nfs/%)

$(VARNFS) := GROUP = bin
FILEMODE= 0644
OWNER= root
GROUP= sys

all: $(TXTS) $(NFSTXTS)

install: all $(IETCFILES) $(ETCNFS) $(INFSETCFILES) $(VARNFS)

$(ROOTETC)/%: %
	$(INS.file)

$(ROOTETC)/nfs/%: %
	$(INS.file)

$(ETCNFS):
	$(INS.dir)

$(VARNFS):
	$(INS.dir)

.KEEP_STATE:

FRC:

clean clobber lint:
