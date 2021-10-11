#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	99/09/21 SMI"
#
# ucblib/libtermcap/i386/Makefile

MAPDIR=	../spec/i386
include ../Makefile.com

DYNFLAGS += $(DYNFLAGS32)
BUILD.SO= $(CC) -o $@ -G $(DYNFLAGS) $(PICS) -L $(ROOT)/usr/ucblib $(LDLIBS)

.KEEP_STATE:

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
