#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# cmd/sgs/libdl/ia64/Makefile

MAPDIR=		../spec/ia64

include		../Makefile.com

DYNFLAGS +=	-F /usr/lib/$(MACH64)/ld.so.1 $(MAPOPTS)

ROOTLINTLIB64=	$(LINTLIB:%=$(ROOTLIBDIR64)/%)

.KEEP_STATE:

all:		$(DYNLIB) $(LINTLIB)

lint:		$(LINTLIB)

include		../Makefile.targ
include		../../Makefile.sub.64


install:	all $(ROOTDYNLIB64) $(ROOTLINKS64) $(ROOTLINTLIB64)

$(DYNLIB):	$(MAPFILES)

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat
