#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# Copyright (c) 1995,1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# pam_modules/sample/ia64/Makefile

include ../Makefile.com
include ../../../Makefile.lib.64

OBJDIR=objs
PICDIR=pics

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

LIBS = 	$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
