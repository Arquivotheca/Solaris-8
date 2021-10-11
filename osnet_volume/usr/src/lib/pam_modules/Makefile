#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.11	99/12/11 SMI"
#
# lib/pam_modules/Makefile
#

SUBDIRS= unix rhosts_auth dial_auth ldap ami smartcard sample krb5 roles

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package
_msg :=		TARGET= _msg

.KEEP_STATE:

all clean clobber lint catalog package install _msg: $(SUBDIRS)

install: all

MSGSUBDIRS=unix krb5
_msg: $(MSGSUBDIRS)
catalog: _msg

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
