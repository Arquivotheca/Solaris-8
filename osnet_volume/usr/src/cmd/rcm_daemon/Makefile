#
#ident	"@(#)Makefile	1.1	99/08/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/rcm_daemon/Makefile
#

#include ../Makefile.cmd

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

#include ../Makefile.targ
