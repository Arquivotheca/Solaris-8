#
#ident	"@(#)Makefile	1.2	98/10/09 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/devfsadm/Makefile
#

#
# One for each ISA.
#
SUBDIRS=	$(MACH)

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
_msg	:=	TARGET= _msg
lint	:=	TARGET= lint

.KEEP_STATE:

all install clean clobber lint _msg:	$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

