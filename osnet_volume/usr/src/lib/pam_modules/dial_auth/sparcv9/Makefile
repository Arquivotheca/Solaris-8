#
#ident	"@(#)Makefile	1.2	98/06/11 SMI"
#
# Copyright (c) 1995,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# pam_modules/dial_auth/sparcv9/Makefile

include ../Makefile.com
include ../../../Makefile.lib.64

OBJDIR=objs
PICDIR=pics

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

LIBS = 	$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
