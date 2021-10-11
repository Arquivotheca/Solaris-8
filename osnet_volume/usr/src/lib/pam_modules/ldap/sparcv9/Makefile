#
# ident	"@(#)Makefile	1.1	99/07/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/pam_modules/ldap/sparcv9/Makefile
#

include ../Makefile.com
include ../../../Makefile.lib.64

OBJDIR=objs
PICDIR=pics

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

LIBS = 	$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
