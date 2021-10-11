#
#ident	"@(#)Makefile	1.6	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= look
DATADIR= dict
DATA= words

include ../Makefile.cmd

ROOTDATADIR=	$(ROOTSHLIB)/$(DATADIR)
ROOTDATA=	$(DATA:%=$(ROOTDATADIR)/%)
# Targetdirs installs /usr/lib.  Don't re-install it
#DIRS=		$(ROOTLIB) $(ROOTDATADIR)
DIRS=		$(ROOTDATADIR)

FILEMODE=	755

$(ROOTDATADIR)/$(DATA):= FILEMODE=444

.KEEP_STATE:

$(ROOTDATADIR)/% : % $(ROOTDATADIR)
	$(INS.file)

all: $(PROG) $(DATA)

install: all $(ROOTPROG) $(DIRS) $(ROOTDATA)

$(DIRS):
	$(INS.dir)

clean:

lint:	lint_PROG

include ../Makefile.targ

