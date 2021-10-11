#
#pragma ident	"@(#)Makefile	1.2	99/07/20 SMI"
#
# Copyright (c) 1995,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# pam_modules/krb5/sparc/Makefile

#include ../../../Makefile.lib
#include ../Makefile.com

# library dependency
LDLIBS += -R $(KRUNPATH) $(KMECHLIB) $(KCLNTLIB) \
	-lmp -lc -ldl -lpam -lintl -lnsl -lxfn -lsocket -lresolv

OBJDIR=objs
PICDIR=pics

.KEEP_STATE:

DYNFLAGS += $(DYNFLAGS32)

LIBS = 	$(DYNLIB)

def_files:
	cp ../*.def .

all:

install:

lint:

# This directory will need to contain ia64 assembler code to be built
# This was not done, so this is a placeholder directory for future work.

#all: def_files $(OBJDIR) $(PICDIR) $(LIBS)
#install: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

