#
#ident	"@(#)Makefile	1.7	99/05/10 SMI"
#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/dhcp/Makefile

include		../../../../Makefile.master

SUBDIRS=	dhtadm pntadm dhcpconfig

all:=		TARGET=	all
clean:=		TARGET=	clean
clobber:=	TARGET=	clobber
install:=	TARGET=	install
lint:=		TARGET=	lint
_msg:=		TARGET=	_msg

.KEEP_STATE:

all clean clobber install lint _msg:	\
		$(SUBDIRS)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
