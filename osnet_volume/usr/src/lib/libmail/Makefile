#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident  "@(#)Makefile 1.3     99/03/18 SMI"
#
# lib/libmail/Makefile

include		../../Makefile.master
include		../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)
LINTDIRS=	$(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint
_msg :=		TARGET= _msg
package :=	TARGET= package

LIBRARY= 	libmail.a
TEXT_DOMAIN=	SUNW_OST_OSLIB
XGETFLAGS=	-a
POFILE=		$(LIBRARY:.a=.po)
POFILES=	generic.po

SED=	sed
CP=	cp

.KEEP_STATE:

all clean clobber install package: $(SUBDIRS)

lint: $(LINTDIRS)

# definitions for install_h target
HDRS=		maillock.h libmail.h s_string.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)

# install rule for install_h target
$(ROOTHDRDIR)/%: inc/%
	$(INS.file)

install_h: $(ROOTHDRS)

# definitions for check
PUBHDRS=	maillock.h
CHECKHDRS=	$(PUBHDRS:%.h=inc/%.check)

check: $(CHECKHDRS)

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

_msg:	$(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE):	$(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

$(POFILES):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) *.[ch]* */*.[ch]*
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

FRC:
