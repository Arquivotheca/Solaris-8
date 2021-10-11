#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.7	99/02/19 SMI"
#
# cmd/sgs/libdl/sparcv9/Makefile

MAPDIR=		../spec/sparcv9

include		../Makefile.com

DYNFLAGS +=     -F /usr/lib/$(MACH64)/ld.so.1 $(MAPOPTS)
LINTFLAGS64 +=	-D__sparcv9

ROOTLINTLIB64=    $(LINTLIB:%=$(ROOTLIBDIR64)/%)

.KEEP_STATE:

all:		$(DYNLIB) $(LINTLIB)

include		../Makefile.targ
include		../../Makefile.sub.64


install:	all $(ROOTDYNLIB64) $(ROOTLINKS64) $(ROOTLINTLIB64)

$(DYNLIB):	$(MAPFILES)

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat
