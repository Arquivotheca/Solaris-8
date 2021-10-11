#
#ident	"@(#)Makefile	1.2	98/06/11 SMI"
#
# Copyright (c) 1995,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# pam_modules/rhosts_auth/sparc/Makefile

include ../Makefile.com

OBJDIR=objs
PICDIR=pics

.KEEP_STATE:

DYNFLAGS += $(DYNFLAGS32)

LIBS = 	$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)
