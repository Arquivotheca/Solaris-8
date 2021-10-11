#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.export	1.1 99/07/23 SMI"
#
#	Makefile for PAM AMI module
#

include ../../../Makefile.master

MSGFILE=pam_ami.msg
TEXT_DOMAIN= SUNW_OST_SYSOSPAM
CP=		cp

all debug clean clobber delete install lint package: FRC

.KEEP_STATE:

_msg: $(MSGDOMAIN) $(MSGFILE)
	$(RM) $(MSGDOMAIN)/$(MSGFILE)
	$(CP) $(MSGFILE) $(MSGDOMAIN)

$(MSGFILE):

$(MSGDOMAIN):
	$(INS.dir)

catalog: _msg

FRC:
