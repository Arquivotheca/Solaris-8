#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.2	99/12/11 SMI" 
#
# lib/krb5/kadm5/clnt/Makefile

include		../../../../Makefile.master
include		../../../Makefile.lib

SUBDIRS=	spec $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
package :=	TARGET= package

LIBRARY= 	libkadm5clnt.a
TEXT_DOMAIN=	SUNW_OST_OSLIB
POFILE=		$(LIBRARY:%.a=%.po)
POFILES=	generic.po

SED=	sed
GREP=	grep
CP=	cp

.KEEP_STATE:

all clean clobber delete install lint package: $(SUBDIRS)

_msg: $(MSGDOMAIN) .WAIT $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po: FRC
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext ../*.[ch] *.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

# definitions for install_h target
HDRS=		client_internal.h
CHECKHDRS=	$(HDRS:%.h=%.check)

install_h:

#check: $(CHECKHDRS)

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

$(MSGDOMAIN):
	$(INS.dir)
