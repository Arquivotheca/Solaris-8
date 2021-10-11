#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# lib/krb5/ss/spec/ia64/Makefile

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib
include	$(SRC)/lib/Makefile.lib.64

# Uncomment the following if the linker complains
#ia64_C_PICFLAGS  = -K PIC
ia64_C_PICFLAGS += -I$(SRC)/lib/krb5 -I$(SRC)/lib/krb5/ss

include	$(SRC)/lib/Makefile.spec

install: $(ROOTABILIB64)
