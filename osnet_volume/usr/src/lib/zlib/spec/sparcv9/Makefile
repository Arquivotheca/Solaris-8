#ident	"@(#)Makefile	1.1	99/10/08 SMI"
#
# lib/zlib/spec/sparcv9/Makefile

.KEEP_STATE:

include	../Makefile.targ

# Add arch specific objects here
OBJECTS	+=

include	$(SRC)/lib/Makefile.lib
include	$(SRC)/lib/Makefile.lib.64

# Uncomment the following if the linker complains
#sparcv9_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec

install: $(ROOTABILIB64)
