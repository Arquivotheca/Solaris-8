#
# Copyright (c) 1995,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# pam_modules/krb5/sparcv9/Makefile
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"

include ../Makefile.com
include ../../../Makefile.lib.64

LDLIBS += -R $(KRUNPATH64) $(KMECHLIB64) $(KCLNTLIB64) \
	-lmp -lc -ldl -lpam -lintl -lnsl -lxfn -lsocket -lresolv

OBJDIR=objs
PICDIR=pics

.KEEP_STATE:

DYNFLAGS += $(DYNFLAGS64)

LIBS = 	$(DYNLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBDIR64) $(ROOTLIBS64) $(ROOTLINKS64)

