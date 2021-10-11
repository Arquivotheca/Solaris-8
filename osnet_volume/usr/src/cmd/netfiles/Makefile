#
#ident	"@(#)Makefile	1.5	99/07/07 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#

TXTS= nsswitch.conf netconfig hosts services

include ../Makefile.cmd

FILES= hosts services
ETCFILES= netconfig nsswitch.conf nsswitch.files nsswitch.nis nsswitch.nisplus \
		nsswitch.dns nsswitch.ldap

ROOTNET=    $(ROOTETC)/net
TICLTS=     $(ROOTNET)/ticlts
TICOTS=     $(ROOTNET)/ticots
TICOTSORD=  $(ROOTNET)/ticotsord

ROOTDIRS= \
	$(ROOTNET) \
	$(TICLTS) \
	$(TICOTS) \
	$(TICOTSORD)

ITICLTSFILES=		$(FILES:%=$(TICLTS)/%)
ITICOTSFILES=		$(FILES:%=$(TICOTS)/%)
ITICOTSORDFILES=	$(FILES:%=$(TICOTSORD)/%)
IETCFILES=			$(ETCFILES:%=$(ROOTETC)/%)

FILEMODE= 0644
OWNER= root
GROUP= sys

$(TICLTS)/% $(TICOTS)/% $(TICOTSORD)/% $(ROOTETC)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(TXTS)

install: all $(ROOTDIRS) $(ITICLTSFILES) $(ITICOTSFILES) $(ITICOTSORDFILES) \
			$(IETCFILES) 

$(ROOTDIRS):
	$(INS.dir)

FRC:

clean clobber lint:

