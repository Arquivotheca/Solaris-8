#
# Copyright (c) 1992-1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	00/09/14 SMI" 
#

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
package :=	TARGET= package

# include library definitions
include ../../../Makefile.master

MSGFILE=pam_krb5.msg
TEXT_DOMAIN= SUNW_OST_SYSOSPAM
POFILE= pam_krb5.po
POFILES= generic.po
KWARNFILES= kwarnd.h kwarnd_clnt_stubs.c kwarnd_handle.c kwarnd_xdr.c \
	kwarnd_clnt.c
FULLKWARNFILES= $(KWARNFILES:%=$(SRC)/cmd/krb5/kwarn/%)

SUBDIRS=	$(MACH)	$(BUILD64) $(MACH64)

all clean clobber delete install lint package: $(SUBDIRS)

OWNER = root
GROUP = sys

.KEEP_STATE:

_msg: $(MSGDOMAIN) .WAIT $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch] $(FULLKWARNFILES)`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)

catalog:

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)


FRC:

# include library targets
include ../../Makefile.targ

