#
#ident	"@(#)Makefile	1.2	99/05/14 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/librt/spec/sparcv9/Makefile

.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib
include	$(SRC)/lib/Makefile.lib.64

# Uncomment the following if the linker complains
#sparcv9_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec

install: $(ROOTABILIB64) $(ROOTABIDIR64)/$(ABIPOSIXLIB)

$(ROOTABIDIR64)/$(ABIPOSIXLIB): $(ABILIB)
	$(RM) $@; $(SYMLINK) $(ABILIB) $@
