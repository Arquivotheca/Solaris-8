#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.3 99/07/23 SMI"
#
# lib/ami/spec/sparcv9/Makefile
#

.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib
include	$(SRC)/lib/Makefile.lib.64

CPPFLAGS += -I$(SRC)/lib/ami/libami/include

# Uncomment the following if the linker complains
#sparcv9_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec

install: $(ROOTABIDIR64) $(ROOTABILIB64)

$(ROOTABIDIR64):
	$(INS.dir)
