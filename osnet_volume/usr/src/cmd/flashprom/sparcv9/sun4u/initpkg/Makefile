#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/06/18 SMI"
#

FLASH=		flashprom
SHFILE=		mk.rc2.d
CMDDIR=		../../../..

include $(CMDDIR)/Makefile.cmd

POFILE=
CLOBBERFILES=	Nothing_to_remove

ROOTETCINITD=		$(ROOTETC)/init.d
ROOTETCINITDPROG=	$(FLASH:%=$(ROOTETCINITD)/%)

FILEMODE = 0744
OWNER = root
GROUP = sys

.KEEP_STATE:

all:		$(FLASH)

install:	all $(ROOTETCINITDPROG) $(SHFILE)

$(ROOTETCINITD)/%:	%
		$(INS.file)

$(SHFILE):	FRC
	@sh $@.sh ROOT=$(ROOT) CH=$(CH)

FRC:

clean:

clobber:

lint:

include $(CMDDIR)/Makefile.targ
