#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.2 99/07/23 SMI"
#
# lib/ami/libami/spec/sparc/Makefile
#
.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib

CPPFLAGS += -I$(SRC)/lib/ami/libami/include

# Uncomment the following if the linker complains
#sparc_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec

install: $(ROOTABIDIR) $(ROOTABILIB)

$(ROOTABIDIR):
	$(INS.dir)
