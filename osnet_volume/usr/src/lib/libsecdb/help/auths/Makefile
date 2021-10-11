# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/06/28 SMI"
#
# lib/libsecdb/help/auths/Makefile
#

include ../../../../Makefile.master

HTMLENTS = \
	AuditConfig.html \
	AuditHeader.html \
	AuditRead.html \
	DevAllocHeader.html \
	DevAllocate.html \
	DevConfig.html \
	DevGrant.html \
	DevRevoke.html \
	JobHeader.html \
	JobsAdmin.html \
	JobsGrant.html \
	JobsUser.html \
	LoginEnable.html \
	LoginHeader.html \
	LoginRemote.html \
	PriAdmin.html \
	ProfmgrAssign.html \
	ProfmgrHeader.html \
	ProfmgrWrite.html \
	RoleAssign.html \
	RoleHeader.html \
	RoleWrite.html \
	SysDate.html \
	SysHeader.html \
	SysShutdown.html \
	index.html

HELPDIR=$(ROOT)/usr/lib/help
AUTHDIR=$(HELPDIR)/auths
LOCALEDIR=$(AUTHDIR)/locale
CDIR=$(LOCALEDIR)/C
DIRS=$(HELPDIR) $(AUTHDIR) $(LOCALEDIR) $(CDIR)
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
