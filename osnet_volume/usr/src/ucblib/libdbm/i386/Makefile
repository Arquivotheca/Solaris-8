#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.5	99/01/25 SMI"
#
# ucblib/libdbm/i386/Makefile

MAPDIR=	../spec/i386
include ../Makefile.com

BUILD.SO= $(CC) -o $@ -G $(DYNFLAGS) $(PICS) -L $(ROOT)/usr/ucblib $(LDLIBS)

.KEEP_STATE:

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
