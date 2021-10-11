#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.2 99/07/23 SMI"
#
# lib/ami/spec/i386/Makefile

.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib

CPPFLAGS += -I$(SRC)/lib/ami/libami/include

# Uncomment the following if the linker complains
#i386_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec

debug install: $(ROOTABIDIR) $(ROOTABILIB)

$(ROOTABIDIR):
	$(INS.dir)
