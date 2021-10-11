#
#ident	"@(#)Makefile	1.13	99/06/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

CRON_ENT= adm root sys
CRON_LIB= .proto at.deny cron.deny queuedefs
ETC_SCRIPT= group ttysrch
SUBDIRS= sun

include ../Makefile.cmd

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

CROND= $(ROOT)/var/spool/cron
CRONTABD= $(ROOT)/var/spool/cron/crontabs
CRONLIBD= $(ROOT)/etc/cron.d
DIRS= $(CROND) $(CRONTABD) $(CRONLIBD) 

CRONTABS=	$(CRON_ENT:%=$(CRONTABD)/%)
CRONLIBS=	$(CRON_LIB:%=$(CRONLIBD)/%)
ETCSCRIPTS=	$(ETC_SCRIPT:%=$(ROOTETC)/%)

FILEMODE= 0644
OWNER= root
GROUP= sys

$(CRONLIBD)/.proto :=	FILEMODE =	0744

.KEEP_STATE:

all: $(CRON_ENT) $(CRON_LIB) $(ETC_SCRIPT) $(SUBDIRS)

install: all $(DIRS) $(CRONTABS) $(CRONLIBS) $(ETCSCRIPTS) $(SUBDIRS) 

$(CRONTABD)/% : %
	$(INS.file)

$(CRONLIBD)/% : %
	$(INS.file)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

$(DIRS):
	$(INS.dir)

FRC:

clean:

lint:

clobber:
