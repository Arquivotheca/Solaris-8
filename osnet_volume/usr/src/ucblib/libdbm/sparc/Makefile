#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	99/01/25 SMI"
#
# ucblib/libdbm/sparc/Makefile

MAPDIR=	../spec/sparc
include ../Makefile.com

BUILD.SO= $(CC) -o $@ -G $(DYNFLAGS) $(PICS) -L $(ROOT)/usr/ucblib $(LDLIBS)

.KEEP_STATE:

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
