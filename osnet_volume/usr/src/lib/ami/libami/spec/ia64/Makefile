#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.1 99/07/11 SMI"
#
# lib/ami/spec/ia64/Makefile

.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib.64

CPPFLAGS += -I$(SRC)/lib/ami/libami/include

# Uncomment the following if the linker complains
#ia64_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec

debug install: $(ROOTABILIB64)
