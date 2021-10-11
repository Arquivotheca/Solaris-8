#
#ident	"@(#)Makefile	1.13	99/03/04 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libdl/i386/Makefile

MAPDIR=		../spec/i386

include		../Makefile.com

DYNFLAGS +=	-F /usr/lib/ld.so.1 $(MAPOPTS)
native:=	DYNFLAGS += -F /usr/lib/ld.so.1

.KEEP_STATE:

include		../Makefile.targ


all:		$(DYNLIB) $(LINTLIB)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB) $(ROOTLINKS) \
		$(ROOTLINTLIB) $(ROOTLIBDIR)/$(LINTLIBSRC)

