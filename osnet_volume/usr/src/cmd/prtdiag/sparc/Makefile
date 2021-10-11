#
#ident	"@(#)Makefile 1.2	95/03/13 SMI"
#
# Copyright (c) 1993, 1995 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/prtdiag/sparc/Makefile
#
SUBDIRS = sun4d sun4u

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=		TARGET= _msg

.KEEP_STATE:

all install clean clobber lint _msg: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
