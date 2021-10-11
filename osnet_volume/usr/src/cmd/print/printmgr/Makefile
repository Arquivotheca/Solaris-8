#
# ident	"@(#)Makefile	1.4	99/07/12 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for print manager
#
PROG =		pmserver.jar pmclient.jar

include		$(SRC)/cmd/Makefile.cmd

SUBDIRS =	com bin

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint
_msg	:=	TARGET= _msg

ROOTDIRS =	$(ROOT)/usr/sadm/admin \
		$(ROOT)/usr/sadm/admin/printmgr \
		$(ROOT)/usr/sadm/admin/printmgr/classes

ROOTBIN =	$(ROOT)/usr/sadm/admin/printmgr/classes
ROOTBINPROG =   $(PROG:%=$(ROOTBIN)/%)

$(ROOTBINPROG):= FILEMODE	= 644
$(ROOTBINPROG):= OWNER		= root
$(ROOTBINPROG):= GROUP		= lp

.KEEP_STATE:

all:		$(SUBDIRS)
install:	$(SUBDIRS) $(ROOTDIRS) $(ROOTBINPROG)

clean clobber:	$(SUBDIRS)
	-$(RM) $(PROG)

lint _msg:	$(SUBDIRS)

strip:

$(PROG):
	@if [ $@ = "pmclient.jar" ]; \
	then $(JAR) cf $@ \
		com/sun/admin/pm/client/*class \
		com/sun/admin/pm/client/images/*gif 2>/dev/null; \
	else $(JAR) cf $@ \
		com/sun/admin/pm/server/*class 2>/dev/null; \
	fi

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(ROOTDIRS):
	$(INS.dir)

$(ROOTBIN)/%: %
	$(INS.file)

FRC:
