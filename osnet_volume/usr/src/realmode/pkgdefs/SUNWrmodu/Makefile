#
#ident	"@(#)Makefile	1.2	99/10/11 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
#

include ../Makefile.com

DATAFILES +=

.KEEP_STATE:

all: $(FILES)
install: all $(MACH)_pkg

include ../Makefile.targ

sparc_pkg: FRC
	[ -d $(PKGARCHIVE)/sparc_rm ] || mkdir -p $(PKGARCHIVE)/sparc_rm
	pkgmk -a sparc -f prototype_sparc -d $(PKGARCHIVE)/sparc_rm -r $(ROOT) -o $(PACKAGE)

i386_pkg: pkg sparc_pkg
	
	
