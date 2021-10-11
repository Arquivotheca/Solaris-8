#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#"@(#)Makefile	1.2	99/09/16 SMI"
#
# lib/pam_modules/roles/sparcv9/Makefile

include ../Makefile.com
include ../../../Makefile.lib.64

OBJDIR=objs
PICDIR=pics

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

LIBS=$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
