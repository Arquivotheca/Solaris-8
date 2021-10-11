#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# lib/gss_mechs/mech_krb5/do/Makefile

KSUBDIR=gl
include ../Makefile.com

CPPFLAGS += -DKRB5_NO_PRIVACY

LIBS += $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)
