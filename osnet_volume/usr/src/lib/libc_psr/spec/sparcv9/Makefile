#
#pragma ident	"@(#)Makefile	1.1	99/01/25 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/spec/sparcv9/Makefile

SUBDIRS=	sun4u

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

# spec files do not have msgs or catalogs
_msg catalog:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
