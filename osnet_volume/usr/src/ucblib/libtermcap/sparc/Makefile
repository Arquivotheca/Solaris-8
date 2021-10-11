#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	97/06/06 SMI"
#
# ucblib/libtermcap/sparc/Makefile

MAPDIR=	../spec/sparc
include ../Makefile.com

DYNFLAGS += $(DYNFLAGS32)
BUILD.SO= $(CC) -o $@ -G $(DYNFLAGS) $(PICS) -L $(ROOT)/usr/ucblib $(LDLIBS)

.KEEP_STATE:

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
