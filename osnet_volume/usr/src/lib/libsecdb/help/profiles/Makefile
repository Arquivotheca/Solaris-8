# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/07/09 SMI"
#
# lib/libsecdb/help/profiles/Makefile
#

include ../../../../Makefile.master

HTMLENTS = \
	All.html \
	AuditControl.html \
	AuditReview.html \
	DevMgmt.html \
	PrinterMgmt.html \
	index.html

HELPDIR =	$(ROOT)/usr/lib/help
PROFDIR =	$(HELPDIR)/profiles
LOCALEDIR =	$(PROFDIR)/locale
CDIR =		$(LOCALEDIR)/C
DIRS =		$(HELPDIR) $(PROFDIR) $(LOCALEDIR) $(CDIR)
HELPFILES=$(HTMLENTS:%=$(CDIR)/%)
$(HELPFILES) := FILEMODE = 0444
$(HELPFILES) := OWNER = root
$(HELPFILES) := GROUP = bin

.KEEP_STATE:

all:	$(HTMLENTS)

install:	all $(DIRS) $(HELPFILES)

$(CDIR)/%: %
	$(INS.file)

$(DIRS):
	$(INS.dir)

clean clobber lint:
