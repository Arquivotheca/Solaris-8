#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.export	1.2 99/07/23 SMI"
#
#	Makefile for libami
#

HDRS= ami.h

SUBDIRS=	spec

MSGFILE=	libami.msg
TEXT_DOMAIN=	SUNW_OST_SYSOSAMI
CP=		cp

ROOTDIRS=	$(ROOT)/usr/include/security
ROOTHDRS=	$(HDRS:%=$(ROOTDIRS)/%)

# include library definitions
include $(SRC)/lib/Makefile.lib

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
check :=	TARGET= check
install :=      TARGET= install
lint :=      	TARGET= lint
install_h :=	TARGET = install_h
debug := 	TARGET = debug
_msg := 	TARGET = _msg

.KEEP_STATE:

all:	install_h
clean clobber debug check lint: FRC
install_h:	$(ROOTDIRS) $(ROOTHDRS)
install:	install_h $(SUBDIRS)

$(ROOTDIRS):
	$(INS.dir)

$(ROOTDIRS)/%.h: include/%.h
	$(INS.file)

catalog: _msg

spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

_msg: $(MSGDOMAIN) common/$(MSGFILE)
	$(RM) $(MSGDOMAIN)/$(MSGFILE)
	$(CP) common/$(MSGFILE) $(MSGDOMAIN)

$(MSGDOMAIN):
	$(INS.dir)

FRC:
