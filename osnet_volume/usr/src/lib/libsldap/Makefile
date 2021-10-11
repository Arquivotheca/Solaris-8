#
# ident	"@(#)Makefile	1.2	00/09/14 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsldap/Makefile
#

include			../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
_msg :=		TARGET= _msg
package :=	TARGET= package

LIBRARY=	libsldap.a
TEXT_DOMAIN =	SUNW_OST_OSLIB
XGETFLAGS=
POFILE=		$(LIBRARY:.a=.po)
POFILES=	generic.po

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

$(MACH) $(MACH64) spec:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

_msg:	$(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(RM) generic.po
	cp $(POFILE) $(MSGDOMAIN)

$(POFILE): $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

$(MSGDOMAIN):
	$(INS.dir)

$(POFILES):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS)  common/*.[ch]*
	sed "/^domain/d" < messages.po  > $@
	$(RM) messages.po

FRC:

