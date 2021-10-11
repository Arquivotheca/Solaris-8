#
# Copyright (c) 1992-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.19	99/08/08 SMI"
#
# lib/libpam/Makefile
#

include ../../Makefile.master
include ../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

all clean clobber delete install lint catalog package: $(SUBDIRS)

INS.conf=	$(INS) -s -m $(FILEMODE) -f $(@D) $<
$(CH)INS.conf=	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

# install pam.conf
ETCFILES= pam.conf
ROOTETC= $(ROOT)/etc
IETCFILES= $(ETCFILES:%=$(ROOTETC)/%)
$(IETCFILES) := FILEMODE = 644
$(IETCFILES) := OWNER = root
$(IETCFILES) := GROUP = sys

HDRS= pam_appl.h pam_modules.h pam_impl.h

ROOTDIRS= $(ROOT)/usr/include/security

ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)
$(ROOTHDRS) := FILEMODE = 644

CHECKHDRS= $(HDRS:%.h=%.check)

CP=cp
TEXT_DOMAIN=	SUNW_OST_SYSOSPAM
MSGFILE=libpam.msg

#install rule
$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

install: install_h $(ROOTLIBS) $(IETCFILES)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS)

check:	$(CHECKHDRS)

$(ROOTDIRS):
	$(INS.dir)

$(ROOTETC)/%: %
	$(INS.conf)

$(MACH) $(MACH64) spec:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

#rules to install message table
_msg: $(MSGDOMAIN) $(MSGFILE)
	$(RM) $(MSGDOMAIN)/$(MSGFILE)
	$(CP) $(MSGFILE) $(MSGDOMAIN)

$(MSGFILE):

$(MSGDOMAIN):
	$(INS.dir)
