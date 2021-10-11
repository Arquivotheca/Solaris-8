#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# lib/gss_mechs/mech_krb5/ia64/Makefile

KSUBDIR=gl
include ../../Makefile.com
include ../../../../Makefile.lib.64

CPPFLAGS += -DKRB5_NO_PRIVACY

LIBS += $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBDIR) $(ROOTLIBS64) $(ROOTLINKS64)

