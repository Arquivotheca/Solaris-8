#
#ident	"@(#)Makefile	1.1	98/06/15 SMI"
#
# Copyright (c) 1995,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# pam_modules/dial_auth/i386/Makefile

include ../Makefile.com

OBJDIR=objs
PICDIR=pics

DYNFLAGS += $(DYNFLAGS32)
LIBS =    $(DYNLIB)

.KEEP_STATE:

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)
