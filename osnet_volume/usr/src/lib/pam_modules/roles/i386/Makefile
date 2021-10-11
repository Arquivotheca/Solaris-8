#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#"@(#)Makefile	1.2	99/09/20 SMI"
#
# pam_modules/roles/i386/Makefile

include ../Makefile.com

OBJDIR=objs
PICDIR=pics

.KEEP_STATE:

DYNFLAGS += $(DYNFLAGS32)

LIBS = 	$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)
