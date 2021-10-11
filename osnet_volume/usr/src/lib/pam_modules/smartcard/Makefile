#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile 1.4     99/07/30 SMI"
#

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
package :=	TARGET= package


NOBUILDX86:sh=	if [ "$MACH" = "i386" ]; \
		then echo \\043; \
		fi

# include library definitions
include ../../../Makefile.master

MSGFILE=pam_smartcard.msg
TEXT_DOMAIN= SUNW_OST_SYSOSPAM
CP=		cp

SUBDIRS=	$(MACH)	$(BUILD64) $(MACH64)

all clean clobber delete install lint package: $(NOBUILDX86) $(SUBDIRS)

OWNER = root
GROUP = sys

.KEEP_STATE:

_msg: $(NOBUILDX86) $(MSGDOMAIN) $(MSGFILE)
	$(NOBUILDX86) $(RM) $(MSGDOMAIN)/$(MSGFILE)
	$(NOBUILDX86) $(CP) $(MSGFILE) $(MSGDOMAIN)

catalog: _msg

$(MSGDOMAIN): FRC
	$(INS.dir)

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

# include library targets
include ../../Makefile.targ
