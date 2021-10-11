#
#ident	"@(#)Makefile	1.35	99/01/25 SMI"
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsocket/Makefile
#

SUBDIRS =	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

SED =	sed
CP =	cp
GREP =	grep

ROOTLINTDIR= $(ROOTLIBDIR)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
_msg :=		TARGET= _msg
package :=	TARGET= package

# include library definitions
include ../Makefile.lib

TEXT_DOMAIN = 	SUNW_OST_NETRPC
POFILE= libsocket.po
POFILES= generic.po 

.KEEP_STATE:

all install clean clobber delete lint package:	$(SUBDIRS)


# install rule for lint library target
$(ROOTLINTDIR)/%: common/%
	$(INS.file)

_msg: $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE):	$(POFILES)
	$(RM) -f $@
	$(CAT) $(POFILES) > $@

generic.po:
	$(RM) messages.po
	$(XGETTEXT) -a $(XGETFLAGS) `$(GREP) -l gettext */*.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)

$(MACH) $(MACH64) spec:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

