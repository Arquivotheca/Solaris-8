#
#ident	"@(#)Makefile	1.4	92/07/14 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# lib/rtld.4.x/Makefile
#
# only installs an already-built a.out ld.so file

#include library definitions
include ../../../lib/Makefile.lib

FILE= ld.so
ROOTFILE= $(ROOTLIBDIR)/$(FILE)

FILEMODE= 755

$(ROOTLIBDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all:

install: $(ROOTFILE)

clean:

clobber: clean

lint:
