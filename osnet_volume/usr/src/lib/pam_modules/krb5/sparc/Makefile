#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# Copyright (c) 1995,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# pam_modules/krb5/sparc/Makefile

include ../../../Makefile.lib
include ../Makefile.com

# library dependency
LDLIBS += -R $(KRUNPATH) $(KMECHLIB) $(KCLNTLIB) \
	-lmp -lc -ldl -lpam -lintl -lnsl -lxfn -lsocket -lresolv

OBJDIR=objs
PICDIR=pics

.KEEP_STATE:

DYNFLAGS += $(DYNFLAGS32)

LIBS = 	$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

