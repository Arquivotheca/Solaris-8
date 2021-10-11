#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libnsl/spec/ia64/Makefile

.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib
include	$(SRC)/lib/Makefile.lib.64

# Uncomment the following if the linker complains
ia64_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec

install: $(ROOTABILIB64)
