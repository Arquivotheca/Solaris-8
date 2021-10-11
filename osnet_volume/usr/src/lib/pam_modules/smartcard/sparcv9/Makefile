#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# ident "@(#)Makefile	1.4	98/08/10 SMI"
#
# lib/pam_modules/unix/sparcv9/Makefile

include ../Makefile.com
include ../../../Makefile.lib.64

OBJDIR=objs
PICDIR=pics

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

LIBS=$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
