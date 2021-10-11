#
#ident	"@(#)Makefile	1.2	92/07/14 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#

SHFILES=	getmajor
ALL=		$(SHFILES)
CLOBBERFILES=	$(ALL)

include ../Makefile.cmd

USRSBINF=	getmajor

OWNER=		root
GROUP=		sys
FILEMODE=	0755

ROOTUSRSBINF=	$(USRSBINF:%=$(ROOTUSRSBIN)/%)

.KEEP_STATE:

all: $(ALL)

install: $(ALL) $(ROOTUSRSBINF)

FRC:

clean lint:

include	../Makefile.targ
