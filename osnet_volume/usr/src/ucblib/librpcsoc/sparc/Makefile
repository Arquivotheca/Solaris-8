#
# Copyright (c) 1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/09/20 SMI"
#
# lib/librpcsoc/sparc/Makefile

MAPDIR=	../spec/sparc
include ../Makefile.com

DYNFLAGS += $(DYNFLAGS32)
BUILD.SO= $(CC) -o $@ -G $(DYNFLAGS) $(PICS) -L $(ROOT)/usr/ucblib $(LDLIBS)

LIBS += $(DYNLIB) $(LINTLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
