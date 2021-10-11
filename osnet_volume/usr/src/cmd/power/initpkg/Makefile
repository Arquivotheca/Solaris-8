#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	99/06/18 SMI"
#

PROG=		power
SHFILE=		mk.rc2.d

include ../../Makefile.cmd


ROOTETCINITD=		$(ROOTETC)/init.d
ROOTETCINITDPROG=	$(PROG:%=$(ROOTETCINITD)/%)

FILEMODE = 0744
OWNER = root
GROUP = sys

.KEEP_STATE:

all:

install:	all $(ROOTETCINITDPROG) $(SHFILE)

$(ROOTETCINITD)/%:	%
		$(INS.file)

$(SHFILE):	FRC
	@sh $@.sh ROOT=$(ROOT) CH=$(CH)

FRC:

clean:

clobber:

lint:

include ../../Makefile.targ
