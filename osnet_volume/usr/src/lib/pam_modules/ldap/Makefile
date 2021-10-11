#
# ident	"@(#)Makefile	1.1	99/07/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/pam_modules/ldap/Makefile
#

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
delete :=       TARGET= delete
install :=      TARGET= install
lint :=         TARGET= lint
package :=      TARGET= package

# include library definitions
include ../../../Makefile.master

MSGFILE=pam_ldap.msg
TEXT_DOMAIN= SUNW_OST_SYSOSPAM
CP=	cp

SUBDIRS=        $(MACH) $(BUILD64) $(MACH64)

all clean clobber delete install lint package: $(SUBDIRS)

OWNER = root
GROUP = sys

.KEEP_STATE:

_msg: $(MSGDOMAIN) $(MSGFILE)
	$(RM) $(MSGDOMAIN)/$(MSGFILE)
	$(CP) $(MSGFILE) $(MSGDOMAIN)

$(MSGFILE):

$(MSGDOMAIN):
	$(INS.dir)

catalog: _msg

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

# include library targets
include ../../Makefile.targ
