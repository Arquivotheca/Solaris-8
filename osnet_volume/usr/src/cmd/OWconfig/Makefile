#
#ident	"@(#)Makefile	1.7	99/08/03 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.cmd

FILES=	OWconfig

ROOTETC=		$(ROOT)/etc
OPENWIND=		$(ROOTETC)/openwin
OPENWINETCD=		$(OPENWIND)/etc
SERVERD=		$(OPENWIND)/server
SERVERETCD=		$(SERVERD)/etc
DEVDATAD=		$(OPENWINETCD)/devdata
SUNWACCELD=		$(DEVDATAD)/SUNWaccel
MONITORSD=		$(SUNWACCELD)/monitors
PDPD=			$(MONITORSD)/pnp


DIRS=			\
	$(ROOTETC)	\
	$(OPENWIND)	\
	$(OPENWINETCD)	\
	$(SERVERD)	\
	$(SERVERETCD)	\
	$(DEVDATAD)	\
	$(SUNWACCELD)	\
	$(MONITORSD)	\
	$(PDPD)

OWNER=root
FILEMODE=0644
DIRMODE=0755

$(OPENWIND) := DIRMODE = 0775

OWCONFIG=$(FILES:%=$(SERVERETCD)/%)

$(SERVERETCD)/%: %
	$(INS.file)


.KEEP_STATE:

all:

install: all $(DIRS) $(OWCONFIG)

$(DIRS):
	-$(INS.dir)

clean:

lint:

include ../Makefile.targ
