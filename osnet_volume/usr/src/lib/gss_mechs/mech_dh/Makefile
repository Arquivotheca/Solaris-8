#
#ident	"@(#)Makefile	1.2	98/05/26 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/gss_mechs/mech_dh/Makefile
#

SUBDIRS= backend .WAIT dh192 dh640 dh1024 
MSGSUBDIRS = backend

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg

.KEEP_STATE:

all clean clobber lint: $(SUBDIRS)

install: $(SUBDIRS)

_msg: $(MSGSUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET); echo

FRC:
