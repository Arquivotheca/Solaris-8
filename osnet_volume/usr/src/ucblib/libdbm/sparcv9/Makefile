#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	99/01/25 SMI"
#
# ucblib/libdbm/sparcv9/Makefile

MAPDIR=	../spec/sparcv9
include ../Makefile.com
include ../../../lib/Makefile.lib.64

BUILD.SO= $(CC) $(CFLAGS) -o $@ -G $(DYNFLAGS) $(PICS) \
	-L $(ROOT)/usr/ucblib/$(MACH64) -L $(ROOT)/usr/lib/$(MACH64) $(LDLIBS)

.KEEP_STATE:

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64)
