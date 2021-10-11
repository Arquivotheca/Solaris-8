#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident   "@(#)Makefile 1.9     99/07/07 SMI"
#
# cmd/ldap/Makefile

include ../Makefile.cmd

SUBDIRS=	$(MACH)	
POFILE=		ldapcmd.po
NS_FILES=	ldapclient ldaplist mapping printResult
POFILES=	$(NS_FILES:%=ns_ldap/%.po)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
_msg :=		TARGET= _msg
package :=	TARGET= package

.KEEP_STATE:

all clobber delete install lint catalog package: $(SUBDIRS)

clean:	$(SUBDIRS)
	$(RM) $(POFILES) $(POFILE)

# install rule for install_h target

# install: $(SUBDIRS)
# 	-$(RM) $(ROOTUSRBIN)

check: $(CHECKHDRS)
	
$(MACH) :	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

FRC:

include ../Makefile.targ
