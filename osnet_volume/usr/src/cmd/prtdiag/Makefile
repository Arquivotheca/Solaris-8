#
#ident	"@(#)Makefile	1.10	95/03/13 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# src/cmd/prtdiag/Makefile
#
# builds occur in kernel-architecture subdirectories using Makefile.com
# in this directory.
#

SUBDIRS=	sparc

# conditional assignments
all:=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg

.KEEP_STATE:

all install clean clobber lint _msg: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
