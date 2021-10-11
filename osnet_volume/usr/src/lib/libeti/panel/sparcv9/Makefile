#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	99/01/25 SMI"
# All rights reserved.
#
# lib/libeti/panel/sparc/Makefile
#

MAPDIR=	../spec/sparcv9
include ../Makefile.com
include ../../../Makefile.lib.64

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

install: all $(ROOTLIBS64) $(ROOTLINKS64)

$(DYNLIB): $(MAPFILE)
