#
# Copyright (c) 1994-1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.5	99/02/17 SMI"
#
# lib/cfgadm_plugins/ac/Makefile

include		../../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
_msg :=		TARGET= _msg
catalog :=	TARGET= catalog
package :=	TARGET= package

TEXT_DOMAIN=	SUNW_OST_OSLIB
XGETFLAGS=	-a -x ac.xcl
POFILE=		ac.po
POFILES=	generic.po

SED=	sed
GREP=	grep
CP=	cp

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

_msg:	$(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE):	$(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

$(POFILES):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext */*.[ch]`
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

FRC:
