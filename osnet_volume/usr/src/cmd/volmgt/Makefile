#
# Copyright 1996, Sun Microsystems
# All rights reserved.
#
# Makefile definitions for volume management
#
#ident "@(#)Makefile	1.3	96/01/16 SMI"
#
# cmd/volmgt/Makefile
#

include ../Makefile.cmd

SUBDIRS1 = util vold rmm
SUBDIRS2 = etc
SUBDIRS	= $(SUBDIRS1) $(SUBDIRS2)

CAT=	cat

#
# for messaging catalog
#
POFILE= volmgt.po
POFILES= $(SUBDIRS1:%=%/%.po)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
cstyle :=	TARGET= cstyle
_msg :=		TARGET= catalog

.KEEP_STATE:

all install cstyle lint clean clobber: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

_msg: $(SUBDIRS1)
	$(RM) $(POFILE)
	$(CAT) $(POFILES) > $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)/$(POFILE)

FRC:
