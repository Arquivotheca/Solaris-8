#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#
#pragma ident "@(#)Makefile	1.1 97/10/27 SMI"
#
# lib/fn/context/x500/i386/Makefile
#

LDFLAGS += -L$(FNLIBDIR)
include ../Makefile.com

.KEEP_STATE:

debug install tcov: all $(ROOTLIBDIR) $(ROOTETC) $(ROOTETCFN) \
			$(ROOTLIBS) $(ROOTETCFNFILES)
