#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# lib/krb5/ss/spec/sparc/Makefile

.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib

# Uncomment the following if the linker complains
#sparc_C_PICFLAGS  = -K PIC
sparc_C_PICFLAGS += -I$(SRC)/lib/krb5 -I$(SRC)/lib/krb5/ss

include	$(SRC)/lib/Makefile.spec

install: $(ROOTABILIB)
