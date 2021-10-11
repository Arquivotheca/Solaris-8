#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#

MAPDIR= ../spec/sparc

include ../Makefile.com

CPPFLAGS += -DSIZEOF_LONG=4 -DSIZEOF_INT=4

LDLIBS += -L $(ROOTLIBDIR)
LDLIBS += -R $(KRUNPATH) $(KMECHLIB) -lgss -lnsl -ldl -lsocket -lc -lxfn

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)
