#
#ident	"@(#)Makefile	1.20	99/02/16 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libdl/sparc/Makefile

MAPDIR=		../spec/sparc

4VERS=		.0

include		../Makefile.com

DYNFLAGS +=	-F /usr/lib/ld.so.1 $(MAPOPTS)
native:=	DYNFLAGS += -F /usr/lib/ld.so.1

# A symbolic link from /usr/4lib/libdl.so.1.0 is required for BCP.

4DYNLIB=	libdl.so.1$(4VERS)
ROOT4LIBDIR=	$(ROOT)/usr/4lib
ROOT4LINKS=	$(ROOT4LIBDIR)/$(4DYNLIB)
$(ROOT4LINKS):=	LIBLINKPATH = ../lib/
ROOTLINTLIB=    $(LINTLIB:%=$(ROOTLIBDIR)/%)

.KEEP_STATE:

include		../Makefile.targ

all:		$(DYNLIB) $(LINTLIB)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB) $(ROOTLINKS) $(ROOT4LINKS) \
		$(ROOTLINTLIB) $(ROOTLIBDIR)/$(LINTLIBSRC)

$(ROOT4LINKS):	$(ROOTDYNLIB)
		$(INS.liblink)
