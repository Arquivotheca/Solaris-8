#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# cmd/krb5/kadmin/Makefile

SUBDIRS= ktutil kpasswd

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package
_msg:=          TARGET= _msg

_msg:	$(SUBDIRS)

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

# install rule for install_h target

install: $(SUBDIRS)
	-$(RM) $(ROOTUSRBIN)

check: $(CHECKHDRS)
	
$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
