#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident   "@(#)Makefile 1.2     99/07/29 SMI"
#
# lib/gss_mechs/mech_dummy/sparcv9/do/Makefile

KSUBDIR=do
include ../../Makefile.com
include ../../../../Makefile.lib.64

LIBS=   $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBDIR64) $(ROOTLIBS64) $(ROOTLINKS64) $(GSSLIBDIR64) \
	$(GSSLINK64)
