#
#pragma ident	"@(#)Makefile	1.1	99/07/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5_psr/spec/sparc/Makefile

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
